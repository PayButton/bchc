// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <config.h>
#include <consensus/activation.h>
#include <util/system.h>
#include <validation.h>

#include <test/libauth_testing_setup.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <optional>
#include <string>

namespace {

/// Test fixture that can force-enable or disable upgrade9 (cashtokens)
struct Upgrade9OverrideTestingSetup : LibauthTestingSetup {
    std::optional<int32_t> upgrade9OriginalOverride;
    bool touchedUpgrade9{};

    Upgrade9OverrideTestingSetup() {
        upgrade9OriginalOverride = g_Upgrade9HeightOverride;
    }

    ~Upgrade9OverrideTestingSetup() override {
        if (touchedUpgrade9) {
            g_Upgrade9HeightOverride = upgrade9OriginalOverride;
        }
    }

    /// Activates or deactivates upgrade 9 by setting the activation time in the past or future respectively
    void SetUpgrade9Active(bool active) {
        const auto currentHeight = []{
            LOCK(cs_main);
            return ::ChainActive().Tip()->nHeight;
        }();
        auto activationHeight = active ? currentHeight - 1 : currentHeight + 1;
        g_Upgrade9HeightOverride = activationHeight;
        touchedUpgrade9 = true;
    }
};


/// Test fixture that can force-enable or disable upgrade 11 (vmlimits + bigint) as well as upgrade9 (cashtokens)
struct Upgrade11OverrideTestingSetup : Upgrade9OverrideTestingSetup {
    std::optional<std::string> optOrigArg;
    bool touchedUpgrade11 = false;

    Upgrade11OverrideTestingSetup() {
        if (gArgs.IsArgSet("-upgrade11activationtime")) {
            optOrigArg = gArgs.GetArg("-upgrade11activationtime", "");
        }
    }

    ~Upgrade11OverrideTestingSetup() override {
        if (touchedUpgrade11) {
            gArgs.ClearArg("-upgrade11activationtime");
            if (optOrigArg) {
                gArgs.ForceSetArg("-upgrade11activationtime", *optOrigArg);
            }
        }
    }

    /// Activates or deactivates upgrade 11 by setting the activation time in the past or future respectively
    void SetUpgrade11Active(bool active) {
        if (active) {
            gArgs.ForceSetArg("-upgrade11activationtime", "1000000");
        } else {
            gArgs.ForceSetArg("-upgrade11activationtime", "9223372036854775807");
        }
        touchedUpgrade11 = true;
    }
};

bool ran2022 = false, ran2023 = false, ran2025 = false;

} // namespace


BOOST_AUTO_TEST_SUITE(libauth_tests)

BOOST_FIXTURE_TEST_CASE(regression_2022, Upgrade11OverrideTestingSetup) {
    SetUpgrade9Active(false); // needs to be forced off for this series of tests
    SetUpgrade11Active(false); // also need to ensure upgrade11 is not activated for this series of tests
    RunTestPack("2022");
    ran2022 = true;
}

BOOST_FIXTURE_TEST_CASE(regression_2023, Upgrade11OverrideTestingSetup) {
    SetUpgrade11Active(false); // need to ensure upgrade11 is not activated for this series of tests
    RunTestPack("2023");
    ran2023 = true;
}

BOOST_FIXTURE_TEST_CASE(upgrade11_2025, Upgrade11OverrideTestingSetup) {
    SetUpgrade11Active(true); // force Upgrade11 (vmlimts + bigint) to be active
    RunTestPack("2025");
    ran2025 = true;
}

// Precondition: This test *requires* that all Libauth test packs have previously completed as part of this
// test_bitcoin run.
BOOST_FIXTURE_TEST_CASE(test_lookup_table, TestingSetup) {
    BOOST_REQUIRE(ran2022 && ran2023 && ran2025);
    LibauthTestingSetup::ProcessExpectedReasonsTable();
    LibauthTestingSetup::ProcessExpectedMetricsTable();
}
BOOST_AUTO_TEST_SUITE_END()
