// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <chainparams.h>
#include <coins.h>
#include <config.h>
#include <script/interpreter.h>
#include <span.h>
#include <test/libauth_testing_setup.h>
#include <tinyformat.h>
#include <util/string.h>
#include <validation.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {

struct PackDesc {
    std::string name; // Test pack name
    struct Flags {
        // Flags to set or exclude for each pack
        uint32_t std = 0, nonStd = 0, excludeStd = 0, excludeNonStd = 0;
    };
    Flags flags;
};

const std::vector<PackDesc> packsToRun{{
    {"2023", {/* .std = */ 0,
              /* .nonStd = */ 0,
              /* .excludeStd = */    SCRIPT_ENABLE_MAY2025 | SCRIPT_VM_LIMITS_STANDARD,
              /* .excludeNonStd = */ SCRIPT_ENABLE_MAY2025 | SCRIPT_VM_LIMITS_STANDARD}},
    {"2025", {/* .std = */    SCRIPT_ENABLE_MAY2025 | SCRIPT_VM_LIMITS_STANDARD,
              /* .nonStd = */ SCRIPT_ENABLE_MAY2025,
              /* .excludeStd = */ 0,
              /* .excludeNonStd = */ SCRIPT_VM_LIMITS_STANDARD}},
}};

// Calibrated to behave "as if" we process a 50KB block full of txns identical to this particular txn.
size_t simulatedBlockSize = 50'000u; // 50KB, adjust this down to make benches run fewer iterations, up for more iters

// Calculate the number of iterations for a test based on its txn size and also the value of `simulatedBlockSize`
size_t GetIters(size_t txnSizeBytes) {
    const size_t iters = std::round(simulatedBlockSize / std::max<double>(65, txnSizeBytes));
    return std::max<size_t>(iters, 3u); // minimum 3
}

using TestVector = LibauthTestingSetup::TestVector;
using Test = TestVector::Test;
using TxStandard = LibauthTestingSetup::TxStandard;
using State = benchmark::State;
using Printer = benchmark::Printer;

void RunBench(State &state, const Test &test, const PackDesc &packDesc, TxStandard testStd, TxStandard useStd);
void BenchCompleted(const State &state, Printer &printer, const Test &test, const PackDesc &packDesc,
                    TxStandard testStd, TxStandard useStd);

const auto TxStd2Letter = LibauthTestingSetup::TxStd2Letter;

// This function adds LibAuth benches at runtime based on compiled-in JSON from the compiled-in Libauth test vectors.
void registerBenches(const std::string &arg) {
    // The below detects benchmarks from the LibAuth JSON and adds them to the static BenchRunner::benchmarks() map.
    //
    // Assumption here: the test packs and their data have stable lifetimes outliving the benchmark runner, so that
    // references to e.g. TestVector::Test instances in the pack are stable and long-lived. This assumption currently
    // is true but if it changes, this code would need to be updated.
    //
    LibauthTestingSetup::LoadAllTestPacks(0);
    bool runAll = false;
    if (arg == "all") {
        runAll = true;
    } else if (arg == "all_slow") {
        runAll = true;
        simulatedBlockSize = 1'000'000u;
    } else if (arg == "slow") {
        simulatedBlockSize = 1'000'000u;
    } else if (arg == "") {
    } else {
        throw std::runtime_error(strprintf("Unsupported arg: -libauth=%s", arg));
    }
    for (const auto &packDesc : packsToRun) {
        auto *pack = LibauthTestingSetup::GetTestPack(packDesc.name);
        assert(pack != nullptr);
        const auto GetEvalModesForTestStandardness = [](const TxStandard testStd) {
            // The only time we run both "standard" and "nonstandard" modes is for "standard" tests,
            // otherwise always just do 1 "nonstandard" run for "invalid" and "nonstandard" tests.
            static const std::array<TxStandard, 2> bothModes{{TxStandard::STANDARD, TxStandard::NONSTANDARD}};
            auto ret = Span{bothModes};
            if (testStd != TxStandard::STANDARD) ret.pop_front(); // pop STANDARD, return 1 item: NONSTANDARD
            return ret;
        };
        if (!pack->benchmarkVectors.empty() || runAll) {
            // Count how many benches total (use by MkName() below to pad with proper number of 0's).
            // Note: We run each *standard* test using "standard" *and* "nonstandard" mode
            //       We run "invalid" and "nonstandard" tests in "nonstandard" mode only.
            size_t numBenches = 0;
            if (runAll) {
                for (const auto &testVec : pack->testVectors) {
                    numBenches += GetEvalModesForTestStandardness(testVec.standardness).size() * testVec.vec.size();
                }
            } else {
                for (const size_t idx : pack->benchmarkVectors) {
                    const auto &testVec = pack->testVectors.at(idx);
                    numBenches += GetEvalModesForTestStandardness(testVec.standardness).size() * testVec.benchmarks.size();
                }
            }
            assert(numBenches > 0u);
            // add baseline first
            const LibauthTestingSetup::TestVector::Test *baselineAdded = nullptr;
            size_t addCt = 0;
            auto MkName = [&addCt, numBenches](const std::string &ident) {
                const int padding = std::log10(numBenches) + 1; // dynamically determine how many 0's to pad with
                return strprintf("%0*d_%s", padding, addCt, ident);
            };
            if (const auto &baseline = pack->baselineBenchmark) {
                auto &testVec = pack->testVectors.at(baseline->first);
                const auto txStd = testVec.standardness;
                auto &test = testVec.vec.at(baseline->second);
                assert(test.benchmark && test.baselineBench);
                benchmark::BenchRunner( // implicitly adds to benchmarks map
                    // Name
                    "LibAuth_" + packDesc.name + "_" + MkName(test.ident) + "_baseline", // We must ensure this sorts first!
                    // Runner
                    [&test, txStd, &packDesc](State &state) {
                        RunBench(state, test, packDesc, txStd, txStd);
                    },
                    // Number of iterations is based off the txSize
                    GetIters(test.txSize),
                    // Completion
                    [&test, txStd, &packDesc](const State &s, Printer &p) {
                        BenchCompleted(s, p, test, packDesc, txStd, txStd);
                    },
                    // resuseChain = true for faster evals
                    true
                );
                baselineAdded = &test;
                ++addCt;
            } else {
                throw std::runtime_error(strprintf("Missing [baseline] test for pack %s", packDesc.name));
            }
            assert(baselineAdded != nullptr);
            // next add everything but baseline
            auto AddTest = [&](const Test &test, const TxStandard testStd) {
                for (const auto useStd : GetEvalModesForTestStandardness(testStd)) {
                    benchmark::BenchRunner( // implicitly adds to benchmarks map
                        // Name
                        "LibAuth_" + packDesc.name + "_" + MkName(test.ident)
                            + strprintf("_%s_%s", TxStd2Letter(testStd), TxStd2Letter(useStd)),
                        // Runner
                        [&test, testStd, useStd, &packDesc](State &state) {
                            RunBench(state, test, packDesc, testStd, useStd);
                        },
                        // Number of iterations is based off the txSize
                        GetIters(test.txSize),
                        // Completion
                        [&test, testStd, useStd, &packDesc](const State &s, Printer &p) {
                            BenchCompleted(s, p, test, packDesc, testStd, useStd);
                        },
                        // resuseChain = true for faster evals
                        true
                        );
                    ++addCt;
                }
            };
            if (runAll) {
                // "all" mode: -libauth=all
                for (const auto &testVec : pack->testVectors) {
                    const auto testStd = testVec.standardness;
                    for (const auto &test : testVec.vec) {
                        if (&test == baselineAdded) continue;
                        AddTest(test, testStd);
                    }
                }
            } else {
                // "benchmarks only" mode: -libauth
                for (const size_t idx : pack->benchmarkVectors) {
                    const auto &testVec = pack->testVectors.at(idx);
                    const auto testStd = testVec.standardness;
                    for (const size_t tidx : testVec.benchmarks) {
                        const auto &test = testVec.vec.at(tidx);
                        if (&test == baselineAdded) continue;
                        assert(test.benchmark);
                        AddTest(test, testStd);
                    }
                }
            }
        }
    }
}

// Keep track of which scripts unexpectedly returned "false", but we tolerated. Keyed off of state.m_name entries.
std::map<std::string, ScriptError> failures;

// This gets called once for each benchmark evaluation.
void RunBench(State &state, const Test &test, const PackDesc &packDesc, const TxStandard testStd, const TxStandard useStd) {
    // We cache the setup for each evaluation to save cycles in the overall runtime of the benchmark(s)
    struct CacheData {
        CCoinsView coinsDummy;
        CCoinsViewCache coinsCache{&coinsDummy};
        std::vector<ScriptExecutionContext> contexts;
        std::optional<PrecomputedTransactionData> precomputedTxDataOpt; // used only if really checking sigs
        std::vector<std::unique_ptr<BaseSignatureChecker>> txSigCheckers; // either fake or real signature checker, 1 per txin
        uint32_t scriptFlags{};
    };

    using CacheDataOpt = std::optional<CacheData>;

    using MapKey = std::tuple<std::string, std::string, TxStandard, TxStandard>;
    static std::map<MapKey, CacheDataOpt> cache;

    const CTransaction &txn = *test.tx;
    auto &cdata = cache[{test.ident, packDesc.name, testStd, useStd}];

    if (!cdata) {
        // Save coin data
        cdata.emplace();
        CCoinsViewCache &coinsCache = cdata->coinsCache;
        for (const auto & [outpt, entry] : test.inputCoins) {
            coinsCache.AddCoin(outpt, entry.coin, false);
        }

        auto &contexts = cdata->contexts = ScriptExecutionContext::createForAllInputs(txn, coinsCache);
        auto &precomputedTxDataOpt = cdata->precomputedTxDataOpt; // used only if really checking sigs
        auto &txSigCheckers = cdata->txSigCheckers; // either fake or real signature checker, 1 per txin
        precomputedTxDataOpt.emplace(contexts.at(0));
        for (const auto &ctx : contexts) {
            txSigCheckers.push_back(std::make_unique<TransactionSignatureChecker>(ctx, *precomputedTxDataOpt));
        }
        cdata->scriptFlags = [&] {
            const CBlockIndex *tip = WITH_LOCK(cs_main, return ::ChainActive().Tip());
            const bool requireStandard = useStd == TxStandard::STANDARD;
            uint32_t blockFlags{};
            const uint32_t standardFlags = GetMemPoolScriptFlags(::GetConfig().GetChainParams().GetConsensus(), tip, &blockFlags);
            uint32_t ret = requireStandard ? standardFlags : blockFlags;
            // Flag overrides
            if (requireStandard) {
                ret |= packDesc.flags.std;
                ret &= ~packDesc.flags.excludeStd;
            } else {
                ret |= packDesc.flags.nonStd;
                ret &= ~packDesc.flags.excludeNonStd;
            }
            return ret;
        }();
    }
    assert(txn.vin.size() == test.inputCoins.size());
    assert(txn.vin.size() == cdata->txSigCheckers.size());
    assert(txn.vin.size() == cdata->contexts.size());

    bool didFail = failures.find(state.GetName()) != failures.end();

    // Finally, after everything is set up ahead of time, run the benchmark loop
    BENCHMARK_LOOP {
        for (size_t size = txn.vin.size(), inputNum = 0; inputNum < size; ++inputNum) {
            const ScriptExecutionContext &context = cdata->contexts[inputNum];
            const BaseSignatureChecker &checker = *cdata->txSigCheckers[inputNum];
            const uint32_t scriptFlags = cdata->scriptFlags;
            ScriptError serror{};
            const bool ok = VerifyScript(context.scriptSig(), context.coinScriptPubKey(), scriptFlags, checker, &serror);
            if (!ok && !didFail) {
                didFail = true;
                const bool didInsert = failures.try_emplace(state.GetName(), serror).second;
                assert(didInsert);
            }
        }
    }
}

// Completion function called once after each LibAuth bench completes all evaluations; pushes supplemental stats to be
// printed in a table at the end.
void BenchCompleted(const State &state, Printer &printer, const Test &test, const PackDesc &packDesc,
                    TxStandard testStd, TxStandard useStd) {
    struct Cost {
        size_t txSize;
        double perIter;
    };
    static std::map<std::string, Cost> packBaselines;
    assert(state.m_num_iters); // cannot proceed if no iters!
    assert(state.GetResults().size()); // cannot proceed if no results!
    const double costPerEval = state.GetTotal() / state.GetResults().size();
    const Cost cost{test.txSize, costPerEval / state.GetNumIters()};
    const Cost *baseline{};
    if (test.baselineBench) {
        baseline = &(packBaselines[packDesc.name] = cost); // save baseline (should be first one seen)
    } else if (auto it = packBaselines.find(packDesc.name); it != packBaselines.end()) {
        baseline = &it->second;
    }
    if (!baseline) {
        // cannot do anything without the baseline result!
        std::cerr << "WARNING: [baseline] was not executed for pack \"" << packDesc.name << "\""
                  << ", skipping extended results for \"" << state.GetName() << "\"\n";
        return;
    }
    const double relCost = cost.perIter / baseline->perIter;
    const double errorPct = 100.0 * (state.GetMax() - state.GetMin()) / state.GetMedian();
    auto MkDesc = [](std::string s) {
        // Keep everything after the first ':' char, trim the resulting string, then wrap the result in quotes.
        std::vector<std::string> parts;
        Split(parts, s, ":", true);
        if (!parts.empty()) {
            parts.erase(parts.begin());
            s = Join(parts, ":");
        }
        return "\"" + TrimString(s) + "\"";
    };
    const std::string failStr = [&state]() -> std::string {
        if (auto it = failures.find(state.GetName()); it != failures.end())
            return "\"" + std::string{ScriptErrorString(it->second)} + "\"";
        return "\"OK\"";
    }();
    printer.appendExtraDataForCategory("LibAuth", {{
        {"ID", test.ident},
        {"TxByteLen", strprintf("%i", cost.txSize)},
        {"RelCost", strprintf("%1.3f", relCost)},
        {"RelCostPerByte", strprintf("%1.6f", relCost * baseline->txSize / test.txSize)},
        {"Hz", strprintf("%1.1f", 1.0 / cost.perIter)},
        {"AvgTimeNSec", strprintf("%i", int64_t(cost.perIter * 1e9))},
        {"VariancePct", strprintf("%1.1f", errorPct)},
        {"Samples", strprintf("%i", state.GetResults().size() * state.GetNumIters())},
        {"TestPack", packDesc.name},
        {"OrigStd", strprintf("%s", TxStd2Letter(testStd))},
        {"UsedStd", strprintf("%s", TxStd2Letter(useStd))},
        {"ErrMsg", strprintf("%s", failStr)},
        {"Description", MkDesc(test.description)},
    }});
}

} // namespace

void EnableLibAuthBenches(const std::string &arg) {
    static std::once_flag flg;
    std::call_once(flg, registerBenches, arg);
}
