// Copyright (c) 2020-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include <script/script_flags.h>
#include <script/vm_limits.h>

#include <cstdint>
#include <optional>

/**
 * Struct for holding cumulative results from executing a script or a sequence
 * of scripts.
 */
class ScriptExecutionMetrics {
    int nSigChecks = 0;

    /** CHIP-2021-05-vm-limits: Targeted Virtual Machine Limits */
    int64_t nOpCost = 0;
    int64_t nHashDigestIterations = 0;
    std::optional<may2025::ScriptLimits> scriptLimits;

    static bool IsVmLimitsStandard(uint32_t scriptFlags) { return scriptFlags & SCRIPT_VM_LIMITS_STANDARD; }

protected:
    // This constructor is used by tests
    ScriptExecutionMetrics(int sigChecks, int64_t opCost, int64_t hashDigestIterations)
        : nSigChecks(sigChecks), nOpCost(opCost), nHashDigestIterations(hashDigestIterations) {}

public:
    ScriptExecutionMetrics() noexcept = default;

    int GetSigChecks() const { return nSigChecks; }

    // Returns the composite value that is: nOpCost + nHashDigestIterators * {192 or 64} + nSigChecks * 26,000
    // Consensus code uses a 64 for the hashing iter cost, standard/relay code uses the more restrictive cost of 192.
    int64_t GetCompositeOpCost(uint32_t scriptFlags) const {
        const int64_t hashIterOpCostFactor = may2025::GetHashIterOpCostFactor(IsVmLimitsStandard(scriptFlags));
        return nOpCost // base cost: encompasses ops + pushes, etc
               // additional cost: add hash iterations * {192 or 64}
               + nHashDigestIterations * hashIterOpCostFactor
               // additional cost: add sig checks * 26,000
               + static_cast<int64_t>(nSigChecks) * int64_t{may2025::SIG_CHECK_COST_FACTOR};
    }

    int64_t GetBaseOpCost() const { return nOpCost; }

    int64_t GetHashDigestIterations() const { return nHashDigestIterations; }

    void TallyOp(uint32_t cost) {
        nOpCost += static_cast<int64_t>(cost);
    }

    void TallyHashOp(uint32_t messageLength, bool isTwoRoundHashOp /* set to true iff OP_HASH256 or OP_HASH160 */) {
        nHashDigestIterations += may2025::CalcHashIters(messageLength, isTwoRoundHashOp);
    }

    void TallyPushOp(uint32_t stackItemLength) {
        nOpCost += static_cast<int64_t>(stackItemLength);
    }

    void TallySigChecks(int nChecks) {
        nSigChecks += nChecks;
    }

    bool IsOverOpCostLimit(uint32_t scriptFlags) const {
        return scriptLimits && GetCompositeOpCost(scriptFlags) > scriptLimits->GetOpCostLimit();
    }

    bool IsOverHashItersLimit() const {
        return scriptLimits && GetHashDigestIterations() > scriptLimits->GetHashItersLimit();
    }

    bool HasValidScriptLimits() const { return scriptLimits.has_value(); }
    void SetScriptLimits(uint32_t scriptFlags, uint64_t scriptSigSize) {
        scriptLimits.emplace(IsVmLimitsStandard(scriptFlags), scriptSigSize);
    }
    const may2025::ScriptLimits *GetScriptLimits() const {
        return scriptLimits ? &*scriptLimits : nullptr;
    }
};
