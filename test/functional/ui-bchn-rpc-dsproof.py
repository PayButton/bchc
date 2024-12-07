#!/usr/bin/env python3
# Copyright (c) 2021-2022 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""" Test for the DoubleSpend Proof UI """

import time
from decimal import Decimal
from test_framework.blocktools import create_raw_transaction
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import find_output


class DoubleSpendProofUITest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-splash=0", "-ui", "-txindex"],[]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def basic_check(self):
        """Tests basic dsproof UI functionality"""

        # Create and mine a regular non-coinbase transaction to fund nodes[1]
        spendable_txid = self.nodes[0].getblock(self.nodes[0].getblockhash(1))['tx'][0]
        spendable_tx = create_raw_transaction(self.nodes[0], spendable_txid, self.nodes[1].getnewaddress(), 49.999)
        funding_txid = self.nodes[0].sendrawtransaction(spendable_tx)
        self.generate(self.nodes[1], 1)
        self.sync_all()


        # Create conflicting transactions. They are only signed, but not yet submitted to the mempool
        first_ds_tx = create_raw_transaction(self.nodes[1], funding_txid, self.nodes[0].getnewaddress(), 49.95)
        second_ds_tx = create_raw_transaction(self.nodes[1], funding_txid, self.nodes[1].getnewaddress(), 49.95)

        # Send the transaction and wait
        first_ds_tx_id = self.nodes[1].sendrawtransaction(first_ds_tx)
        time.sleep(10)

        # See notification and status change
        self.nodes[1].call_rpc('sendrawtransaction', second_ds_tx,
                                                 ignore_error='txn-mempool-conflict')
        time.sleep(10)

        # extend the spending chain
        vout = find_output(self.nodes[1], first_ds_tx_id, Decimal('49.95'))
        child = create_raw_transaction(self.nodes[0], first_ds_tx_id, self.nodes[1].getnewaddress(), 49.90, vout)
        self.nodes[0].sendrawtransaction(child)

        # synchronize
        self.sync_all()
        self.generate(self.nodes[1], 6)
        self.sync_all()

        # wait before exit
        time.sleep(1000)

    def run_test(self):
        self.basic_check()

if __name__ == '__main__':
    DoubleSpendProofUITest().main()
