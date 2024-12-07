// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/libauth_testing_setup.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <fs.h>
#include <policy/policy.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <script/script_metrics.h>
#include <script/sigcache.h>
#include <span.h>
#include <streams.h>
#include <txmempool.h>
#include <util/defer.h>
#include <validation.h>

#include <test/data/libauth_test_vectors.json.h>
#include <test/data/libauth_expected_test_fail_reasons.json.h>
#include <test/data/libauth_expected_test_metrics.json.h>
#include <test/jsonutil.h>
#include <test/util.h>

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

/* static */
std::map<std::string, LibauthTestingSetup::TestPack> LibauthTestingSetup::allTestPacks;
LibauthTestingSetup::ReasonsMap LibauthTestingSetup::expectedReasons;
LibauthTestingSetup::ReasonsMap LibauthTestingSetup::newReasons;
LibauthTestingSetup::MetricsMap LibauthTestingSetup::metricsMap;
size_t LibauthTestingSetup::metricsMapNewCt = 0u;

/* static */
void LibauthTestingSetup::LoadAllTestPacks(std::optional<unsigned> optCoinHeights) {
    if (!allTestPacks.empty()) return;

    auto UncompressJson = [](Span<const uint8_t> bytes, const size_t uncompSz) {
        return read_json(UncompressStr(bytes, uncompSz));
    };

    const auto testPacksUV = UncompressJson(json_tests::libauth_test_vectors,
                                            json_tests::libauth_test_vectors_uncompressed_size);
    const auto reasonsJson = UncompressJson(json_tests::libauth_expected_test_fail_reasons,
                                            json_tests::libauth_expected_test_fail_reasons_uncompressed_size);
    const auto metricsJson = UncompressJson(json_tests::libauth_expected_test_metrics,
                                            json_tests::libauth_expected_test_metrics_uncompressed_size);

    // Load in the BCHN error message lookup table
    // Format: [ ["packName", "ident", "testStandardness", "evalStandardness", "reason"], ... ]
    //
    expectedReasons.clear();
    newReasons.clear();
    for (const auto &item : reasonsJson) {
        BOOST_REQUIRE(item.isArray());
        const auto &arr = item.get_array();
        BOOST_REQUIRE(arr.size() >= 5);
        TestRunKey key;
        key.packName = arr.at(0).get_str();
        key.ident = arr.at(1).get_str();
        key.testStd = Letter2TxStd(arr.at(2).get_str());
        key.evalStd = Letter2TxStd(arr.at(3).get_str());
        const auto & [it, inserted] = expectedReasons.try_emplace(std::move(key), arr.at(4).get_str());
        if (!inserted) {
            BOOST_TEST_ERROR(strprintf("Dupe expected reason: (%s, %s, %s, %s)",
                                       it->first.packName, it->first.ident, arr.at(2).get_str(), arr.at(3).get_str()));
        }
    }

    // Load the BCHN expected test metrics data
    metricsMap.clear();
    metricsMapNewCt = 0;
    for (const auto &uv : metricsJson) {
        BOOST_REQUIRE(uv.isArray());
        const auto &arr = uv.get_array();
        TestRunKey key;
        key.packName = arr.at(0).get_str();
        key.ident = arr.at(1).get_str();
        key.testStd = Letter2TxStd(arr.at(2).get_str());
        key.evalStd = Letter2TxStd(arr.at(3).get_str());
        std::vector<Metrics> metrics;
        for (const auto &uv2 : arr.at(4).get_array()) {
            metrics.push_back(Metrics::fromUniValue(uv2.get_array()));
        }
        metricsMap.try_emplace(std::move(key), std::move(metrics));
    }

    // Load the test pack vectors
    BOOST_CHECK( ! testPacksUV.empty());
    unsigned coinHeights = optCoinHeights ? *optCoinHeights : []{
        LOCK(cs_main);
        return ::ChainActive().Tip()->nHeight;
    }();
    for (auto &pack : testPacksUV) {
        BOOST_CHECK(pack.isObject());
        if (pack.isObject()) {
            auto &packObj = pack.get_obj();
            auto *nameVal = packObj.locate("name");
            BOOST_CHECK(nameVal != nullptr);
            if (nameVal) {
                TestPack testPack;
                testPack.name = nameVal->get_str();
                std::vector<TestVector> &packVec = testPack.testVectors; // append to this member variable of testPack
                for (const auto &uv : packObj.at("tests").get_array()) {
                    BOOST_CHECK(uv.isObject());
                    if (uv.isObject()) {
                        auto &uvObj = uv.get_obj();
                        auto *testNameVal = uvObj.locate("name");
                        BOOST_CHECK(testNameVal != nullptr);
                        if (testNameVal) {
                            std::string testName = testNameVal->get_str();
                            const std::string &standardnessStr = testName;
                            BOOST_CHECK(standardnessStr == "invalid" ||
                                        standardnessStr == "nonstandard" ||
                                        standardnessStr == "standard" );
                            TxStandard testStandardness = INVALID;
                            if (standardnessStr == "nonstandard") {
                                testStandardness = NONSTANDARD;
                            } else if (standardnessStr == "standard") {
                                testStandardness = STANDARD;
                            }
                            std::string descStdString = "fail validation in both nonstandard and standard mode";
                            if (testStandardness == NONSTANDARD) {
                                descStdString = "fail validation in standard mode but pass validation in nonstandard mode";
                            } else if (testStandardness == STANDARD) {
                                descStdString = "pass validation in both standard and nonstandard mode";
                            }
                            std::string testDescription =  "Test vectors that must " + descStdString;
                            TestVector testVec;
                            testVec.name = testName;
                            testVec.description = testDescription;
                            testVec.standardness = testStandardness;

                            // Read the "scriptonly" list and put into a set (may be empty)
                            std::set<std::string> scriptOnlyOverrides;
                            for (const auto &identUV : uvObj.at("scriptonly").get_array()) {
                                scriptOnlyOverrides.insert(identUV.get_str());
                            }
                            for (const auto &t : uvObj.at("tests").get_array()) {
                                const UniValue::Array &vec = t.get_array();
                                BOOST_CHECK_GE(vec.size(), 6);
                                TestVector::Test test;
                                test.ident = vec.at(0).get_str();
                                test.description = vec.at(1).get_str();
                                test.stackAsm = vec.at(2).get_str();
                                test.scriptAsm = vec.at(3).get_str();
                                test.scriptOnly = scriptOnlyOverrides.count(test.ident);
                                if (vec.size() >= 7) {
                                    // for scriptOnly
                                    test.inputNum = vec.at(6).get_int();
                                }
                                test.benchmark = test.description.find("validation benchmarks:") != std::string::npos;
                                test.baselineBench = test.benchmark && test.description.find("[baseline]") != std::string::npos;

                                CMutableTransaction mtx;
                                BOOST_CHECK(DecodeHexTx(mtx, vec.at(4).get_str()));
                                test.tx = MakeTransactionRef(std::move(mtx));
                                BOOST_REQUIRE(test.inputNum < test.tx->vin.size());
                                const auto serinputs = ParseHex(vec.at(5).get_str());
                                std::vector<CTxOut> utxos;
                                {
                                    VectorReader vr(SER_NETWORK, INIT_PROTO_VERSION, serinputs, 0);
                                    vr >> utxos;
                                    BOOST_CHECK(vr.empty());
                                }
                                BOOST_CHECK_EQUAL(utxos.size(), test.tx->vin.size());
                                std::string skipReason;
                                for (size_t i = 0; i < utxos.size(); ++i) {
                                    auto [it, inserted] = test.inputCoins.emplace(std::piecewise_construct,
                                                                                  std::forward_as_tuple(test.tx->vin[i].prevout),
                                                                                  std::forward_as_tuple(Coin(utxos[i],
                                                                                                             coinHeights, false)));
                                    it->second.flags = CCoinsCacheEntry::FRESH;
                                    if (!inserted) {
                                        skipReason += strprintf("\n- Skipping bad tx due to dupe input Input[%i]: %s, Coin1: %s,"
                                                                " Coin2: %s\n%s",
                                                                i, it->first.ToString(true),
                                                                it->second.coin.GetTxOut().ToString(true),
                                                                utxos[i].ToString(true), test.tx->ToString(true));
                                    }
                                    BOOST_CHECK(!it->second.coin.IsSpent());
                                }
                                test.txSize = ::GetSerializeSize(*test.tx);
                                if ( ! skipReason.empty()) {
                                    BOOST_WARN_MESSAGE(false, strprintf("Skipping test \"%s\": %s", test.ident, skipReason));
                                } else {
                                    if (test.benchmark) {
                                        testVec.benchmarks.push_back(testVec.vec.size());
                                        if (test.baselineBench) {
                                            testVec.baselineBench = testVec.benchmarks.back();
                                        }
                                    }
                                    testVec.vec.push_back(std::move(test));
                                }
                            }
                            if (!testVec.benchmarks.empty()) {
                                testPack.benchmarkVectors.push_back(packVec.size());
                                if (!testPack.baselineBenchmark && testVec.baselineBench) {
                                    testPack.baselineBenchmark.emplace(testPack.benchmarkVectors.back(), *testVec.baselineBench);
                                }
                            }
                            packVec.push_back(std::move(testVec));
                        }
                    }
                }
                allTestPacks[testPack.name] = std::move(testPack);
            }
        }
    }
    BOOST_CHECK( ! allTestPacks.empty());
}

/* static */
std::optional<std::string> LibauthTestingSetup::LookupExpectedReason(const TestRunKey &k) {
    if (auto it = expectedReasons.find(k); it != expectedReasons.end()) return it->second;
    return std::nullopt;
}

/* static */
void LibauthTestingSetup::GotUnexpectedReason(const TestRunKey &k, const std::string &reason) {
    const auto &[it, inserted] = newReasons.try_emplace(k, reason);
    if (!inserted && reason != it->second) {
        throw std::runtime_error(strprintf("%s: Failed to insert a new reason (%s, %s, %s, %s) -> %s -- newReasons map"
                                           " already has reason: %s!", __func__, k.packName, k.ident,
                                           TxStd2Letter(k.testStd), TxStd2Letter(k.evalStd), reason, it->second));
    }
}

/* static */
const std::vector<LibauthTestingSetup::Metrics> *LibauthTestingSetup::LookupExpectedMetrics(const TestRunKey &k) {
    if (auto it = metricsMap.find(k); it != metricsMap.end()) {
        return &it->second;
    }
    return nullptr;
}

/* static */
void LibauthTestingSetup::GotUnexpectedMetrics(const TestRunKey &k, const std::vector<Metrics> &metrics) {
    if (metrics.empty()) {
        metricsMap.erase(k);
    } else {
        metricsMap[k] = metrics;
    }
    ++metricsMapNewCt;
}

/* static */
bool LibauthTestingSetup::RunScriptOnlyTest(const TestVector::Test &tv, bool standard, CValidationState &state,
                                            Metrics *metricsOut, bool skipChecks, const TransactionSignatureChecker *checker) {
    AssertLockHeld(cs_main);
    const uint32_t flags = [&] {
        uint32_t blockFlags{};
        uint32_t stdFlags = GetMemPoolScriptFlags(GetConfig().GetChainParams().GetConsensus(), ChainActive().Tip(),
                                                  &blockFlags);
        return standard ? stdFlags : blockFlags;
    }();
    state = {};
    if (standard && !skipChecks) {
        // If caller wants standardness, do rudimentary checks anyway even in "scriptonly" mode
        if (std::string reason; !IsStandardTx(*tv.tx, reason, flags)) {
            return state.Invalid(false, REJECT_NONSTANDARD, reason);
        }
        if (!AreInputsStandard(*tv.tx, *pcoinsTip, flags)) {
            return state.Invalid(false, REJECT_NONSTANDARD, "bad-txns-nonstandard-inputs");
        }
    }
    std::vector<ScriptExecutionContext> contexts;
    std::optional<PrecomputedTransactionData> optTxData;
    std::optional<CachingTransactionSignatureChecker> optChecker;
    const ScriptExecutionContext *context{};
    if (!checker) {
        // In this mode, we run the test versus the input specified by tv.inputNum
        contexts = ScriptExecutionContext::createForAllInputs(*tv.tx, *pcoinsTip);
        context = &contexts.at(tv.inputNum);
        optTxData.emplace(*context);
        checker = &optChecker.emplace(*context, true, *optTxData);
    } else {
        // In this mode, we rely on the passed-in checker to determine which input we run against, etc.
        context = checker->GetContext();
    }
    assert(context != nullptr && checker != nullptr);

    ScriptExecutionMetrics metrics;
    ScriptError serror{};
    const bool ret = VerifyScript(context->scriptSig(), context->coinScriptPubKey(), flags, *checker, metrics, &serror);
    if (metricsOut) {
        *metricsOut = Metrics::fromScriptMetrics(context->inputIndex(), metrics, flags, context->scriptSig().size());
    }
    if (!ret) {
        state.Invalid(false, REJECT_INVALID, ScriptErrorString(serror));
    }
    if (!skipChecks) {
        BOOST_TEST_MESSAGE(strprintf("\"%s\" *scriptonly* eval input number: %i, nSigChecks: %i, opCost: %i, hashIters: %i,"
                                     " result: %i, error: \"%s\"",
                                     tv.ident, tv.inputNum, metrics.GetSigChecks(), metrics.GetCompositeOpCost(flags),
                                     metrics.GetHashDigestIterations(), ret, state.GetRejectReason()));
    }
    return ret;
}

/* static */
void LibauthTestingSetup::RunTestVector(const TestVector &test, const std::string &packName) {
    const bool expectStd = test.standardness == STANDARD;
    const bool expectNonStd = test.standardness == STANDARD || test.standardness == NONSTANDARD;
    BOOST_TEST_MESSAGE(strprintf("Running test vectors \"%s\", description: \"%s\" ...", test.name, test.description));

    size_t num = 0;
    for (const auto &tv : test.vec) {
        ++num;
        const std::string scriptOnlyBlurb = tv.scriptOnly ? strprintf(" (scriptonly, input number %i)", tv.inputNum) : std::string{};
        BOOST_TEST_MESSAGE(strprintf("Executing \"%s\" test %i \"%s\": \"%s\", tx-size: %i, nInputs: %i%s ...\n",
                                     test.name, num, tv.ident, tv.description, ::GetSerializeSize(*tv.tx),
                                     tv.inputCoins.size(), scriptOnlyBlurb));
        Defer cleanup([&]{
            LOCK(cs_main);
            g_mempool.clear();
            for (auto & [outpt, _] : tv.inputCoins) {
                // clear utxo set of the temp coins we added for this tx
                pcoinsTip->SpendCoin(outpt);
            }
        });
        LOCK(cs_main);
        for (const auto &[outpt, entry] : tv.inputCoins) {
            // add each coin that the tx spends to the utxo set
            pcoinsTip->AddCoin(outpt, entry.coin, false);;
        }
        // First, do "standard" test; result should match `expectStd`
        ::fRequireStandard = true;
        CValidationState state;
        bool missingInputs{};
        bool ok1;
        std::vector<Metrics> metrics;

        auto RunEachInputAndGrabMetrics = [&metrics, &tv](CValidationState *stateOut = nullptr) {
            // Evaluate each input to grab the per-input metrics
            const size_t nIn = tv.tx->vin.size();
            // cache contexts and txdata for faster execution in the below loop
            auto const contexts = ScriptExecutionContext::createForAllInputs(*tv.tx, *pcoinsTip);
            assert(nIn > 0u && contexts.size() == nIn);
            PrecomputedTransactionData txdata{contexts[0]};
            metrics.resize(nIn);
            if (stateOut) *stateOut = {};
            for (size_t i = 0; i < nIn; ++i) {
                auto const &context = contexts[i];
                const CachingTransactionSignatureChecker checker{context, true, txdata};
                CValidationState state2{};
                const bool ok = RunScriptOnlyTest(tv, ::fRequireStandard, state2, &metrics[i], true, &checker);
                BOOST_CHECK(ok);
                if (!ok) {
                    if (stateOut) *stateOut = state2;
                    return false;
                }
            }
            return true;
        };

        if (tv.scriptOnly) {
            metrics.resize(1);
            ok1 = RunScriptOnlyTest(tv, ::fRequireStandard, state, &metrics[0]);
        } else {
            ok1 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                     true           /* bypass_limits (minfee, etc) */,
                                     Amount::zero() /* nAbsurdFee    */,
                                     false          /* test_accept   */);
            if (ok1) {
                // Alas, to grab the metrics, we must run each input individually
                BOOST_CHECK(RunEachInputAndGrabMetrics());
            }
        }
        std::string standardReason = state.GetRejectReason();
        std::string nonstandardReason{""};
        if (standardReason.empty() && !ok1 && missingInputs) standardReason = "Missing inputs";
        BOOST_CHECK_MESSAGE(ok1 == expectStd, strprintf("(standard) %s Wrong result. %s.", tv.ident,
                                                        expectStd ? strprintf("Pass expected, test failed (%s)", standardReason)
                                                                  : "Fail expected, test passed"));

        auto DoMetricsCheck = [&metrics](const TestRunKey &k) {
            const std::vector<Metrics> *expected = LookupExpectedMetrics(k);
            const auto MakeMsg = [&k] {
                return strprintf("New or unexpected  metrics for: (%s, %s, %s, %s)",
                                 k.packName, k.ident, TxStd2Letter(k.testStd), TxStd2Letter(k.evalStd));
            };
            if (expected == nullptr && !metrics.empty()) {
                // Tolerate
                BOOST_TEST_WARN(false, MakeMsg());
                GotUnexpectedMetrics(k, metrics);
            } else if (expected && *expected != metrics) {
                // Require they match if we had an expected metrics
                BOOST_CHECK_MESSAGE(false, MakeMsg());
                GotUnexpectedMetrics(k, metrics);
            }
        };

        auto DoExpectedReasonCheck = [](const TestRunKey &k, const std::string &reason, const std::string &stdStr) {
            // warn if reasons differ
            if (auto expectedReason = LookupExpectedReason(k); expectedReason != reason) {
                const auto msg = strprintf("New or unexpected '%s' reason for: (%s, %s, %s, %s): got: '%s', expected: '%s'",
                                           stdStr, k.packName, k.ident, TxStd2Letter(k.testStd), TxStd2Letter(k.evalStd),
                                           reason, expectedReason.value_or("(*nothing*)"));
                if (expectedReason.has_value()) {
                    // Require they match if we had an expected reason
                    BOOST_CHECK_MESSAGE(false, msg);
                } else {
                    // Tolerate us missing reasons for a particular test (may be a newly imported set of tests from libauth...)
                    BOOST_TEST_WARN(false, msg);
                }
                GotUnexpectedReason(k, reason);
            }
        };

        if (const TestRunKey k{packName, tv.ident, test.standardness, STANDARD}; ok1) {
            DoMetricsCheck(k);
        } else {
            DoExpectedReasonCheck(k, standardReason, "standard");
        }
        metrics.clear();

        bool ok2 = expectNonStd;
        if (!expectStd) {
            // Next, do "nonstandard" test but only if `!expectStd`; result should match `expectNonStd`
            state = {};
            missingInputs = false;
            ::fRequireStandard = false;
            if (tv.scriptOnly) {
                metrics.resize(1);
                ok2 = RunScriptOnlyTest(tv, ::fRequireStandard, state, &metrics[0]);
            } else {
                WITH_LOCK(cs_main, g_mempool.clear()); // just in case above already added it
                ok2 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                         true           /* bypass_limits (minfee, etc) */,
                                         Amount::zero() /* nAbsurdFee    */,
                                         false          /* test_accept   */);
                if (!ok2 && expectNonStd && state.GetRejectReason().find("non-mandatory-script-verify-flag") != std::string::npos) {
                    // The mempool rejected this txn but it failed for a "non-mandatory-script-verify-flag" reason.
                    // Try again with each input individually. Background: ATMP is weird and it rejects non-standard
                    // txns that *would be ok* as block txns, even *if* fRequireStandard is set to false!
                    ok2 = RunEachInputAndGrabMetrics(&state);
                } else if (ok2) {
                    // Alas, to grab the metrics, we must run each input individually
                    BOOST_CHECK(RunEachInputAndGrabMetrics());
                }
            }
            nonstandardReason = state.GetRejectReason();
            if (nonstandardReason.empty() && !ok2 && missingInputs) nonstandardReason = "Missing inputs";
            BOOST_CHECK_MESSAGE(ok2 == expectNonStd,
                                strprintf("(nonstandard) %s Wrong result. %s.", tv.ident,
                                          expectNonStd ? strprintf("Pass expected, test failed (%s)", nonstandardReason)
                                                       : "Fail expected, test passed"));

            if (const TestRunKey k{packName, tv.ident, test.standardness, NONSTANDARD}; ok2) {
                DoMetricsCheck(k);
            } else {
                DoExpectedReasonCheck(k, nonstandardReason, "nonstandard");
            }
        }
        if (ok1 != expectStd || ok2 != expectNonStd) {
            auto &tx = tv.tx;
            BOOST_TEST_MESSAGE(strprintf("TxId %s for test \"%s\" details:", tx->GetId().ToString(), tv.ident));
            size_t i = 0;
            for (auto &inp : tx->vin) {
                const CTxOut &txout = pcoinsTip->AccessCoin(inp.prevout).GetTxOut();
                BOOST_TEST_MESSAGE(strprintf("Input %i: %s, coin = %s", i, inp.prevout.ToString(true), txout.ToString(true)));
                ++i;
            }
            i = 0;
            for (auto &outp : tx->vout) {
                BOOST_TEST_MESSAGE(strprintf("Output %i: %s", i, outp.ToString(true)));
                ++i;
            }
        }
    }
}

const char *LibauthTestingSetup::TxStd2Letter(TxStandard std) {
    switch (std) {
        case STANDARD: return "S";
        case NONSTANDARD: return "N";
        case INVALID: return "I";
    }
    return "?";
}

LibauthTestingSetup::TxStandard LibauthTestingSetup::Letter2TxStd(std::string_view letter) {
    if (letter == "S") return STANDARD;
    if (letter == "N") return NONSTANDARD;
    if (letter == "I") return INVALID;
    throw std::invalid_argument(strprintf("%s: Unknown TxStandard abbrev.: \"%s\"", __func__, std::string{letter}));
}


LibauthTestingSetup::LibauthTestingSetup()
    : saved_fRequireStandard{::fRequireStandard}
{}

LibauthTestingSetup::~LibauthTestingSetup() {
    // restore original fRequireStandard flag since the testing setup definitely touched this flag
    ::fRequireStandard = saved_fRequireStandard;
}

/* static */
const LibauthTestingSetup::TestPack *LibauthTestingSetup::GetTestPack(const std::string &packName) {
    LoadAllTestPacks();
    const auto it = std::as_const(allTestPacks).find(packName);
    if (it != allTestPacks.cend()) {
        return &it->second;
    }
    return nullptr;
}

void LibauthTestingSetup::RunTestPack(const std::string &packName) {
    if (auto *ppack = GetTestPack(packName)) {
        const TestPack &pack = *ppack;
        BOOST_CHECK_EQUAL(packName, pack.name); // paranoia, should always match
        BOOST_TEST_MESSAGE(strprintf("----- Running '%s' tests -----", packName));
        for (const TestVector &testVector : pack.testVectors) {
            RunTestVector(testVector, packName);
        }
    } else {
        // fail if test vectors for `packName` are not found
        BOOST_CHECK_MESSAGE(false, strprintf("No tests found for '%s'!", packName));
    }
}

static std::string StringifyArrayCompact(const UniValue::Array &arr) {
    const std::string NL{"\n"}, commaNL = "," + NL;
    std::string ret = "[\n";
    size_t i{};
    const size_t sz = arr.size();
    for (const auto &item : arr) {
        ret += UniValue::stringify(item);
        if (++i < sz) {
            ret += commaNL; // not last item, has comma
        } else {
            ret += NL; // last item, no comma
        }
    }
    ret += "]\n";
    return ret;
}

/* static */
void LibauthTestingSetup::ProcessExpectedReasonsTable() {
    if (newReasons.empty()) {
        return;
    }
    UniValue::Array outputJson;

    std::set<TestRunKey> alreadyAdded;
    for (const auto &[k, r] : expectedReasons) {
        std::string reason;
        if (auto it = newReasons.find(k); it != newReasons.end()) {
            reason = it->second;
        } else {
            reason = r;
        }
        UniValue::Array item;
        item.reserve(5);
        item.emplace_back(k.packName);
        item.emplace_back(k.ident);
        item.emplace_back(std::string{TxStd2Letter(k.testStd)});
        item.emplace_back(std::string{TxStd2Letter(k.evalStd)});
        item.emplace_back(std::move(reason));
        outputJson.emplace_back(std::move(item));
        alreadyAdded.insert(k);
    }
    for (const auto &[k, r] : newReasons) {
        if (alreadyAdded.count(k)) continue;
        UniValue::Array item;
        item.reserve(5);
        item.emplace_back(k.packName);
        item.emplace_back(k.ident);
        item.emplace_back(std::string{TxStd2Letter(k.testStd)});
        item.emplace_back(std::string{TxStd2Letter(k.evalStd)});
        item.emplace_back(r);
        outputJson.emplace_back(std::move(item));
        alreadyAdded.insert(k);
    }
    assert(!outputJson.empty());

    const std::string path{"./libauth_expected_test_fail_reasons.json"};
    const std::string jsonOut = StringifyArrayCompact(outputJson);
    const auto msg = "Some unexpected test failure reasons occurred; saving an updated BCHN error message lookup table"
                     " to: \"" + path + "\". You may inspect this file and if it seems ok, copy it to src/test/data/"
                     " and rebuild test_bitcoin to fix this warning.";
    // output to both stdout and boost logs, to ensure user sees this.
    std::cerr << msg << "\n";
    BOOST_TEST_WARN(false, msg);
    fs::path filePath{path};
    CAutoFile file(fsbridge::fopen(fs::path{filePath}, "wt"), 0, 0);
    if (!file.IsNull()) {
        BOOST_CHECK_MESSAGE(std::fwrite(jsonOut.data(), 1, jsonOut.size(), file.Get()) == jsonOut.size(),
                            "Error writing to file: " << path);
    } else {
        BOOST_TEST_ERROR("Can't open output file: " << path);
    }
}

/* static */
void LibauthTestingSetup::ProcessExpectedMetricsTable() {
    if (!metricsMapNewCt) return;

    UniValue::Array outputJson;

    for (const auto & [k, metrics] : metricsMap) {
        if (metrics.empty()) continue;
        UniValue::Array item;
        item.reserve(5u);
        item.emplace_back(k.packName);
        item.emplace_back(k.ident);
        item.emplace_back(std::string{TxStd2Letter(k.testStd)});
        item.emplace_back(std::string{TxStd2Letter(k.evalStd)});
        UniValue::Array arr;
        arr.reserve(metrics.size());
        for (const auto &metric : metrics) {
            arr.push_back(metric.toUniValue());
        }
        item.emplace_back(std::move(arr));
        outputJson.emplace_back(std::move(item));
    }

    assert(!outputJson.empty());

    const std::string path{"./libauth_expected_test_metrics.json"};
    const std::string jsonOut = StringifyArrayCompact(outputJson);
    const auto msg = "Some unexpected test metrics occurred; saving an updated BCHN test metrics table to: \""
                     + path + "\". You may inspect this file and if it seems ok, copy it to src/test/data/ and"
                     " rebuild test_bitcoin to fix this warning.";
    // output to both stdout and boost logs, to ensure user sees this.
    std::cerr << msg << "\n";
    BOOST_TEST_WARN(false, msg);
    fs::path filePath{path};
    CAutoFile file(fsbridge::fopen(fs::path{filePath}, "wt"), 0, 0);
    if (!file.IsNull()) {
        BOOST_CHECK_MESSAGE(std::fwrite(jsonOut.data(), 1, jsonOut.size(), file.Get()) == jsonOut.size(),
                            "Error writing to file: " << path);
    } else {
        BOOST_TEST_ERROR("Can't open output file: " << path);
    }
}

UniValue::Array LibauthTestingSetup::Metrics::toUniValue() const {
    UniValue::Array ret;
    ret.reserve(7);
    ret.emplace_back(inputNum);
    ret.emplace_back(opCost);
    if (opCostLimit > -1) {
        ret.emplace_back(opCostLimit);
    } else {
        ret.emplace_back(UniValue{});
    }
    ret.emplace_back(hashIters);
    if (hashItersLimit > -1) {
        ret.emplace_back(hashItersLimit);
    } else {
        ret.emplace_back(UniValue{});
    }
    ret.emplace_back(sigChecks);
    if (sigChecksLimit > -1) {
        ret.emplace_back(sigChecksLimit);
    } else {
        ret.emplace_back(UniValue{});
    }
    return ret;
}

/* static */
LibauthTestingSetup::Metrics
LibauthTestingSetup::Metrics::fromUniValue(const UniValue::Array &uv) {
    Metrics ret;
    ret.inputNum = uv.at(0).get_int();
    ret.opCost = uv.at(1).get_int64();
    if (const auto &item = uv.at(2); item.isNull()) {
        ret.opCostLimit = -1;
    } else {
        ret.opCostLimit = item.get_int64();
    }
    ret.hashIters = uv.at(3).get_int64();
    if (const auto &item = uv.at(4); item.isNull()) {
        ret.hashItersLimit = -1;
    } else {
        ret.hashItersLimit = item.get_int64();
    }
    ret.sigChecks = uv.at(5).get_int64();
    if (const auto &item = uv.at(6); item.isNull()) {
        ret.sigChecksLimit = -1;
    } else {
        ret.sigChecksLimit = item.get_int64();
    }
    return ret;
}

/* static */
LibauthTestingSetup::Metrics
LibauthTestingSetup::Metrics::fromScriptMetrics(unsigned inputNum, const ScriptExecutionMetrics &metrics, uint32_t flags,
                                                size_t scriptSigSize) {
    Metrics ret;

    ret.inputNum = inputNum;
    ret.sigChecks = metrics.GetSigChecks();
    ret.opCost = metrics.GetCompositeOpCost(flags);
    ret.hashIters = metrics.GetHashDigestIterations();

    if (auto *limits = metrics.GetScriptLimits(); limits && (flags & SCRIPT_ENABLE_MAY2025)) {
        ret.opCostLimit = limits->GetOpCostLimit();
        ret.hashItersLimit = limits->GetHashItersLimit();
    }
    if (flags & SCRIPT_VERIFY_INPUT_SIGCHECKS) {
        ret.sigChecksLimit = (scriptSigSize + 60) / 43;
    }

    return ret;
}
