// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include <cassert>
#include <cstdint>

// Maximum number of bytes pushable to the stack; only used pre May 2025; after we use may2025::MAX_SCRIPT_ELEMENT_SIZE
static constexpr unsigned int MAX_SCRIPT_ELEMENT_SIZE_LEGACY = 520;

// Maximum number of non-push operations per script; only used for consensus rules pre May 2025 upgrade; ignored after.
static constexpr int MAX_OPS_PER_SCRIPT_LEGACY = 201;

// Maximum number of public keys per multisig
static constexpr int MAX_PUBKEYS_PER_MULTISIG = 20;

// Maximum script length in bytes
static constexpr int MAX_SCRIPT_SIZE = 10000;

// Maximum number of values on script interpreter stack
static constexpr int MAX_STACK_SIZE = 1000;

// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp. Thresold is Tue Nov 5 00:53:20 1985 UTC
static constexpr uint32_t LOCKTIME_THRESHOLD = 500'000'000u;


// The below constants are used after activation of the May 2025 upgrade (Targeted VM Limits CHIP)
namespace may2025 {

// Maximum number of bytes pushable to the stack
static constexpr unsigned int MAX_SCRIPT_ELEMENT_SIZE = MAX_SCRIPT_SIZE;

// Base cost for each executed opcode; no opcodes incur a cost less than this, but some may incur more.
static constexpr unsigned int OPCODE_COST = 100u;

// Conditional stack depth limit (max depth of OP_IF and friends)
static constexpr unsigned int MAX_CONDITIONAL_STACK_DEPTH = 100u;

// Each sigcheck done by an input adds this amount to the total op cost
static constexpr unsigned int SIG_CHECK_COST_FACTOR = 26'000u;

// Some constants used by helper code.
namespace detail {
// 'non-standard' txns (block txns) get a 7x bonus to their hash iterations limit
static constexpr unsigned int HASH_ITER_BONUS_FOR_NONSTD_TXNS = 7u;
// Op cost allowance factor; this is multiplied by the input byte size to determine the total op cost allowance for an input.
static constexpr unsigned int OP_COST_BUDGET_PER_INPUT_BYTE = 800u;
// The penalty paid by "standard" (relay) txns per hash op; that is, 'standard' txns hash ops cost 3x.
static constexpr unsigned int HASH_COST_PENALTY_FOR_STD_TXNS = 3u;
// All hashers supported by VM opcodes (OP_HASH160, OP_HASH256, etc) use a 64-byte block size; update if adding hashers.
static constexpr unsigned int HASH_BLOCK_SIZE = 64u;
// As per the VM Limits CHIP, each input script has this fixed serialization overhead we credit to it, in bytes.
static constexpr unsigned int INPUT_SCRIPT_SIZE_FIXED_CREDIT = 41u;
// Returns the hash iteration limit for an input, given: 1) whether "standard" rules are in effect, and 2) the input's
// scriptSig size. See: https://github.com/bitjson/bch-vm-limits/tree/master?tab=readme-ov-file#maximum-hashing-density
inline constexpr int64_t GetInputHashItersLimit(bool standard, uint64_t scriptSigSize) noexcept {
    const auto factor = standard ? 1u : detail::HASH_ITER_BONUS_FOR_NONSTD_TXNS;
    const int64_t ret = ((scriptSigSize + detail::INPUT_SCRIPT_SIZE_FIXED_CREDIT) * factor) / 2u;
    assert(ret >= 0);
    return ret;
}
// Returns the op cost limit for an input, given an input's scriptSig size.
// See: https://github.com/bitjson/bch-vm-limits/tree/master?tab=readme-ov-file#operation-cost-limit
inline constexpr int64_t GetInputOpCostLimit(uint64_t scriptSigSize) noexcept {
    const int64_t ret = (scriptSigSize + detail::INPUT_SCRIPT_SIZE_FIXED_CREDIT) * detail::OP_COST_BUDGET_PER_INPUT_BYTE;
    assert(ret >= 0);
    return ret;
}
} // namespace detail

// Returns the per-hash iteration op cost, either: 64 if standard==false or 192 standard==true flag is set.
// See: https://github.com/bitjson/bch-vm-limits/tree/master?tab=readme-ov-file#summary
inline constexpr int64_t GetHashIterOpCostFactor(bool standard) noexcept {
    return standard ? detail::HASH_BLOCK_SIZE * detail::HASH_COST_PENALTY_FOR_STD_TXNS : detail::HASH_BLOCK_SIZE;
}

// Returns the hash iteration count given a particular message length and whether the hasher was two-round or not.
// See: https://github.com/bitjson/bch-vm-limits/tree/master?tab=readme-ov-file#digest-iteration-count
inline constexpr int64_t CalcHashIters(uint32_t messageLength, bool isTwoRoundHashOp) noexcept {
    return isTwoRoundHashOp + 1u + ((static_cast<uint64_t>(messageLength) + 8u) / detail::HASH_BLOCK_SIZE);
}

/// Encapsulates the script VM execution limits for a particular script, as derived from the scriptSig size and whether
/// we are in standard or non-standard mode. Used by interpreter.cpp EvalScript() and VerifyScript() in conjunction
/// with the ScriptExecutionMetrics object.
class ScriptLimits {
    int64_t opCostLimit;
    int64_t hashItersLimit;

public:
    ScriptLimits(bool standard, uint64_t scriptSigSize)
        : opCostLimit{detail::GetInputOpCostLimit(scriptSigSize)}, hashItersLimit{detail::GetInputHashItersLimit(standard, scriptSigSize)}
    {}

    int64_t GetOpCostLimit() const { return opCostLimit; }
    int64_t GetHashItersLimit() const { return hashItersLimit; }
};

} // namespace may2025
