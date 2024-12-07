// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>

#include <chainparams.h>
#include <consensus/consensus.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(config_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(max_block_size) {
    GlobalConfig config;

    // Too small.
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(0));
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(12345));
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(LEGACY_MAX_BLOCK_SIZE - 1));
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(LEGACY_MAX_BLOCK_SIZE));

    // LEGACY_MAX_BLOCK_SIZE + 1
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(LEGACY_MAX_BLOCK_SIZE + 1));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), LEGACY_MAX_BLOCK_SIZE + 1);

    // 2MB
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(2 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), 2 * ONE_MEGABYTE);

    // 8MB
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(8 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), 8 * ONE_MEGABYTE);

    // Invalid size keep config.
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(54321));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), 8 * ONE_MEGABYTE);

    // Setting it back down
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(7 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), 7 * ONE_MEGABYTE);
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(ONE_MEGABYTE + 1));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), ONE_MEGABYTE + 1);

    // MAX_CONSENSUS_BLOCK_SIZE
    BOOST_CHECK(config.SetConfiguredMaxBlockSize(MAX_CONSENSUS_BLOCK_SIZE));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), MAX_CONSENSUS_BLOCK_SIZE);

    // Invalid size keep config.
    BOOST_CHECK(!config.SetConfiguredMaxBlockSize(MAX_CONSENSUS_BLOCK_SIZE + 1));
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), MAX_CONSENSUS_BLOCK_SIZE);
}

BOOST_AUTO_TEST_CASE(chain_params) {
    GlobalConfig config;

    // Global config is consistent with params.
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::TESTNET4);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::SCALENET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::CHIPNET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());
}

BOOST_AUTO_TEST_CASE(generated_block_size_percent) {
    GlobalConfig config;

    // Default constructed should be at the default consensus blocksize
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), DEFAULT_CONSENSUS_BLOCK_SIZE);

    // Default to equal the max block size.
    BOOST_CHECK_EQUAL(config.GetConfiguredMaxBlockSize(), config.GetGeneratedBlockSize(std::nullopt));

    // Out of range
    BOOST_CHECK(!config.SetGeneratedBlockSizePercent(-0.01));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(std::nullopt), config.GetConfiguredMaxBlockSize());
    BOOST_CHECK(!config.SetGeneratedBlockSizePercent(100.1));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(std::nullopt), config.GetConfiguredMaxBlockSize());

    BOOST_CHECK(config.SetGeneratedBlockSizePercent(0.0));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(std::nullopt), 0);

    BOOST_CHECK(config.SetGeneratedBlockSizePercent(100.0));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(std::nullopt), config.GetConfiguredMaxBlockSize());
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(64 * ONE_MEGABYTE), 64 * ONE_MEGABYTE);

    // try various percentages and they should be what we expect
    for (double percent = 0.0; percent <= 100.0; percent += 0.1) {
        const uint64_t size_override = 64 * ONE_MEGABYTE;
        const uint64_t expected = config.GetConfiguredMaxBlockSize() * (percent / 100.0);
        const uint64_t expected_override = size_override * (percent / 100.0);
        BOOST_CHECK(config.SetGeneratedBlockSizePercent(percent));
        BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(std::nullopt), expected);
        BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(size_override), expected_override);
    }
}

BOOST_AUTO_TEST_CASE(lookahead_guess) {
    GlobalConfig config;

    BOOST_REQUIRE_EQUAL(config.GetConfiguredMaxBlockSize(), DEFAULT_CONSENSUS_BLOCK_SIZE);

    for (uint64_t size = 0; size <= MAX_CONSENSUS_BLOCK_SIZE + ONE_MEGABYTE; size += ONE_MEGABYTE / 10) {
        config.NotifyMaxBlockSizeLookAheadGuessChanged(size);
        if (size <= config.GetConfiguredMaxBlockSize()) {
            // the max blocksize lookahead guess can never be smaller than the configured max blocksize
            BOOST_CHECK_EQUAL(config.GetMaxBlockSizeLookAheadGuess(), config.GetConfiguredMaxBlockSize());
        } else if (size <= MAX_CONSENSUS_BLOCK_SIZE) {
            // however if it is set to larger, the lookahead guess should be verbatim what was set by
            // NotifyMaxBlockSizeLookAheadGuessChanged() above
            BOOST_CHECK_EQUAL(config.GetMaxBlockSizeLookAheadGuess(), size);
        } else {
            // except the lookahead guess should never exceed MAX_CONSENSUS_BLOCK_SIZE
            BOOST_CHECK_EQUAL(config.GetMaxBlockSizeLookAheadGuess(), MAX_CONSENSUS_BLOCK_SIZE);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
