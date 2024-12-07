// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <coins.h>
#include <primitives/transaction.h>
#include <test/setup_common.h>
#include <univalue.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

class CValidationState;
class ScriptExecutionMetrics;
class TransactionSignatureChecker;

/// Testing setup that:
/// - loads all of the json data for all of the libauth tests into a static structure (lazy load, upon first use)
/// - tracks if we overrode ::fRequireStandard, and resets it on test end
class LibauthTestingSetup : public TestChain100Setup {
public:
    enum TxStandard { INVALID, NONSTANDARD, STANDARD };

    struct TestVector {
        std::string name;
        std::string description;
        TxStandard standardness{}; // Which validation standard this test should meet

        struct Test {
            std::string ident;
            std::string description;
            std::string stackAsm;
            std::string scriptAsm;
            CTransactionRef tx;
            size_t txSize{};
            CCoinsMap inputCoins;
            bool scriptOnly = false; //< If true, this test should *not* test against AcceptToMemoryPool() for the
                                     //< whole txn, but should just evaluate the script for input `inputNum`.
            bool benchmark = false;  //< True if the test description contains the string " benchmark:"
            bool baselineBench = false; //< True if `benchmark==true` and the description contains "[baseline]"
            unsigned inputNum = 0;   //< The input number to test. Comes from the optional 7th column of the JSON array
                                     //< for this test, defaults to 0 if unspecified. Only used if scriptOnly == true.
        };

        std::vector<Test> vec;
        std::vector<size_t> benchmarks; // indices into above vector; all `Test`s that are also `benchmark==true`
        std::optional<size_t> baselineBench; // if set, index into the above vector for the first Test that is `baselineBench`
    };

    // Container for a group of test vectors, corresponds to a consensus year packname .e.g "2022", "2023", "2025, etc.
    struct TestPack {
        std::string name; //! Test pack name, same as the key in the `allTestPacks` map below
        std::vector<TestVector> testVectors;
        std::vector<size_t> benchmarkVectors; // indices into the above `testVectors` for all vectors that also have benchmarks in them
        // if set: the baseline benchmark; pair of: .first = index into testVectors, .second = index into testVector.vec.
        std::optional<std::pair<size_t, size_t>> baselineBenchmark;

        TestPack() = default;
    };

    // Utility that returns one of: "I", "S", "N"
    static const char *TxStd2Letter(TxStandard std);
    // Inverse of above. Throws if the "letter" arg is not one of "I", "S", "N"
    static TxStandard Letter2TxStd(std::string_view letter);

private:
    static std::map<std::string, TestPack> allTestPacks;  //! key: testPack.name, value: testPack

    // Uniquely identifies an individual test run vs standard or nonstandard eval rules.
    struct TestRunKey {
        std::string packName; // pack name e.g. "2023", etc
        std::string ident; // test identifier e.g. "skjac9"
        TxStandard testStd; // standardness setting for the test itself: may be INVALID, STANDARD, or NONSTANDARD
        TxStandard evalStd; // standardness setting for the evaluation, one of: STANDARD or NONSTANDARD

        auto toTupleRef() const { return std::tie(packName, ident, testStd, evalStd); }
        bool operator<(const TestRunKey &o) const { return toTupleRef() < o.toTupleRef(); }
        bool operator==(const TestRunKey &o) const { return toTupleRef() == o.toTupleRef(); }
    };

    // A structure to hold all BCHN failure reason messages for all tests for all test packs
    // Maps: TestRunKey -> "bchn-reason-string"
    using ReasonsMap = std::map<TestRunKey, std::string>;

    // All error messages that were expected and loaded from `libauth_expected_test_fail_reasons.json`.
    // This map is populated by `LoadAllTestPacks()`.
    static ReasonsMap expectedReasons;
    // All error messages actually produced that disagree with `expectedReasons`.
    // This map is populated per pack by running `RunTestPack()`.
    static ReasonsMap newReasons;

    // Returns the test fail reason from expectedReasons, for a particular test, or std::nullopt if no reason is known.
    static std::optional<std::string> LookupExpectedReason(const TestRunKey &k);

    // Registers an unexpected reason with the newReasons map, to be saved to a new `libauth_expected_test_fail_reasons.json`
    static void GotUnexpectedReason(const TestRunKey &k, const std::string &reason);

    // Keeps track of the op costs for individual inputs
    struct Metrics {
        int inputNum{};
        int64_t opCost{}, opCostLimit{-1}, hashIters{}, hashItersLimit{-1}, sigChecks{}, sigChecksLimit{-1};

        UniValue::Array toUniValue() const;
        static Metrics fromUniValue(const UniValue::Array &uv);
        static Metrics fromScriptMetrics(unsigned inputNum, const ScriptExecutionMetrics &metrics, uint32_t flags,
                                         size_t scriptSigSize);
        auto toTup() const {
            return std::tuple(inputNum, opCost, opCostLimit, hashIters, hashItersLimit, sigChecks, sigChecksLimit);
        }

        bool operator==(const Metrics &o) const { return toTup() == o.toTup(); }
        bool operator!=(const Metrics &o) const { return ! this->operator==(o); }
    };

    // Mapping of test run -> metrics for each input evaluated for that run. (Successful runs only).
    using MetricsMap = std::map<TestRunKey, std::vector<Metrics>>;
    static MetricsMap metricsMap;
    static size_t metricsMapNewCt;

    static const std::vector<Metrics> *LookupExpectedMetrics(const TestRunKey &k);
    static void GotUnexpectedMetrics(const TestRunKey &k, const std::vector<Metrics> &metrics);

    static void RunTestVector(const TestVector &test, const std::string &packName);
    static bool RunScriptOnlyTest(const TestVector::Test &tv, bool standard, CValidationState &state,
                                  Metrics *metricsOut = nullptr, bool skipChecks = false,
                                  const TransactionSignatureChecker *checker = nullptr);

    const bool saved_fRequireStandard;

public:
    LibauthTestingSetup();
    ~LibauthTestingSetup() override;

    /// Explicitly load all test packs, optionally specifying the height for all coins internally.
    static void LoadAllTestPacks(std::optional<unsigned> coinHeights = std::nullopt);

    /// Run all tests for the test pack named `name`.
    void RunTestPack(const std::string &name);

    /// Returns the TestPack named `name`, or `nullptr` if no such TestPack exists.
    static const TestPack *GetTestPack(const std::string &name);

    /// If new unexpected/mismatched reasons occurred, generate the reasons lookup JSON file, and print a message to
    /// the boost log and to stderr on the console. The generated file is a JSON file with the updated reasons, ready to
    /// be copied into the source tree. If all failure reasons were known and were not unexpected, does nothing.
    static void ProcessExpectedReasonsTable();
    /// Same as above, but do it for the expected metrics table and produce a JSON file if some metrics are missing.
    static void ProcessExpectedMetricsTable();
};
