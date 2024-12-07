// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/interpreter.h>

#include <crypto/ripemd160.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/bitfield.h>
#include <script/script.h>
#include <script/script_flags.h>
#include <script/sigencoding.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/bitmanip.h>

bool CastToBool(const valtype &vch) {
    for (size_t i = 0; i < vch.size(); i++) {
        if (vch[i] != 0) {
            // Can be negative zero
            if (i == vch.size() - 1 && vch[i] == 0x80) {
                return false;
            }
            return true;
        }
    }
    return false;
}

/**
 * Script is a stack machine (like Forth) that evaluates a predicate
 * returning a bool indicating valid or not.  There are no loops.
 */
#define stacktop(i) (stack.at(stack.size() + (i)))
#define altstacktop(i) (altstack.at(altstack.size() + (i)))
static inline void popstack(std::vector<valtype> &stack) {
    if (stack.empty()) {
        throw std::runtime_error("popstack(): stack empty");
    }
    stack.pop_back();
}

int FindAndDelete(CScript &script, const CScript &b) {
    int nFound = 0;
    if (b.empty()) {
        return nFound;
    }

    CScript result;
    CScript::const_iterator pc = script.begin(), pc2 = script.begin(),
                            end = script.end();
    opcodetype opcode;
    do {
        result.insert(result.end(), pc2, pc);
        while (static_cast<size_t>(end - pc) >= b.size() &&
               std::equal(b.begin(), b.end(), pc)) {
            pc = pc + b.size();
            ++nFound;
        }
        pc2 = pc;
    } while (script.GetOp(pc, opcode));

    if (nFound > 0) {
        result.insert(result.end(), pc2, end);
        script = std::move(result);
    }

    return nFound;
}

static void CleanupScriptCode(CScript &scriptCode,
                              const std::vector<uint8_t> &vchSig,
                              uint32_t flags) {
    // Drop the signature in scripts when SIGHASH_FORKID is not used.
    SigHashType sigHashType = GetHashType(vchSig);
    if (!(flags & SCRIPT_ENABLE_SIGHASH_FORKID) || !sigHashType.hasFork()) {
        FindAndDelete(scriptCode, CScript(vchSig));
    }
}

static bool IsOpcodeDisabled(opcodetype opcode, uint32_t flags) {
    switch (opcode) {
        case OP_INVERT:
        case OP_2MUL:
        case OP_2DIV:
        case OP_LSHIFT:
        case OP_RSHIFT:
            // Disabled opcodes.
            return true;
        case OP_MUL:
            return (flags & SCRIPT_64_BIT_INTEGERS) == 0;
        default:
            break;
    }
    return false;
}

namespace {
/**
 * A data type to abstract out the condition stack during script execution.
 *
 * Conceptually it acts like a vector of booleans, one for each level of nested
 * IF/THEN/ELSE, indicating whether we're in the active or inactive branch of
 * each.
 *
 * The elements on the stack cannot be observed individually; we only need to
 * expose whether the stack is empty and whether or not any false values are
 * present at all. To implement OP_ELSE, a toggle_top modifier is added, which
 * flips the last value without returning it.
 *
 * This uses an optimized implementation that does not materialize the
 * actual stack. Instead, it just stores the size of the would-be stack,
 * and the position of the first false value in it.
 */
class ConditionStack {
private:
    //! A constant for m_first_false_pos to indicate there are no falses.
    static constexpr uint32_t NO_FALSE = std::numeric_limits<uint32_t>::max();

    //! The size of the implied stack.
    uint32_t m_stack_size = 0;
    //! The position of the first false value on the implied stack, or NO_FALSE
    //! if all true.
    uint32_t m_first_false_pos = NO_FALSE;

public:
    [[nodiscard]] constexpr bool empty() const noexcept { return m_stack_size == 0; }
    [[nodiscard]] constexpr bool all_true() const noexcept { return m_first_false_pos == NO_FALSE; }
    constexpr void push_back(bool f) noexcept {
        if (m_first_false_pos == NO_FALSE && !f) {
            // The stack consists of all true values, and a false is added.
            // The first false value will appear at the current size.
            m_first_false_pos = m_stack_size;
        }
        ++m_stack_size;
    }
    constexpr void pop_back() noexcept {
        --m_stack_size;
        if (m_first_false_pos == m_stack_size) {
            // When popping off the first false value, everything becomes true.
            m_first_false_pos = NO_FALSE;
        }
    }
    constexpr void toggle_top() noexcept {
        if (m_first_false_pos == NO_FALSE) {
            // The current stack is all true values; the first false will be the
            // top.
            m_first_false_pos = m_stack_size - 1;
        } else if (m_first_false_pos == m_stack_size - 1) {
            // The top is the first false value; toggling it will make
            // everything true.
            m_first_false_pos = NO_FALSE;
        } else {
            // There is a false value, but not on top. No action is needed as
            // toggling anything but the first false value is unobservable.
        }
    }

    constexpr uint32_t size() const noexcept { return m_stack_size; }
};

template<bool UsesBigInt>
bool EvalScriptImpl(std::vector<valtype> &stack, const CScript &script, uint32_t flags,
                    const BaseSignatureChecker &checker, ScriptExecutionMetrics &metrics, ScriptError *serror) {
    // UsesBigInt template arg must match flags
    assert(UsesBigInt == bool(flags & SCRIPT_ENABLE_MAY2025));
    using ScriptNumType = std::conditional_t<UsesBigInt, ScriptBigInt, CScriptNum>;

    static auto const bnZero = ScriptNumType::fromIntUnchecked(0);
    static const valtype vchFalse(0);
    static const valtype vchTrue(1, 1);

    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    ConditionStack vfExec;
    std::vector<valtype> altstack;
    set_error(serror, ScriptError::UNKNOWN);
    if (script.size() > MAX_SCRIPT_SIZE) {
        return set_error(serror, ScriptError::SCRIPT_SIZE);
    }
    int nOpCount = 0; /* Only used iff chipVmLimitsEnabled == false */
    bool const fRequireMinimal = (flags & SCRIPT_VERIFY_MINIMALDATA) != 0;
    bool const nativeIntrospection = (flags & SCRIPT_NATIVE_INTROSPECTION) != 0;
    bool const integers64Bit = (flags & SCRIPT_64_BIT_INTEGERS) != 0;
    bool const nativeTokens = (flags & SCRIPT_ENABLE_TOKENS) != 0;
    const ScriptExecutionContext * const context = checker.GetContext();

    size_t const maxIntegerSizeLegacy =
        integers64Bit ? CScriptNum::MAXIMUM_ELEMENT_SIZE_64_BIT
                      : CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;

    size_t const maxIntegerSize = UsesBigInt ? ScriptBigInt::MAXIMUM_ELEMENT_SIZE_BIG_INT : maxIntegerSizeLegacy;

    ScriptError const invalidNumberRangeErrorLegacy =
        integers64Bit ? ScriptError::INVALID_NUMBER_RANGE_64_BIT
                      : ScriptError::INVALID_NUMBER_RANGE;

    ScriptError const invalidNumberRangeError = UsesBigInt ? ScriptError::INVALID_NUMBER_RANGE_BIG_INT
                                                           : invalidNumberRangeErrorLegacy;

    bool const chipVmLimitsEnabled = (flags & SCRIPT_ENABLE_MAY2025) != 0;
    size_t const maxScriptElementSize = chipVmLimitsEnabled ? may2025::MAX_SCRIPT_ELEMENT_SIZE
                                                            : MAX_SCRIPT_ELEMENT_SIZE_LEGACY;

    if (chipVmLimitsEnabled && !metrics.HasValidScriptLimits() && context) {
        // Calculate metrics "scriptLimits", if not already passed-in, and if we have a `context` object
        // from which to get the scriptSig size.
        metrics.SetScriptLimits(flags, context->scriptSig().size());
    }

    try {
        while (pc < pend) {
            bool fExec = vfExec.all_true();

            //
            // Read instruction
            //
            valtype vchPushValue;
            if (!script.GetOp(pc, opcode, vchPushValue)) {
                return set_error(serror, ScriptError::BAD_OPCODE);
            }
            if (vchPushValue.size() > maxScriptElementSize) {
                return set_error(serror, ScriptError::PUSH_SIZE);
            }

            // Op-code cost accounting
            // May 2025 upgrade: Cost of 100 per instruction executed; this is measured unconditionally but the limit
            // for this metric is only enforced after the May 2025 upgrade.
            metrics.TallyOp(may2025::OPCODE_COST);
            // Pre May 2025 upgrade: increment the `nOpCount` variable
            if ( ! chipVmLimitsEnabled) {
                // Note how OP_RESERVED does not count towards the opcode limit.
                if (opcode > OP_16 && ++nOpCount > MAX_OPS_PER_SCRIPT_LEGACY) {
                    return set_error(serror, ScriptError::OP_COUNT);
                }
            }

            // Some opcodes are disabled.
            if (IsOpcodeDisabled(opcode, flags)) {
                return set_error(serror, ScriptError::DISABLED_OPCODE);
            }

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4) {
                if (fRequireMinimal &&
                    !CheckMinimalPush(vchPushValue, opcode)) {
                    return set_error(serror, ScriptError::MINIMALDATA);
                }
                stack.push_back(std::move(vchPushValue));
                metrics.TallyPushOp(stack.back().size());
            } else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF)) {
                switch (opcode) {
                    //
                    // Push value
                    //
                    case OP_1NEGATE:
                    case OP_1:
                    case OP_2:
                    case OP_3:
                    case OP_4:
                    case OP_5:
                    case OP_6:
                    case OP_7:
                    case OP_8:
                    case OP_9:
                    case OP_10:
                    case OP_11:
                    case OP_12:
                    case OP_13:
                    case OP_14:
                    case OP_15:
                    case OP_16: {
                        // ( -- value)
                        auto const bn = CScriptNum::fromIntUnchecked(int(opcode) - int(OP_1 - 1));
                        stack.push_back(bn.getvch());
                        metrics.TallyPushOp(stack.back().size());
                        // The result of these opcodes should always be the
                        // minimal way to push the data they push, so no need
                        // for a CheckMinimalPush here.
                    } break;

                    //
                    // Control
                    //
                    case OP_NOP:
                        break;

                    case OP_CHECKLOCKTIMEVERIFY: {
                        if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {
                            break;
                        }

                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // Note that elsewhere numeric opcodes are limited to
                        // operands in the range -2**31+1 to 2**31-1, however it
                        // is legal for opcodes to produce results exceeding
                        // that range. This limitation is implemented by
                        // CScriptNum's default 4-byte limit.
                        //
                        // If we kept to that limit we'd have a year 2038
                        // problem, even though the nLockTime field in
                        // transactions themselves is uint32 which only becomes
                        // meaningless after the year 2106.
                        //
                        // Thus as a special case we tell CScriptNum to accept
                        // up to 5-byte bignums, which are good until 2**39-1,
                        // well beyond the 2**32-1 limit of the nLockTime field
                        // itself.
                        const CScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);

                        // In the rare event that the argument may be < 0 due to
                        // some arithmetic being done first, you can always use
                        // 0 MAX CHECKLOCKTIMEVERIFY.
                        if (nLockTime < 0) {
                            return set_error(serror, ScriptError::NEGATIVE_LOCKTIME);
                        }

                        // Actually compare the specified lock time with the
                        // transaction.
                        if (!checker.CheckLockTime(nLockTime)) {
                            return set_error(serror,
                                             ScriptError::UNSATISFIED_LOCKTIME);
                        }

                        break;
                    }

                    case OP_CHECKSEQUENCEVERIFY: {
                        if (!(flags & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY)) {
                            break;
                        }

                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // nSequence, like nLockTime, is a 32-bit unsigned
                        // integer field. See the comment in CHECKLOCKTIMEVERIFY
                        // regarding 5-byte numeric operands.
                        const CScriptNum nSequence(stacktop(-1), fRequireMinimal, 5);

                        // In the rare event that the argument may be < 0 due to
                        // some arithmetic being done first, you can always use
                        // 0 MAX CHECKSEQUENCEVERIFY.
                        if (nSequence < 0) {
                            return set_error(serror, ScriptError::NEGATIVE_LOCKTIME);
                        }

                        // To provide for future soft-fork extensibility, if the
                        // operand has the disabled lock-time flag set,
                        // CHECKSEQUENCEVERIFY behaves as a NOP.
                        auto res = nSequence.safeBitwiseAnd(CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG);
                        if ( ! res) {
                            // Defensive programming: It is impossible for the following error to be
                            // returned unless the current possible values of the operands change.
                            return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                        }
                        if (*res != 0) {
                            break;
                        }

                        // Compare the specified sequence number with the input.
                        if (!checker.CheckSequence(nSequence)) {
                            return set_error(serror, ScriptError::UNSATISFIED_LOCKTIME);
                        }
                        break;
                    }

                    case OP_NOP1:
                    case OP_NOP4:
                    case OP_NOP5:
                    case OP_NOP6:
                    case OP_NOP7:
                    case OP_NOP8:
                    case OP_NOP9:
                    case OP_NOP10: {
                        if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                            return set_error(
                                serror,
                                ScriptError::DISCOURAGE_UPGRADABLE_NOPS);
                        }
                    } break;

                    case OP_IF:
                    case OP_NOTIF: {
                        // <expression> if [statements] [else [statements]]
                        // endif
                        bool fValue = false;
                        if (fExec) {
                            if (stack.size() < 1) {
                                return set_error(
                                    serror,
                                    ScriptError::UNBALANCED_CONDITIONAL);
                            }
                            const valtype &vch = stacktop(-1);
                            if (flags & SCRIPT_VERIFY_MINIMALIF) {
                                if (vch.size() > 1) {
                                    return set_error(serror,
                                                     ScriptError::MINIMALIF);
                                }
                                if (vch.size() == 1 && vch[0] != 1) {
                                    return set_error(serror,
                                                     ScriptError::MINIMALIF);
                                }
                            }
                            fValue = CastToBool(vch);
                            if (opcode == OP_NOTIF) {
                                fValue = !fValue;
                            }
                            popstack(stack);
                        }
                        vfExec.push_back(fValue);
                    } break;

                    case OP_ELSE: {
                        if (vfExec.empty()) {
                            return set_error(
                                serror, ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.toggle_top();
                    } break;

                    case OP_ENDIF: {
                        if (vfExec.empty()) {
                            return set_error(
                                serror, ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.pop_back();
                    } break;

                    case OP_VERIFY: {
                        // (true -- ) or
                        // (false -- false) and return
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        bool fValue = CastToBool(stacktop(-1));
                        if (fValue) {
                            popstack(stack);
                        } else {
                            return set_error(serror, ScriptError::VERIFY);
                        }
                    } break;

                    case OP_RETURN: {
                        return set_error(serror, ScriptError::OP_RETURN);
                    } break;

                    //
                    // Stack ops
                    //
                    case OP_TOALTSTACK: {
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        altstack.push_back(std::move(stacktop(-1)));
                        popstack(stack);
                        // Intentional: no tallying is done to metrics.TallyPushOp()
                    } break;

                    case OP_FROMALTSTACK: {
                        if (altstack.size() < 1) {
                            return set_error(
                                serror,
                                ScriptError::INVALID_ALTSTACK_OPERATION);
                        }
                        stack.push_back(std::move(altstacktop(-1)));
                        metrics.TallyPushOp(stack.back().size());
                        popstack(altstack);
                    } break;

                    case OP_2DROP: {
                        // (x1 x2 -- )
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                        popstack(stack);
                    } break;

                    case OP_2DUP: {
                        // (x1 x2 -- x1 x2 x1 x2)
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-2);
                        valtype vch2 = stacktop(-1);
                        stack.push_back(std::move(vch1));
                        metrics.TallyPushOp(stack.back().size());
                        stack.push_back(std::move(vch2));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_3DUP: {
                        // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
                        if (stack.size() < 3) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-3);
                        valtype vch2 = stacktop(-2);
                        valtype vch3 = stacktop(-1);
                        stack.push_back(std::move(vch1));
                        metrics.TallyPushOp(stack.back().size());
                        stack.push_back(std::move(vch2));
                        metrics.TallyPushOp(stack.back().size());
                        stack.push_back(std::move(vch3));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_2OVER: {
                        // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
                        if (stack.size() < 4) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-4);
                        valtype vch2 = stacktop(-3);
                        stack.push_back(std::move(vch1));
                        metrics.TallyPushOp(stack.back().size());
                        stack.push_back(std::move(vch2));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_2ROT: {
                        // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
                        if (stack.size() < 6) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-6);
                        valtype vch2 = stacktop(-5);
                        stack.erase(stack.end() - 6, stack.end() - 4);
                        stack.push_back(std::move(vch1));
                        metrics.TallyPushOp(stack.back().size());
                        stack.push_back(std::move(vch2));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_2SWAP: {
                        // (x1 x2 x3 x4 -- x3 x4 x1 x2)
                        if (stack.size() < 4) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-4), stacktop(-2));
                        swap(stacktop(-3), stacktop(-1));
                        // Intentional: no tallying is done to metrics.TallyPushOp()
                    } break;

                    case OP_IFDUP: {
                        // (x - 0 | x x)
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        if (CastToBool(vch)) {
                            stack.push_back(std::move(vch));
                            metrics.TallyPushOp(stack.back().size());
                        }
                    } break;

                    case OP_DEPTH: {
                        // -- stacksize
                        auto const bn = CScriptNum::fromIntUnchecked(stack.size());
                        stack.push_back(bn.getvch());
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_DROP: {
                        // (x -- )
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                    } break;

                    case OP_DUP: {
                        // (x -- x x)
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        stack.push_back(std::move(vch));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_NIP: {
                        // (x1 x2 -- x2)
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        stack.erase(stack.end() - 2);
                    } break;

                    case OP_OVER: {
                        // (x1 x2 -- x1 x2 x1)
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-2);
                        stack.push_back(std::move(vch));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_PICK:
                    case OP_ROLL: {
                        // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
                        // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const n = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        popstack(stack);
                        if (n < 0 || uint64_t(n) >= stack.size()) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype vch;
                        if (auto it = stack.end() - n - 1; opcode == OP_ROLL) {
                            // We use std::move to avoid excess copying in the OP_ROLL case.
                            vch = std::move(*it);
                            stack.erase(it); // `it` is invalidated here
                            metrics.TallyOp(n); // erasing in the middle is linear with `n`
                        } else {
                            // The OP_PICK case must do a copy, but at least we save on not having to erase in the
                            // middle and thus we don't have to slide everything over by 1 (hence extraCost = 0).
                            vch = *it;
                        }
                        stack.push_back(std::move(vch)); // move-construct to save on copying
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_ROT: {
                        // (x1 x2 x3 -- x2 x3 x1)
                        //  x2 x1 x3  after first swap
                        //  x2 x3 x1  after second swap
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-3), stacktop(-2));
                        swap(stacktop(-2), stacktop(-1));
                        // Intentional: no tallying is done to metrics.TallyPushOp()
                    } break;

                    case OP_SWAP: {
                        // (x1 x2 -- x2 x1)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-2), stacktop(-1));
                        // Intentional: no tallying is done to metrics.TallyPushOp()
                    } break;

                    case OP_TUCK: {
                        // (x1 x2 -- x2 x1 x2)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        metrics.TallyPushOp(vch.size());
                        stack.insert(stack.end() - 2, std::move(vch));
                    } break;

                    case OP_SIZE: {
                        // (in -- in size)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        auto const bn = CScriptNum::fromIntUnchecked(stacktop(-1).size());
                        stack.push_back(bn.getvch());
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    //
                    // Bitwise logic
                    //
                    case OP_AND:
                    case OP_OR:
                    case OP_XOR: {
                        // (x1 x2 - out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch1 = stacktop(-2);
                        valtype &vch2 = stacktop(-1);

                        // Inputs must be the same size
                        if (vch1.size() != vch2.size()) {
                            return set_error(serror, ScriptError::INVALID_OPERAND_SIZE);
                        }

                        // To avoid allocating, we modify vch1 in place.
                        switch (opcode) {
                            case OP_AND:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] &= vch2[i];
                                }
                                break;
                            case OP_OR:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] |= vch2[i];
                                }
                                break;
                            case OP_XOR:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] ^= vch2[i];
                                }
                                break;
                            default:
                                break;
                        }

                        // May 2025 Upgrade: tally this as the length of the result (vch1)
                        metrics.TallyOp(vch1.size());

                        // And pop vch2.
                        popstack(stack);
                    } break;

                    case OP_EQUAL:
                    case OP_EQUALVERIFY:
                        // case OP_NOTEQUAL: // use OP_NUMNOTEQUAL
                        {
                            // (x1 x2 - bool)
                            if (stack.size() < 2) {
                                return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                            }

                            const bool fEqual = stacktop(-2) == stacktop(-1);
                            // OP_NOTEQUAL is disabled because it would be too
                            // easy to say something like n != 1 and have some
                            // wiseguy pass in 1 with extra zero bytes after it
                            // (numerically, 0x01 == 0x0001 == 0x000001)
                            // if (opcode == OP_NOTEQUAL)
                            //    fEqual = !fEqual;
                            popstack(stack);
                            popstack(stack);
                            stack.push_back(fEqual ? vchTrue : vchFalse);
                            metrics.TallyPushOp(stack.back().size());
                            if (opcode == OP_EQUALVERIFY) {
                                if (fEqual) {
                                    popstack(stack);
                                } else {
                                    return set_error(serror, ScriptError::EQUALVERIFY);
                                }
                            }
                        }
                        break;

                    //
                    // Numeric
                    //
                    case OP_1ADD:
                    case OP_1SUB:
                    case OP_NEGATE:
                    case OP_ABS:
                    case OP_NOT:
                    case OP_0NOTEQUAL: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNumType bn(stacktop(-1), fRequireMinimal, maxIntegerSize);
                        uint32_t pushCostFactor = 2u; // all except OP_NOT and OP_0NOTEQUAL will be costed 2x

                        switch (opcode) {
                            case OP_1ADD: {
                                auto res = bn.safeAdd(1);
                                if ( ! res) {
                                    return set_error(serror, invalidNumberRangeError);
                                }
                                bn = std::move(*res);
                                break;
                            }
                            case OP_1SUB: {
                                auto res = bn.safeSub(1);
                                if ( ! res) {
                                    return set_error(serror, invalidNumberRangeError);
                                }
                                bn = std::move(*res);
                                break;
                            }
                            case OP_NEGATE:
                                bn = -bn;
                                break;
                            case OP_ABS:
                                if (bn < bnZero) {
                                    bn = -bn;
                                }
                                break;
                            case OP_NOT:
                                bn = ScriptNumType::fromIntUnchecked(bn == bnZero);
                                pushCostFactor = 1u; // as per spec, this op gets costed 1x
                                break;
                            case OP_0NOTEQUAL:
                                bn = ScriptNumType::fromIntUnchecked(bn != bnZero);
                                pushCostFactor = 1u; // as per spec, this op gets costed 1x
                                break;
                            default:
                                assert(!"invalid opcode");
                                break;
                        }
                        popstack(stack);
                        auto vch = bn.getvch();
                        // belt-and-suspenders check (BigInt case only)
                        if (UsesBigInt && vch.size() > maxScriptElementSize) {
                            return set_error(serror, invalidNumberRangeError);
                        }
                        stack.push_back(std::move(vch));
                        metrics.TallyPushOp(stack.back().size() * pushCostFactor);
                    } break;

                    case OP_ADD:
                    case OP_SUB:
                    case OP_MUL:
                    case OP_DIV:
                    case OP_MOD:
                    case OP_BOOLAND:
                    case OP_BOOLOR:
                    case OP_NUMEQUAL:
                    case OP_NUMEQUALVERIFY:
                    case OP_NUMNOTEQUAL:
                    case OP_LESSTHAN:
                    case OP_GREATERTHAN:
                    case OP_LESSTHANOREQUAL:
                    case OP_GREATERTHANOREQUAL:
                    case OP_MIN:
                    case OP_MAX: {
                        // (x1 x2 -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        const valtype &vch1 = stacktop(-2);
                        const valtype &vch2 = stacktop(-1);
                        ScriptNumType const bn1(vch1, fRequireMinimal, maxIntegerSize);
                        ScriptNumType const bn2(vch2, fRequireMinimal, maxIntegerSize);
                        auto bn = ScriptNumType::fromIntUnchecked(0);
                        uint32_t quadraticOpCost = 0u; // for OP_MUL, OP_DIV, and OP_MOD
                        uint32_t pushCostFactor = 1u; // arithmetic and min/max ops below set this to 2x
                        constexpr uint64_t worstCaseSize = std::max(MAX_SCRIPT_ELEMENT_SIZE_LEGACY, may2025::MAX_SCRIPT_ELEMENT_SIZE);
                        static_assert(worstCaseSize * worstCaseSize <= std::numeric_limits<uint32_t>::max(),
                                      "Assumption is that the largest theoretical op cost fits in a 32-bit unsigned int.");

                        switch (opcode) {
                            case OP_ADD: {
                                auto res = bn1.safeAdd(bn2);
                                if ( ! res) {
                                    return set_error(serror, invalidNumberRangeError);
                                }
                                bn = std::move(*res);
                                pushCostFactor = 2u;
                                break;
                            }

                            case OP_SUB: {
                                auto res = bn1.safeSub(bn2);
                                if ( ! res) {
                                    return set_error(serror, invalidNumberRangeError);
                                }
                                bn = std::move(*res);
                                pushCostFactor = 2u;
                                break;
                            }

                            case OP_MUL: {
                                auto res = bn1.safeMul(bn2);
                                if ( ! res) {
                                    return set_error(serror, invalidNumberRangeError);
                                }
                                bn = std::move(*res);
                                quadraticOpCost = vch1.size() * vch2.size();
                                pushCostFactor = 2u;
                                break;
                            }

                            case OP_DIV:
                                // denominator must not be 0
                                if (bn2 == 0) {
                                    return set_error(serror, ScriptError::DIV_BY_ZERO);
                                }
                                bn = bn1 / bn2;
                                quadraticOpCost = vch1.size() * vch2.size();
                                pushCostFactor = 2u;
                                break;

                            case OP_MOD:
                                // divisor must not be 0
                                if (bn2 == 0) {
                                    return set_error(serror, ScriptError::MOD_BY_ZERO);
                                }
                                bn = bn1 % bn2;
                                quadraticOpCost = vch1.size() * vch2.size();
                                pushCostFactor = 2u;
                                break;

                            case OP_BOOLAND:
                                bn = ScriptNumType::fromIntUnchecked(bn1 != bnZero && bn2 != bnZero);
                                break;
                            case OP_BOOLOR:
                                bn = ScriptNumType::fromIntUnchecked(bn1 != bnZero || bn2 != bnZero);
                                break;
                            case OP_NUMEQUAL:
                                bn = ScriptNumType::fromIntUnchecked(bn1 == bn2);
                                break;
                            case OP_NUMEQUALVERIFY:
                                bn = ScriptNumType::fromIntUnchecked(bn1 == bn2);
                                break;
                            case OP_NUMNOTEQUAL:
                                bn = ScriptNumType::fromIntUnchecked(bn1 != bn2);
                                break;
                            case OP_LESSTHAN:
                                bn = ScriptNumType::fromIntUnchecked(bn1 < bn2);
                                break;
                            case OP_GREATERTHAN:
                                bn = ScriptNumType::fromIntUnchecked(bn1 > bn2);
                                break;
                            case OP_LESSTHANOREQUAL:
                                bn = ScriptNumType::fromIntUnchecked(bn1 <= bn2);
                                break;
                            case OP_GREATERTHANOREQUAL:
                                bn = ScriptNumType::fromIntUnchecked(bn1 >= bn2);
                                break;
                            case OP_MIN:
                                bn = (bn1 < bn2 ? bn1 : bn2);
                                pushCostFactor = 2u;
                                break;
                            case OP_MAX:
                                bn = (bn1 > bn2 ? bn1 : bn2);
                                pushCostFactor = 2u;
                                break;
                            default:
                                assert(!"invalid opcode");
                                break;
                        }

                        metrics.TallyOp(quadraticOpCost); // is 0 for most opcodes except: MUL, MOD, DIV

                        popstack(stack); // invalidates: vch1 and vch2
                        popstack(stack);
                        {
                            auto vch = bn.getvch();
                            // Belt-and-suspenders check that we aren't overflowing the push limit in the BigInt case
                            if (UsesBigInt && vch.size() > maxScriptElementSize) {
                                return set_error(serror, invalidNumberRangeError);
                            }
                            stack.push_back(std::move(vch));
                        }
                        metrics.TallyPushOp(stack.back().size() * pushCostFactor);

                        if (opcode == OP_NUMEQUALVERIFY) {
                            if (CastToBool(stacktop(-1))) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::NUMEQUALVERIFY);
                            }
                        }
                    } break;

                    case OP_WITHIN: {
                        // (x min max -- out)
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        ScriptNumType const bn1(stacktop(-3), fRequireMinimal, maxIntegerSize);
                        ScriptNumType const bn2(stacktop(-2), fRequireMinimal, maxIntegerSize);
                        ScriptNumType const bn3(stacktop(-1), fRequireMinimal, maxIntegerSize);

                        bool fValue = (bn2 <= bn1 && bn1 < bn3);
                        popstack(stack);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fValue ? vchTrue : vchFalse);
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    //
                    // Crypto
                    //
                    case OP_RIPEMD160:
                    case OP_SHA1:
                    case OP_SHA256:
                    case OP_HASH160:
                    case OP_HASH256: {
                        // (in -- hash)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        const valtype &vch = stacktop(-1);
                        valtype vchHash((opcode == OP_RIPEMD160 ||
                                         opcode == OP_SHA1 ||
                                         opcode == OP_HASH160)
                                            ? 20
                                            : 32);
                        bool isTwoRoundHashOp = false;
                        if (opcode == OP_RIPEMD160) {
                            CRIPEMD160()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_SHA1) {
                            CSHA1()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_SHA256) {
                            CSHA256()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_HASH160) {
                            CHash160().Write(vch).Finalize(vchHash);
                            isTwoRoundHashOp = true;
                        } else if (opcode == OP_HASH256) {
                            CHash256().Write(vch).Finalize(vchHash);
                            isTwoRoundHashOp = true;
                        }
                        metrics.TallyHashOp(vch.size(), isTwoRoundHashOp);
                        popstack(stack);
                        stack.push_back(std::move(vchHash));
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_CODESEPARATOR: {
                        // Hash starts after the code separator
                        pbegincodehash = pc;
                    } break;

                    case OP_CHECKSIG:
                    case OP_CHECKSIGVERIFY: {
                        // (sig pubkey -- bool)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype const &vchSig = stacktop(-2);
                        valtype const &vchPubKey = stacktop(-1);

                        if (!CheckTransactionSignatureEncoding(vchSig, flags,
                                                               serror) ||
                            !CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                            // serror is set
                            return false;
                        }

                        bool fSuccess = false;
                        if (vchSig.size()) {
                            // Subset of script starting at the most recent
                            // codeseparator
                            CScript scriptCode(pbegincodehash, pend);

                            // Remove signature for pre-fork scripts
                            CleanupScriptCode(scriptCode, vchSig, flags);

                            size_t bytesHashed{};
                            fSuccess = checker.CheckSig(vchSig, vchPubKey, scriptCode, flags, &bytesHashed);
                            metrics.TallySigChecks(1);
                            if (bytesHashed) {
                                metrics.TallyHashOp(bytesHashed, true);
                            }

                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL)) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }
                        }

                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        metrics.TallyPushOp(stack.back().size());
                        if (opcode == OP_CHECKSIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKSIGVERIFY);
                            }
                        }
                    } break;

                    case OP_CHECKDATASIG:
                    case OP_CHECKDATASIGVERIFY: {
                        // (sig message pubkey -- bool)
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype const &vchSig = stacktop(-3);
                        valtype const &vchMessage = stacktop(-2);
                        valtype const &vchPubKey = stacktop(-1);

                        if (!CheckDataSignatureEncoding(vchSig, flags,
                                                        serror) ||
                            !CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                            // serror is set
                            return false;
                        }

                        bool fSuccess = false;
                        if (vchSig.size()) {
                            uint256 sigHash{uint256::Uninitialized};
                            CSHA256()
                                .Write(vchMessage.data(), vchMessage.size())
                                .Finalize(sigHash.data());
                            fSuccess = checker.VerifySignature(vchSig, CPubKey(vchPubKey), sigHash);
                            metrics.TallySigChecks(1);
                            metrics.TallyHashOp(vchMessage.size(), false);

                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL)) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }
                        }

                        popstack(stack);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        metrics.TallyPushOp(stack.back().size());
                        if (opcode == OP_CHECKDATASIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKDATASIGVERIFY);
                            }
                        }
                    } break;

                    case OP_CHECKMULTISIG:
                    case OP_CHECKMULTISIGVERIFY: {
                        // ([dummy] [sig ...] num_of_signatures [pubkey ...]
                        // num_of_pubkeys -- bool)
                        const size_t idxKeyCount = 1;
                        if (stack.size() < idxKeyCount) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const nKeysCount = CScriptNum(stacktop(-idxKeyCount), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        if (nKeysCount < 0 || nKeysCount > MAX_PUBKEYS_PER_MULTISIG) {
                            return set_error(serror, ScriptError::PUBKEY_COUNT);
                        }
                        if ( ! chipVmLimitsEnabled) {
                            // Pre May 2025: tally nOpCount
                            // Post May 2025: we account for this differently
                            nOpCount += nKeysCount;
                            if (nOpCount > MAX_OPS_PER_SCRIPT_LEGACY) {
                                return set_error(serror, ScriptError::OP_COUNT);
                            }
                        }

                        // stack depth of the top pubkey
                        const size_t idxTopKey = idxKeyCount + 1;

                        // stack depth of nSigsCount
                        const size_t idxSigCount = idxTopKey + nKeysCount;
                        if (stack.size() < idxSigCount) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const nSigsCount = CScriptNum(stacktop(-idxSigCount), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        if (nSigsCount < 0 || nSigsCount > nKeysCount) {
                            return set_error(serror, ScriptError::SIG_COUNT);
                        }

                        // stack depth of the top signature
                        const size_t idxTopSig = idxSigCount + 1;

                        // stack depth of the dummy element
                        const size_t idxDummy = idxTopSig + nSigsCount;
                        if (stack.size() < idxDummy) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // Subset of script starting at the most recent
                        // codeseparator
                        CScript scriptCode(pbegincodehash, pend);

                        // Assuming success is usually a bad idea, but the
                        // schnorr path can only succeed.
                        bool fSuccess = true;

                        if ((flags & SCRIPT_ENABLE_SCHNORR_MULTISIG) &&
                            stacktop(-idxDummy).size() != 0) {
                            // SCHNORR MULTISIG
                            static_assert(
                                MAX_PUBKEYS_PER_MULTISIG < 32,
                                "Schnorr multisig checkbits implementation "
                                "assumes < 32 pubkeys.");
                            uint32_t checkBits = 0;

                            // Dummy element is to be interpreted as a bitfield
                            // that represent which pubkeys should be checked.
                            valtype const &vchDummy = stacktop(-idxDummy);
                            if ( ! DecodeBitfield(vchDummy, nKeysCount, checkBits, serror)) {
                                // serror is set
                                return false;
                            }

                            // The bitfield doesn't set the right number of
                            // signatures.
                            if (countBits(checkBits) != uint32_t(nSigsCount)) {
                                return set_error(serror, ScriptError::INVALID_BIT_COUNT);
                            }

                            const size_t idxBottomKey = idxTopKey + nKeysCount - 1;
                            const size_t idxBottomSig = idxTopSig + nSigsCount - 1;

                            int iKey = 0;
                            for (int iSig = 0; iSig < nSigsCount; iSig++, iKey++) {
                                if ((checkBits >> iKey) == 0) {
                                    // This is a sanity check and should be
                                    // unrecheable.
                                    return set_error(serror, ScriptError::INVALID_BIT_RANGE);
                                }

                                // Find the next suitable key.
                                while (((checkBits >> iKey) & 0x01) == 0) {
                                    iKey++;
                                }

                                if (iKey >= nKeysCount) {
                                    // This is a sanity check and should be
                                    // unrecheable.
                                    return set_error(serror, ScriptError::PUBKEY_COUNT);
                                }

                                // Check the signature.
                                valtype const &vchSig = stacktop(-idxBottomSig + iSig);
                                valtype const &vchPubKey = stacktop(-idxBottomKey + iKey);

                                // Note that only pubkeys associated with a
                                // signature are checked for validity.
                                if ( ! CheckTransactionSchnorrSignatureEncoding(vchSig, flags, serror) ||
                                     ! CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                                    // serror is set
                                    return false;
                                }

                                // Check signature
                                size_t bytesHashed{};
                                if (!checker.CheckSig(vchSig, vchPubKey, scriptCode, flags, &bytesHashed)) {
                                    // This can fail if the signature is empty,
                                    // which also is a NULLFAIL error as the
                                    // bitfield should have been null in this
                                    // situation.
                                    return set_error(serror, ScriptError::SIG_NULLFAIL);
                                }

                                // this is guaranteed to execute exactly
                                // nSigsCount times (if not script error)
                                metrics.TallySigChecks(1);
                                // Account for hash ops
                                if (bytesHashed > 0u) {
                                    metrics.TallyHashOp(bytesHashed, true);
                                }
                            }

                            if ((checkBits >> iKey) != 0) {
                                // This is a sanity check and should be
                                // unrecheable.
                                return set_error(serror, ScriptError::INVALID_BIT_COUNT);
                            }
                        } else {
                            // LEGACY MULTISIG (ECDSA / NULL)

                            // Remove signature for pre-fork scripts
                            for (int k = 0; k < nSigsCount; k++) {
                                valtype const &vchSig = stacktop(-idxTopSig - k);
                                CleanupScriptCode(scriptCode, vchSig, flags);
                            }

                            int nSigsRemaining = nSigsCount;
                            int nKeysRemaining = nKeysCount;
                            while (fSuccess && nSigsRemaining > 0) {
                                valtype const &vchSig = stacktop(
                                    -idxTopSig - (nSigsCount - nSigsRemaining));
                                valtype const &vchPubKey = stacktop(
                                    -idxTopKey - (nKeysCount - nKeysRemaining));

                                // Note how this makes the exact order of
                                // pubkey/signature evaluation distinguishable
                                // by CHECKMULTISIG NOT if the STRICTENC flag is
                                // set. See the script_(in)valid tests for
                                // details.
                                if (!CheckTransactionECDSASignatureEncoding(
                                        vchSig, flags, serror) ||
                                    !CheckPubKeyEncoding(vchPubKey, flags,
                                                         serror)) {
                                    // serror is set
                                    return false;
                                }

                                // Check signature
                                size_t bytesHashed{};
                                bool fOk = checker.CheckSig(vchSig, vchPubKey, scriptCode, flags, &bytesHashed);
                                // Account for hash ops (may be 0 on nullsig, in which case we hashed nothing)
                                if (bytesHashed > 0u) {
                                    metrics.TallyHashOp(bytesHashed, true);
                                }

                                if (fOk) {
                                    nSigsRemaining--;
                                }
                                nKeysRemaining--;

                                // If there are more signatures left than keys
                                // left, then too many signatures have failed.
                                // Exit early, without checking any further
                                // signatures.
                                if (nSigsRemaining > nKeysRemaining) {
                                    fSuccess = false;
                                }
                            }

                            bool areAllSignaturesNull = true;
                            for (int i = 0; i < nSigsCount; i++) {
                                if (stacktop(-idxTopSig - i).size()) {
                                    areAllSignaturesNull = false;
                                    break;
                                }
                            }

                            // If the operation failed, we may require that all
                            // signatures must be empty vector
                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL) &&
                                !areAllSignaturesNull) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }

                            if (!areAllSignaturesNull) {
                                // This is not identical to the number of actual
                                // ECDSA verifies, but, it is an upper bound
                                // that can be easily determined without doing
                                // CPU-intensive checks.
                                metrics.TallySigChecks(nKeysCount);
                            }
                        }

                        // Clean up stack of all arguments
                        for (size_t i = 0; i < idxDummy; i++) {
                            popstack(stack);
                        }

                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        metrics.TallyPushOp(stack.back().size());
                        if (opcode == OP_CHECKMULTISIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKMULTISIGVERIFY);
                            }
                        }
                    } break;

                    //
                    // Byte string operations
                    //
                    case OP_CAT: {
                        // (x1 x2 -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch1 = stacktop(-2);
                        const valtype &vch2 = stacktop(-1);
                        if (vch1.size() + vch2.size() > maxScriptElementSize) {
                            return set_error(serror, ScriptError::PUSH_SIZE);
                        }
                        vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                        popstack(stack);
                        metrics.TallyPushOp(stack.back().size());
                    } break;

                    case OP_SPLIT: {
                        // (in position -- x1 x2)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        const valtype &data = stacktop(-2);

                        // Make sure the split point is appropriate.
                        int64_t const position = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        if (position < 0 || uint64_t(position) > data.size()) {
                            return set_error(serror, ScriptError::INVALID_SPLIT_RANGE);
                        }

                        // Prepare the results in their own buffer as `data` will be invalidated.
                        valtype n1(data.begin(), data.begin() + position);
                        valtype n2(data.begin() + position, data.end());

                        // Replace existing stack values by the new values.
                        const size_t totalSize = n1.size() + n2.size();
                        stacktop(-2) = std::move(n1);
                        stacktop(-1) = std::move(n2);
                        metrics.TallyPushOp(totalSize);
                    } break;

                    case OP_REVERSEBYTES: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype &data = stacktop(-1);
                        std::reverse(data.begin(), data.end());
                        metrics.TallyPushOp(data.size());
                    } break;

                    //
                    // Conversion operations
                    //
                    case OP_NUM2BIN: {
                        // (in size -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        uint64_t const size = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        if (size > maxScriptElementSize) {
                            return set_error(serror, ScriptError::PUSH_SIZE);
                        }

                        popstack(stack);
                        valtype &rawnum = stacktop(-1);

                        // Try to see if we can fit that number in the number of byte requested.
                        ScriptNumType::MinimallyEncode(rawnum);
                        if (rawnum.size() > size) {
                            // We definitively cannot.
                            return set_error(serror, ScriptError::IMPOSSIBLE_ENCODING);
                        }

                        // We already have an element of the right size, we don't need to do anything.
                        if (rawnum.size() == size) {
                            metrics.TallyPushOp(rawnum.size());
                            break;
                        }

                        uint8_t signbit = 0x00;
                        if (rawnum.size() > 0) {
                            signbit = rawnum.back() & 0x80;
                            rawnum[rawnum.size() - 1] &= 0x7f;
                        }

                        rawnum.reserve(size);
                        while (rawnum.size() < size - 1) {
                            rawnum.push_back(0x00);
                        }

                        rawnum.push_back(signbit);
                        metrics.TallyPushOp(rawnum.size());
                    } break;

                    case OP_BIN2NUM: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype &n = stacktop(-1);
                        ScriptNumType::MinimallyEncode(n);
                        metrics.TallyPushOp(n.size());

                        // The resulting number must be a valid number.
                        // Note: IsMinimallyEncoded() here is really just checking if the number is in range.
                        if ( ! ScriptNumType::IsMinimallyEncoded(n, maxIntegerSize)) {
                            return set_error(serror, invalidNumberRangeError);
                        }
                    } break;


                    // Note: For the introspection opcodes, we intentionally use CScriptNum for reading/writing the
                    //       inputs/outputs of these opcodes as a performance optimization, since their parameters
                    //       and their results can never exceed 64-bits.

                    // Native Introspection opcodes (Nullary)
                    case OP_INPUTINDEX:
                    case OP_ACTIVEBYTECODE:
                    case OP_TXVERSION:
                    case OP_TXINPUTCOUNT:
                    case OP_TXOUTPUTCOUNT:
                    case OP_TXLOCKTIME: {
                        if ( ! nativeIntrospection) {
                            return set_error(serror, ScriptError::BAD_OPCODE);
                        }
                        if ( ! context) {
                            return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                        }

                        switch (opcode) {
                            //  Operations
                            case OP_INPUTINDEX: {
                                auto const bn = CScriptNum::fromInt(context->inputIndex()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_ACTIVEBYTECODE: {
                                // Subset of script starting at the most recent code separator (if any)
                                // or the entire script if no code separators are present.
                                if (size_t(script.end() - pbegincodehash) > maxScriptElementSize) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(pbegincodehash, script.end());
                            } break;
                            case OP_TXVERSION: {
                                auto const bn = CScriptNum::fromInt(context->tx().nVersion()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXINPUTCOUNT: {
                                auto const bn = CScriptNum::fromInt(context->tx().vin().size()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXOUTPUTCOUNT: {
                                auto const bn = CScriptNum::fromInt(context->tx().vout().size()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXLOCKTIME: {
                                auto const bn = CScriptNum::fromInt(context->tx().nLockTime()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            default: {
                                assert(!"invalid opcode");
                                break;
                            }
                        }
                        // Tally push cost
                        metrics.TallyPushOp(stack.back().size());
                    } break; // end of Native Introspection opcodes (Nullary)

                    // Native Introspection opcodes (Unary)
                    case OP_UTXOTOKENCATEGORY:
                    case OP_UTXOTOKENCOMMITMENT:
                    case OP_UTXOTOKENAMOUNT:
                    case OP_OUTPUTTOKENCATEGORY:
                    case OP_OUTPUTTOKENCOMMITMENT:
                    case OP_OUTPUTTOKENAMOUNT:
                        // These require native tokens (upgrade9)
                        if ( ! nativeTokens) {
                            return set_error(serror, ScriptError::BAD_OPCODE);
                        }
                        [[fallthrough]];
                    case OP_UTXOVALUE:
                    case OP_UTXOBYTECODE:
                    case OP_OUTPOINTTXHASH:
                    case OP_OUTPOINTINDEX:
                    case OP_INPUTBYTECODE:
                    case OP_INPUTSEQUENCENUMBER:
                    case OP_OUTPUTVALUE:
                    case OP_OUTPUTBYTECODE: {

                        if ( ! nativeIntrospection) {
                            return set_error(serror, ScriptError::BAD_OPCODE);
                        }
                        if ( ! context) {
                            return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                        }

                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSizeLegacy).getint64();
                        popstack(stack); // consume element

                        auto is_valid_input_index = [&] {
                            if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                            }
                            return true;
                        };
                        auto is_valid_output_index = [&] {
                            if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                            }
                            return true;
                        };
                        auto get_bytecode = [&nativeTokens](const CTxOut &txout) -> std::vector<uint8_t> {
                            std::vector<uint8_t> ret;
                            if (!nativeTokens && txout.tokenDataPtr) {
                                // Special pre-activation case for upgrade9; If they ask for the bytecode, and
                                // there is PATFO token data we must return what a naive node would return here:
                                // The full serialized spk blob pre-activation [TOKEN_PREFIX + tokenData + spk]
                                token::WrappedScriptPubKey wspk;
                                token::WrapScriptPubKey(wspk, txout.tokenDataPtr, txout.scriptPubKey, INIT_PROTO_VERSION);
                                ret.assign(wspk.begin(), wspk.end());
                            } else {
                                // Post-activation or if no PATFO token data; Return just the scriptPubKey.
                                ret.assign(txout.scriptPubKey.begin(), txout.scriptPubKey.end());
                            }
                            return ret;
                        };

                        switch (opcode) {
                            case OP_UTXOVALUE: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const bn = CScriptNum::fromInt(context->coinAmount(index) / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_UTXOBYTECODE: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto utxoScript = get_bytecode(context->coin(index).GetTxOut());
                                if (utxoScript.size() > maxScriptElementSize) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.push_back(std::move(utxoScript));
                            } break;

                            case OP_OUTPOINTTXHASH: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const& txid = input.prevout.GetTxId();
                                static_assert(TxId::size() <= std::min(MAX_SCRIPT_ELEMENT_SIZE_LEGACY, may2025::MAX_SCRIPT_ELEMENT_SIZE));
                                stack.emplace_back(txid.begin(), txid.end());
                            } break;

                            case OP_OUTPOINTINDEX: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const bn = CScriptNum::fromInt(input.prevout.GetN()).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_INPUTBYTECODE: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                auto const& inputScript = context->scriptSig(index);
                                if (inputScript.size() > maxScriptElementSize) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(inputScript.begin(), inputScript.end());
                            } break;

                            case OP_INPUTSEQUENCENUMBER: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const bn = CScriptNum::fromInt(input.nSequence).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_OUTPUTVALUE: {
                                if ( ! is_valid_output_index()) {
                                    return false; // serror set by is_invalid_output_index lambda
                                }
                                auto const& output = context->tx().vout()[index];
                                auto const bn = CScriptNum::fromInt(output.nValue / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_OUTPUTBYTECODE: {
                                if ( ! is_valid_output_index()) {
                                    return false; // serror set by is_invalid_output_index lambda
                                }
                                auto outputScript = get_bytecode(context->tx().vout()[index]);
                                if (outputScript.size() > maxScriptElementSize) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.push_back(std::move(outputScript));
                            } break;

                            // Token introspection
                            case OP_UTXOTOKENCATEGORY: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                if (const auto &pdata = context->coinTokenData(index); !pdata) {
                                    // no token data, push CScriptNum 0 (empty vec)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // has token data, push token id (32 bytes) + *maybe* 0x1 or 0x2 (1 byte)
                                    const auto &tokId = pdata->GetId();
                                    valtype vch;
                                    // only push the capability if it's one of: 0x1 (mutable) or 0x2 (minting)
                                    const bool pushCapByte = pdata->IsMintingNFT() || pdata->IsMutableNFT();
                                    vch.reserve(tokId.size() + pushCapByte);
                                    vch.insert(vch.end(), tokId.begin(), tokId.end());
                                    if (pushCapByte) vch.push_back(static_cast<uint8_t>(pdata->GetCapability()));
                                    if (vch.size() > maxScriptElementSize) {
                                        // This branch cannot be taken in the current code, but is left in defensively.
                                        return set_error(serror, ScriptError::PUSH_SIZE);
                                    }
                                    stack.push_back(std::move(vch));
                                }
                            } break;

                            case OP_UTXOTOKENCOMMITMENT: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                if (const auto &pdata = context->coinTokenData(index); !pdata || !pdata->HasNFT()) {
                                    // no token data, or has token data but is not an NFT, push CScriptNum 0 (empty vec)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // Has token data, push commitment bytes, if they are <= MAX_SCRIPT_ELEMENT_SIZE_*
                                    const auto &commitment = pdata->GetCommitment();
                                    if (commitment.size() > maxScriptElementSize) {
                                        // This branch can normally only be taken in tests
                                        return set_error(serror, ScriptError::PUSH_SIZE);
                                    }
                                    // Push the bytes verbatim to the stack
                                    stack.emplace_back(commitment.begin(), commitment.end());
                                }
                            } break;

                            case OP_UTXOTOKENAMOUNT: {
                                if ( ! is_valid_input_index()) {
                                    return false; // serror set by is_invalid_input_index lambda
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                if (const auto &pdata = context->coinTokenData(index); !pdata) {
                                    // no token data, push VM number 0 (empty vector)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // push the amount as a CScriptNum amount. Note it can be zero for NFT-only
                                    // tokens, in which case an empty vector {} will be pushed.
                                    auto const bn = CScriptNum::fromInt(pdata->GetAmount().getint64()).value();
                                    stack.push_back(bn.getvch());
                                }
                            } break;

                            case OP_OUTPUTTOKENCATEGORY: {
                                if ( ! is_valid_output_index()) {
                                    return false; // serror set by is_invalid_output_index lambda
                                }
                                auto const& output = context->tx().vout()[index];
                                if (const auto &pdata = output.tokenDataPtr; !pdata) {
                                    // no token data, push CScriptNum 0 (empty vec)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // has token data, push token id (32 bytes) + *maybe* 0x1 or 0x2 (1 byte)
                                    const auto &tokId = pdata->GetId();
                                    valtype vch;
                                    // only push the capability if it's one of: 0x1 (mutable) or 0x2 (minting)
                                    const bool pushCapByte = pdata->IsMintingNFT() || pdata->IsMutableNFT();
                                    vch.reserve(tokId.size() + pushCapByte);
                                    vch.insert(vch.end(), tokId.begin(), tokId.end());
                                    if (pushCapByte) vch.push_back(static_cast<uint8_t>(pdata->GetCapability()));
                                    if (vch.size() > maxScriptElementSize) {
                                        // This branch cannot be taken in the current code, but is left in defensively.
                                        return set_error(serror, ScriptError::PUSH_SIZE);
                                    }
                                    stack.push_back(std::move(vch));
                                }
                            } break;

                            case OP_OUTPUTTOKENCOMMITMENT: {
                                if ( ! is_valid_output_index()) {
                                    return false; // serror set by is_invalid_output_index lambda
                                }
                                auto const& output = context->tx().vout()[index];
                                if (const auto &pdata = output.tokenDataPtr; !pdata || !pdata->HasNFT()) {
                                    // no token data, or has token data but is not an NFT, push CScriptNum 0 (empty vec)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // Has token data, push commitment bytes, if they are <= MAX_SCRIPT_ELEMENT_SIZE_*
                                    const auto &commitment = pdata->GetCommitment();
                                    if (commitment.size() > maxScriptElementSize) {
                                        // This branch can normally only be taken in tests
                                        return set_error(serror, ScriptError::PUSH_SIZE);
                                    }
                                    // Push the bytes verbatim to the stack
                                    stack.emplace_back(commitment.begin(), commitment.end());
                                }
                            } break;

                            case OP_OUTPUTTOKENAMOUNT: {
                                if ( ! is_valid_output_index()) {
                                    return false; // serror set by is_invalid_output_index lambda
                                }
                                auto const& output = context->tx().vout()[index];
                                if (const auto &pdata = output.tokenDataPtr; !pdata) {
                                    // no token data, push VM number 0 (empty vector)
                                    stack.push_back(CScriptNum::fromIntUnchecked(0).getvch());
                                } else {
                                    // push the amount as a CScriptNum amount. Note it can be zero for NFT-only
                                    // tokens, in which case an empty vector {} will be pushed.
                                    auto const bn = CScriptNum::fromInt(pdata->GetAmount().getint64()).value();
                                    stack.push_back(bn.getvch());
                                }
                            } break;

                            default: {
                                assert(!"invalid opcode");
                                break;
                            }
                        }
                        // Tally push cost
                        metrics.TallyPushOp(stack.back().size());
                    } break; // end of Native Introspection opcodes (Unary)

                    default:
                        return set_error(serror, ScriptError::BAD_OPCODE);
                }
            }

            // Size limits
            if (stack.size() + altstack.size() > MAX_STACK_SIZE) {
                return set_error(serror, ScriptError::STACK_SIZE);
            }

            // Enforce May 2025 VM limits
            if (chipVmLimitsEnabled) {
                // Check that this opcode did not cause us to exceed opCost and/or hashIters limits.
                // Note: `metrics` may lack a valid "scriptLimits" object in rare cases (tests only), in which case
                // the below two limit checks are always going to return false.
                if (metrics.IsOverOpCostLimit(flags)) {
                    return set_error(serror, ScriptError::OP_COST);
                }
                if (metrics.IsOverHashItersLimit()) {
                    return set_error(serror, ScriptError::TOO_MANY_HASH_ITERS);
                }

                // Conditional stack may not exceed depth of 100
                if (vfExec.size() > may2025::MAX_CONDITIONAL_STACK_DEPTH) {
                    return set_error(serror, ScriptError::CONDITIONAL_STACK_DEPTH);
                }
            }
        }
    } catch (const scriptnum_error &e) {
        return set_error(serror, e.scriptError);
    } catch (...) {
        return set_error(serror, ScriptError::UNKNOWN);
    }

    if (!vfExec.empty()) {
        return set_error(serror, ScriptError::UNBALANCED_CONDITIONAL);
    }

    return set_success(serror);
}

/**
 * Wrapper that serializes like CTransaction, but with the modifications
 *  required for the signature hash done in-place
 */
class CTransactionViewSignatureSerializer {
    //! reference to the spending transaction (the one being serialized)
    const CTransactionView txTo;
    //! output script being consumed
    const CScript &scriptCode;
    //! input index of txTo being signed
    const unsigned int nIn;
    //! container for hashtype flags
    const SigHashType sigHashType;

public:
    CTransactionViewSignatureSerializer(CTransactionView txToIn, const CScript &scriptCodeIn, unsigned nInIn,
                                        SigHashType sigHashTypeIn)
        : txTo(txToIn), scriptCode(scriptCodeIn), nIn(nInIn), sigHashType(sigHashTypeIn) {}

    /** Serialize the passed scriptCode, skipping OP_CODESEPARATORs */
    template <typename S> void SerializeScriptCode(S &s) const {
        CScript::const_iterator it = scriptCode.begin();
        CScript::const_iterator itBegin = it;
        opcodetype opcode;
        unsigned int nCodeSeparators = 0;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                nCodeSeparators++;
            }
        }
        ::WriteCompactSize(s, scriptCode.size() - nCodeSeparators);
        it = itBegin;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                s.write((char *)&itBegin[0], it - itBegin - 1);
                itBegin = it;
            }
        }
        if (itBegin != scriptCode.end()) {
            s.write((char *)&itBegin[0], it - itBegin);
        }
    }

    /** Serialize an input of txTo */
    template <typename S> void SerializeInput(S &s, unsigned int nInput) const {
        // In case of SIGHASH_ANYONECANPAY, only the input being signed is
        // serialized
        if (sigHashType.hasAnyoneCanPay()) {
            nInput = nIn;
        }
        // Serialize the prevout
        ::Serialize(s, txTo.vin()[nInput].prevout);
        // Serialize the script
        if (nInput != nIn) {
            // Blank out other inputs' signatures
            ::Serialize(s, CScript());
        } else {
            SerializeScriptCode(s);
        }
        // Serialize the nSequence
        if (nInput != nIn &&
            (sigHashType.getBaseType() == BaseSigHashType::SINGLE ||
             sigHashType.getBaseType() == BaseSigHashType::NONE)) {
            // let the others update at will
            ::Serialize(s, (int)0);
        } else {
            ::Serialize(s, txTo.vin()[nInput].nSequence);
        }
    }

    /** Serialize an output of txTo */
    template <typename S>
    void SerializeOutput(S &s, unsigned int nOutput) const {
        if (sigHashType.getBaseType() == BaseSigHashType::SINGLE &&
            nOutput != nIn) {
            // Do not lock-in the txout payee at other indices as txin
            ::Serialize(s, CTxOut());
        } else {
            ::Serialize(s, txTo.vout()[nOutput]);
        }
    }

    /** Serialize txTo */
    template <typename S> void Serialize(S &s) const {
        // Serialize nVersion
        ::Serialize(s, txTo.nVersion());
        // Serialize vin
        unsigned int nInputs =
            sigHashType.hasAnyoneCanPay() ? 1 : txTo.vin().size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++) {
            SerializeInput(s, nInput);
        }
        // Serialize vout
        unsigned int nOutputs =
            (sigHashType.getBaseType() == BaseSigHashType::NONE)
                ? 0
                : ((sigHashType.getBaseType() == BaseSigHashType::SINGLE)
                       ? nIn + 1
                       : txTo.vout().size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++) {
            SerializeOutput(s, nOutput);
        }
        // Serialize nLockTime
        ::Serialize(s, txTo.nLockTime());
    }
};

uint256 GetPrevoutHash(const ScriptExecutionContext &context) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : context.tx().vin()) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}

uint256 GetSequenceHash(const ScriptExecutionContext &context) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : context.tx().vin()) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

uint256 GetOutputsHash(const ScriptExecutionContext &context) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txout : context.tx().vout()) {
        ss << txout;
    }
    return ss.GetHash();
}

uint256 GetUtxosHash(const ScriptExecutionContext &context) {
    assert(!context.isLimited());
    const size_t nInputs = context.tx().vin().size();
    CHashWriter ss(SER_GETHASH, 0);
    for (size_t i = 0; i < nInputs; ++i) {
        const Coin &coin = context.coin(i);
        ss << coin.GetTxOut();
    }
    return ss.GetHash();
}

} // namespace

void PrecomputedTransactionData::PopulateFromContext(const ScriptExecutionContext &context) {
    hashPrevouts = GetPrevoutHash(context);
    hashSequence = GetSequenceHash(context);
    hashOutputs = GetOutputsHash(context);
    if (!context.isLimited()) {
        hashUtxos = GetUtxosHash(context);
    } else {
        hashUtxos.reset();
    }
    populated = true;
}

SignatureHashResult SignatureHash(const CScript &scriptCode, const ScriptExecutionContext &context,
                                  SigHashType sigHashType, const PrecomputedTransactionData *cache, uint32_t flags) {
    const unsigned nIn = context.inputIndex();
    const CTransactionView &txTo = context.tx();
    assert(nIn < txTo.vin().size());

    if (sigHashType.hasFork() && (flags & SCRIPT_ENABLE_SIGHASH_FORKID)) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;
        std::optional<uint256> hashUtxos;

        if (!sigHashType.hasAnyoneCanPay()) {
            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(context);
        }

        if (!sigHashType.hasAnyoneCanPay() && sigHashType.getBaseType() != BaseSigHashType::SINGLE
            && sigHashType.getBaseType() != BaseSigHashType::NONE) {
            hashSequence = cache ? cache->hashSequence : GetSequenceHash(context);
        }

        if (sigHashType.hasUtxos() && (flags & SCRIPT_ENABLE_TOKENS)) {
            if (cache && cache->hashUtxos) {
                hashUtxos = *cache->hashUtxos;
            } else if (!context.isLimited()) {
                hashUtxos = GetUtxosHash(context);
            }
        }

        if (const auto bt = sigHashType.getBaseType(); bt != BaseSigHashType::SINGLE && bt != BaseSigHashType::NONE) {
            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(context);
        } else if (bt == BaseSigHashType::SINGLE && nIn < txTo.vout().size()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo.vout()[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo.nVersion();
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        // SIGHASH_UTXOS requires Upgrade9 SCRIPT_ENABLE_TOKENS, otherwise skip
        if (sigHashType.hasUtxos() && (flags & SCRIPT_ENABLE_TOKENS)) {
            if (!hashUtxos) {
                // This should never happen in production because in production we have real "non limited" contexts.
                throw SignatureHashMissingUtxoDataError(
                    strprintf("SignatureHash error: SIGHASH_UTXOS requested but missing utxo data,"
                              " txid: %s, inputNum: %i", txTo.GetId().ToString(), nIn));
            }
            ss << *hashUtxos;
        }
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with [tokenBlob?] + scriptCode + amount). The prevout may
        // already be contained in hashPrevout, and the nSequence may already be contained in hashSequence.
        ss << txTo.vin()[nIn].prevout;
        const CTxOut &prevTxOut = context.coin().GetTxOut();
        if (prevTxOut.tokenDataPtr && (flags & SCRIPT_ENABLE_TOKENS)) {
            // New! For tokens (Upgrade9). If we had tokenData we inject it as a blob of:
            //    token::PREFIX_BYTE + ser_token_data
            // right *before* scriptCode's length byte.  This *intentionally* makes it so that unupgraded software
            // cannot send tokens (and thus cannot unintentionally burn tokens).
            //
            // Note: The serialization operation for token::OutputData may throw if the data it is serializing is not
            // sane.  The data will always be sane when verifying or producing sigs in production. However, the below
            // may throw in tests that intentionally sabotage the tokenData to be inconsistent.
            ss << token::PREFIX_BYTE << *prevTxOut.tokenDataPtr;
        }
        ss << scriptCode;
        ss << prevTxOut.nValue;
        ss << txTo.vin()[nIn].nSequence;
        // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;
        // Locktime
        ss << txTo.nLockTime();
        // Sighash type
        ss << sigHashType;

        return {ss.GetHash(), ss.GetNumBytesWritten()};
    }

    // Check for invalid use of SIGHASH_SINGLE
    if (sigHashType.getBaseType() == BaseSigHashType::SINGLE && nIn >= txTo.vout().size()) {
        //  nOut out of range
        static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
        return {one, 0u};
    }

    // Wrapper to serialize only the necessary parts of the transaction being
    // signed
    const CTransactionViewSignatureSerializer txTmp(txTo, scriptCode, nIn, sigHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << sigHashType;
    return {ss.GetHash(), ss.GetNumBytesWritten()};
}

bool BaseSignatureChecker::VerifySignature(const std::vector<uint8_t> &vchSig,
                                           const CPubKey &pubkey,
                                           const uint256 &sighash) const {
    if (vchSig.size() == 64) {
        return pubkey.VerifySchnorr(sighash, vchSig);
    } else {
        return pubkey.VerifyECDSA(sighash, vchSig);
    }
}

ContextOptSignatureChecker::~ContextOptSignatureChecker() {}

bool TransactionSignatureChecker::CheckSig(const std::vector<uint8_t> &vchSigIn, const std::vector<uint8_t> &vchPubKey,
                                           const CScript &scriptCode, uint32_t flags, size_t *pnBytesHashed) const {
    if (pnBytesHashed) *pnBytesHashed = 0u;
    CPubKey const pubkey(vchPubKey);
    if (!pubkey.IsValid()) {
        return false;
    }

    // Hash type is one byte tacked on to the end of the signature
    std::vector<uint8_t> vchSig(vchSigIn);
    if (vchSig.empty()) {
        return false;
    }
    SigHashType const sigHashType = GetHashType(vchSig);
    vchSig.pop_back();

    const auto & [sighash, bytesHashed] = SignatureHash(scriptCode, context, sigHashType, this->txdata, flags);
    if (pnBytesHashed) *pnBytesHashed = bytesHashed;

    return VerifySignature(vchSig, pubkey, sighash);
}

bool TransactionSignatureChecker::CheckLockTime(const CScriptNum &nLockTime) const {
    const CTransactionView &txTo = context.tx();
    const unsigned nIn = context.inputIndex();
    // There are two kinds of nLockTime: lock-by-blockheight and
    // lock-by-blocktime, distinguished by whether nLockTime <
    // LOCKTIME_THRESHOLD.
    //
    // We want to compare apples to apples, so fail the script unless the type
    // of nLockTime being tested is the same as the nLockTime in the
    // transaction.
    if (!((txTo.nLockTime() < LOCKTIME_THRESHOLD && nLockTime < LOCKTIME_THRESHOLD)
          || (txTo.nLockTime() >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD))) {
        return false;
    }

    // Now that we know we're comparing apples-to-apples, the comparison is a
    // simple numeric one.
    if (nLockTime > static_cast<int64_t>(txTo.nLockTime())) { // CScriptNum has many overloads - ensure the right one is selected
        return false;
    }

    // Finally the nLockTime feature can be disabled and thus
    // CHECKLOCKTIMEVERIFY bypassed if every txin has been finalized by setting
    // nSequence to maxint. The transaction would be allowed into the
    // blockchain, making the opcode ineffective.
    //
    // Testing if this vin is not final is sufficient to prevent this condition.
    // Alternatively we could test all inputs, but testing just this input
    // minimizes the data required to prove correct CHECKLOCKTIMEVERIFY
    // execution.
    if (CTxIn::SEQUENCE_FINAL == txTo.vin()[nIn].nSequence) {
        return false;
    }

    return true;
}

bool TransactionSignatureChecker::CheckSequence(const CScriptNum &nSequence) const {
    const CTransactionView &txTo = context.tx();
    const unsigned nIn = context.inputIndex();
    // Relative lock times are supported by comparing the passed in operand to
    // the sequence number of the input.
    const int64_t txToSequence = static_cast<int64_t>(txTo.vin()[nIn].nSequence);

    // Fail if the transaction's version number is not set high enough to
    // trigger BIP 68 rules.
    if (static_cast<uint32_t>(txTo.nVersion()) < 2u) {
        return false;
    }

    // Sequence numbers with their most significant bit set are not consensus
    // constrained. Testing that the transaction's sequence number do not have
    // this bit set prevents using this property to get around a
    // CHECKSEQUENCEVERIFY check.
    if (txToSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
        return false;
    }

    // Mask off any bits that do not have consensus-enforced meaning before
    // doing the integer comparisons
    const uint32_t nLockTimeMask = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | CTxIn::SEQUENCE_LOCKTIME_MASK;
    const int64_t txToSequenceMasked = txToSequence & nLockTimeMask;

    auto const res = nSequence.safeBitwiseAnd(nLockTimeMask);
    if ( ! res) {
        // Defensive programming: It is impossible that this branch be taken unless the current
        // values of the operands are changed.
        return false;
    }
    auto const nSequenceMasked = *res;

    // There are two kinds of nSequence: lock-by-blockheight and
    // lock-by-blocktime, distinguished by whether nSequenceMasked <
    // CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG.
    //
    // We want to compare apples to apples, so fail the script unless the type
    // of nSequenceMasked being tested is the same as the nSequenceMasked in the
    // transaction.
    if (!((txToSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) ||
          (txToSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG))) {
        return false;
    }

    // Now that we know we're comparing apples-to-apples, the comparison is a
    // simple numeric one.
    if (nSequenceMasked > txToSequenceMasked) {
        return false;
    }

    return true;
}

bool EvalScript(std::vector<valtype> &stack, const CScript &script, uint32_t flags,
                const BaseSignatureChecker &checker, ScriptExecutionMetrics &metrics, ScriptError *serror) {
    if (flags & SCRIPT_ENABLE_MAY2025) {
        return EvalScriptImpl<true>(stack, script, flags, checker, metrics, serror);
    } else {
        return EvalScriptImpl<false>(stack, script, flags, checker, metrics, serror);
    }
}

bool VerifyScript(const CScript &scriptSig, const CScript &scriptPubKey, uint32_t flags, const BaseSignatureChecker &checker,
                  ScriptExecutionMetrics &metricsOut, ScriptError *serror) {
    set_error(serror, ScriptError::UNKNOWN);

    // If FORKID is enabled, we also ensure strict encoding.
    if (flags & SCRIPT_ENABLE_SIGHASH_FORKID) {
        flags |= SCRIPT_VERIFY_STRICTENC;
    }

    if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
        return set_error(serror, ScriptError::SIG_PUSHONLY);
    }

    ScriptExecutionMetrics metrics = {};

    if (flags & SCRIPT_ENABLE_MAY2025) {
        // May 2025: Pre-set metrics.scriptLimits to enforce op cost limits.
        metrics.SetScriptLimits(flags, scriptSig.size());
    }

    std::vector<valtype> stack, stackCopy;
    if ( ! EvalScript(stack, scriptSig, flags, checker, metrics, serror)) {
        // serror is set
        return false;
    }
    if (flags & SCRIPT_VERIFY_P2SH) {
        stackCopy = stack;
    }
    if ( ! EvalScript(stack, scriptPubKey, flags, checker, metrics, serror)) {
        // serror is set
        return false;
    }
    if (stack.empty()) {
        return set_error(serror, ScriptError::EVAL_FALSE);
    }
    if (CastToBool(stack.back()) == false) {
        return set_error(serror, ScriptError::EVAL_FALSE);
    }

    // Additional validation for spend-to-script-hash transactions:
    if (bool p2sh_32; (flags & SCRIPT_VERIFY_P2SH) && scriptPubKey.IsPayToScriptHash(flags, nullptr, &p2sh_32)) {
        // scriptSig must be literals-only or validation fails
        if (!scriptSig.IsPushOnly()) {
            return set_error(serror, ScriptError::SIG_PUSHONLY);
        }

        // Restore stack.
        swap(stack, stackCopy);

        // stack cannot be empty here, because if it was the P2SH  HASH <> EQUAL
        // scriptPubKey would be evaluated with an empty stack and the
        // EvalScript above would return false.
        assert(!stack.empty());

        const valtype &pubKeySerialized = stack.back();
        CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stack);

        // Bail out early if SCRIPT_DISALLOW_SEGWIT_RECOVERY is not set, the
        // redeem script is a p2sh_20 segwit program, and it was the only item
        // pushed onto the stack.
        //
        // Note; We *never* allow this "unconditional" segwit recovery for
        // p2sh_32 since segwit funds can only be inadvertently locked into
        // p2sh_20 (legacy BTC) scripts, thus this special case for segwit
        // recovery should never apply to p2sh_32.
        if ((flags & SCRIPT_DISALLOW_SEGWIT_RECOVERY) == 0 && !p2sh_32 && stack.empty() &&
            pubKey2.IsWitnessProgram()) {
            // must set metricsOut for all successful returns
            metricsOut = metrics;
            return set_success(serror);
        }

        if ( ! EvalScript(stack, pubKey2, flags, checker, metrics, serror)) {
            // serror is set
            return false;
        }
        if (stack.empty()) {
            return set_error(serror, ScriptError::EVAL_FALSE);
        }
        if (!CastToBool(stack.back())) {
            return set_error(serror, ScriptError::EVAL_FALSE);
        }
    }

    // The CLEANSTACK check is only performed after potential P2SH evaluation,
    // as the non-P2SH evaluation of a P2SH script will obviously not result in
    // a clean stack (the P2SH inputs remain). The same holds for witness
    // evaluation.
    if ((flags & SCRIPT_VERIFY_CLEANSTACK) != 0) {
        // Disallow CLEANSTACK without P2SH, as otherwise a switch
        // CLEANSTACK->P2SH+CLEANSTACK would be possible, which is not a
        // softfork (and P2SH should be one).
        assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        if (stack.size() != 1) {
            return set_error(serror, ScriptError::CLEANSTACK);
        }
    }

    if (flags & SCRIPT_VERIFY_INPUT_SIGCHECKS) {
        // This limit is intended for standard use, and is based on an
        // examination of typical and historical standard uses.
        // - allowing P2SH ECDSA multisig with compressed keys, which at an
        // extreme (1-of-15) may have 15 SigChecks in ~590 bytes of scriptSig.
        // - allowing Bare ECDSA multisig, which at an extreme (1-of-3) may have
        // 3 sigchecks in ~72 bytes of scriptSig.
        // - Since the size of an input is 41 bytes + length of scriptSig, then
        // the most dense possible inputs satisfying this rule would be:
        //   2 sigchecks and 26 bytes: 1/33.50 sigchecks/byte.
        //   3 sigchecks and 69 bytes: 1/36.66 sigchecks/byte.
        // The latter can be readily done with 1-of-3 bare multisignatures,
        // however the former is not practically doable with standard scripts,
        // so the practical density limit is 1/36.66.
        static_assert(INT_MAX > MAX_SCRIPT_SIZE,
                      "overflow sanity check on max script size");
        static_assert(INT_MAX / 43 / 3 > MAX_OPS_PER_SCRIPT_LEGACY,
                      "overflow sanity check on maximum possible sigchecks "
                      "from sig+redeem+pub scripts");
        if (int(scriptSig.size()) < metrics.GetSigChecks() * 43 - 60) {
            return set_error(serror, ScriptError::INPUT_SIGCHECKS);
        }
    }

    metricsOut = metrics;
    return set_success(serror);
}
