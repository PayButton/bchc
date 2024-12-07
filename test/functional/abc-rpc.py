#!/usr/bin/env python3
# Copyright (c) 2017-2023 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Exercise the Bitcoin ABC RPC calls.

import re

from test_framework.cdefs import (
    DEFAULT_CONSENSUS_BLOCK_SIZE,
    LEGACY_MAX_BLOCK_SIZE,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

BLOCKSIZE_TOO_LOW = "Invalid parameter, maxBlockSize must be larger than {}".format(
    LEGACY_MAX_BLOCK_SIZE)

BLOCKSIZE_OUT_OF_RANGE = "Parameter out of range"


class ABC_RPC_Test(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.tip = None
        self.setup_clean_chain = True
        self.extra_args = [['-whitelist=127.0.0.1']]

    def check_subversion(self, pattern_str):
        # Check that the subversion is set as expected
        netinfo = self.nodes[0].getnetworkinfo()
        subversion = netinfo['subversion']
        pattern = re.compile(pattern_str)
        assert pattern.match(subversion)

    def test_excessiveblock(self):
        # Check that we start with DEFAULT_CONSENSUS_BLOCK_SIZE
        getsize = self.nodes[0].getexcessiveblock()
        ebs = getsize['excessiveBlockSize']
        assert_equal(ebs, DEFAULT_CONSENSUS_BLOCK_SIZE)

    def run_test(self):
        self.genesis_hash = int(self.nodes[0].getbestblockhash(), 16)
        self.test_excessiveblock()


if __name__ == '__main__':
    ABC_RPC_Test().main()
