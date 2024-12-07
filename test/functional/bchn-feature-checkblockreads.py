#!/usr/bin/env python3
# Copyright (c) 2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Tests that the -checkblockreads argument works as expected"""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import connect_nodes, disconnect_nodes

CHECK_BLOCK_READS_LOG_MSG = 'ReadRawBlockFromDisk: checks passed for block'
SENT_A_BLOCK_LOG_MSG = 'received getdata for: block'


class CheckBlockReadsTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [["-checkblockreads=1"], ["-checkblockreads=0"]]

    def run_test(self):
        node_check, node_nocheck = self.nodes[0], self.nodes[1]

        # When node0 serves up blocks, it should run the extra slow sanity checks because it was started with
        # -checkblockreads=1
        with node_check.assert_debug_log([SENT_A_BLOCK_LOG_MSG, CHECK_BLOCK_READS_LOG_MSG]):
            # Disconnect the nodes to ensure that the blocks we generate get requested via the GETDATA(block) p2p msg
            disconnect_nodes(node_check, node_nocheck)
            node_check.generatetoaddress(10, node_check.get_deterministic_priv_key().address)
            # Reconnect the nodes to induce a block download
            connect_nodes(node_check, node_nocheck)
            self.sync_blocks()

        # However, when node1 serves up blocks, its debug log should *not* contain any messages related to sanity checks
        with node_nocheck.assert_debug_log([SENT_A_BLOCK_LOG_MSG]):
            # Disconnect the nodes to ensure that the blocks we generate get requested via the GETDATA(block) p2p msg
            disconnect_nodes(node_check, node_nocheck)
            node_nocheck.generatetoaddress(10, node_nocheck.get_deterministic_priv_key().address)
            # Reconnect the nodes to induce a block download
            connect_nodes(node_check, node_nocheck)
            self.sync_blocks()

        # Read the debug log for node1 and ensure that CHECK_BLOCK_READS_LOG_MSG is not in the log
        debug_log = os.path.join(node_nocheck.datadir, node_check.chain, 'debug.log')
        with open(debug_log, "r", encoding="utf-8") as dl:
            log_text = dl.read()
            assert CHECK_BLOCK_READS_LOG_MSG not in log_text


if __name__ == '__main__':
    CheckBlockReadsTest().main()
