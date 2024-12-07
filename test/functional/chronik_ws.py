# Copyright (c) 2023 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test whether Chronik sends WebSocket messages correctly."""
import time

from test_framework.blocktools import create_block, create_coinbase
from test_framework.messages import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, chronik_sub_to_blocks

QUORUM_NODE_COUNT = 16


class ChronikWsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.noban_tx_relay = True
        self.extra_args = [
            [
                "-chronik",
                "-whitelist=noban@127.0.0.1",
            ],
        ]
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_chronik()

    def run_test(self):
        node = self.nodes[0]
        chronik = node.get_chronik_client()

        # Connect, but don't subscribe yet
        ws = chronik.ws()

        tip = node.getbestblockhash()

        # Make sure chronik has synced
        node.syncwithvalidationinterfacequeue()

        # Now subscribe to blocks, we'll get block updates from now on
        chronik_sub_to_blocks(ws, node)

        now = int(time.time())
        node.setmocktime(now)

        # Mine block
        tip = self.generate(node, 1)[-1]
        height = node.getblockcount()

        from test_framework.chronik.client import pb

        # We get a CONNECTED msg
        assert_equal(
            ws.recv(),
            pb.WsMsg(
                block=pb.MsgBlock(
                    msg_type=pb.BLK_CONNECTED,
                    block_hash=bytes.fromhex(tip)[::-1],
                    block_height=height,
                    block_timestamp=now,
                )
            ),
        )

        def coinbase_data_from_block(blockhash):
            coinbase = node.getblock(blockhash, 2)["tx"][0]
            coinbase_scriptsig = bytes.fromhex(coinbase["vin"][0]["coinbase"])
            coinbase_outputs = [
                {
                    "value": int(txout["value"] * COIN),
                    "output_script": bytes.fromhex(txout["scriptPubKey"]["hex"]),
                }
                for txout in coinbase["vout"]
            ]
            return pb.CoinbaseData(
                coinbase_scriptsig=coinbase_scriptsig, coinbase_outputs=coinbase_outputs
            )

        coinbase_data = coinbase_data_from_block(tip)

        # When we invalidate, we get a DISCONNECTED msg
        node.invalidateblock(tip)
        assert_equal(
            ws.recv(),
            pb.WsMsg(
                block=pb.MsgBlock(
                    msg_type=pb.BLK_DISCONNECTED,
                    block_hash=bytes.fromhex(tip)[::-1],
                    block_height=height,
                    block_timestamp=now,
                    coinbase_data=coinbase_data,
                )
            ),
        )

        now += 1
        node.setmocktime(now)

        tip = self.generate(node, 1)[-1]
        height = node.getblockcount()

        # We get a CONNECTED msg
        assert_equal(
            ws.recv(),
            pb.WsMsg(
                block=pb.MsgBlock(
                    msg_type=pb.BLK_CONNECTED,
                    block_hash=bytes.fromhex(tip)[::-1],
                    block_height=height,
                    block_timestamp=now,
                )
            ),
        )

        # When we invalidate, we get a DISCONNECTED msg
        node.invalidateblock(tip)

        coinbase_data = coinbase_data_from_block(tip)

        # We get a DISCONNECTED msg
        assert_equal(
            ws.recv(),
            pb.WsMsg(
                block=pb.MsgBlock(
                    msg_type=pb.BLK_DISCONNECTED,
                    block_hash=bytes.fromhex(tip)[::-1],
                    block_height=height,
                    block_timestamp=now,
                    coinbase_data=coinbase_data,
                )
            ),
        )

        now += 1
        node.setmocktime(now)
        tip = node.getbestblockhash()

        # Create a block that will be immediately parked by the node so it will
        # not even disconnect.
        # This coinbase is missing the miner fund output
        height = node.getblockcount() + 1
        cb1 = create_coinbase(height)
        block1 = create_block(int(tip, 16), cb1, now)
        block1.solve()

        # And a second block that builds on top of the first one, which will
        # also be rejected
        cb2 = create_coinbase(height + 1)
        block2 = create_block(int(block1.hash, 16), cb2, now)
        block2.solve()

        ws.close()


if __name__ == "__main__":
    ChronikWsTest().main()
