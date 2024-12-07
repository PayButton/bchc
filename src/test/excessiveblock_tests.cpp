// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/consensus.h>
#include <rpc/server.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

extern UniValue CallRPC(const std::string &strMethod, bool multithreaded = false);

BOOST_FIXTURE_TEST_SUITE(excessiveblock_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(excessiveblock_rpc) {
    BOOST_CHECK_NO_THROW(CallRPC("getexcessiveblock"));
}

BOOST_AUTO_TEST_SUITE_END()
