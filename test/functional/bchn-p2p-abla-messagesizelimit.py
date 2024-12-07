#!/usr/bin/env python3
# Copyright (c) 2023-2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test support for ABLA-related p2p 'block' message size limits."""
import random

from test_framework.abla import AblaState
from test_framework.cdefs import (
    BLOCK_DOWNLOAD_WINDOW,
    DEFAULT_CONSENSUS_BLOCK_SIZE,
    ONE_MEGABYTE,
)
from test_framework.messages import msg_block
from test_framework.p2p import P2PDataStore
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    wait_until,
)

ABLA_LOOKAHEAD_BLOCKS = BLOCK_DOWNLOAD_WINDOW * 2


class MyP2P(P2PDataStore):
    n_disconnects = 0
    n_rej_blk_length = 0

    def on_close(self):
        self.n_disconnects += 1
        super().on_close()

    def on_reject(self, message):
        # Tally 'bad-blk-length' reject messages seen.
        if message.reason.find(b'bad-blk-length') >= 0:
            self.n_rej_blk_length += 1
        super().on_reject(message)

    # noinspection PyPep8Naming
    class fake_msg_block(msg_block):
        size = 0

        def __init__(self, size):
            super().__init__()
            self.size = size

        def serialize(self):
            """Generates random (and bad) block data with a serialized size of `self.size`"""
            buf = random.getrandbits(80 * 8).to_bytes(length=80, byteorder='little')
            rem = self.size - len(buf)
            if rem > 0:
                buf += bytes([0]) * rem
            return buf

    def send_dummy_block_msg(self, size):
        self.send_message(self.fake_msg_block(size))


class P2PAblaMessageSizeLimitTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.base_extra_args = ['-expire=0', '-checkmempool=0', '-allowunconnectedmining=1', '-whitelist=127.0.0.1',
                                '-percentblockmaxsize=100']
        self.extra_args = [['-upgrade10activationheight=2147483647'] + self.base_extra_args] * self.num_nodes
        # We need a long rpc timeout here so that the sanitizer-undefined CI job may pass
        self.rpc_timeout = 600

    def get_header(self, blockhash=None, verbose=True):
        if blockhash is None:
            blockhash = self.nodes[0].getblockchaininfo()['bestblockhash']
        return self.nodes[0].getblockheader(blockhash, verbose)

    def get_height(self, blockhash=None):
        return self.get_header(blockhash, True)['height']

    def get_abla_activation_height(self):
        """Walk backwards from tip, finding ABLA activation height. Returns the height or None if ABLA not activated."""
        node = self.nodes[0]
        seen = False
        bh = node.getblockchaininfo()['bestblockhash']
        while bh:
            hdr = self.get_header(bh)
            if 'ablastate' in hdr:
                seen = True
            else:
                if seen:
                    return hdr['height'] + 1
                else:
                    return None
            bh = hdr.get('previousblockhash')
        return 0

    @staticmethod
    def get_lookahead_limit(abla_state=None):
        if abla_state is None:
            abla_state = AblaState()
        return abla_state.CalcLookaheadBlockSizeLimit(ABLA_LOOKAHEAD_BLOCKS)

    def bootstrap_p2p(self):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        self.nodes[0].add_p2p_connection(MyP2P())
        # We need to wait for the initial getheaders from the peer before we
        # start populating our blockstore. If we don't, then we may run ahead
        # to the next subtest before we receive the getheaders. We'd then send
        # an INV for the next block and receive two getheaders - one for the
        # IBD and one for the INV. We'd respond to both and could get
        # unexpectedly disconnected if the DoS score for that error is 50.
        self.nodes[0].p2p.wait_for_getheaders(timeout=10)

    def reconnect_p2p(self):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        bs, lbh, ts, p2p, nd, nr = None, None, None, None, 0, 0
        if self.nodes[0].p2ps:
            p2p = self.nodes[0].p2p
            bs, lbh, ts, nd, nr = (p2p.block_store, p2p.last_block_hash, p2p.tx_store, p2p.n_disconnects,
                                   p2p.n_rej_blk_length)
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p()
        if p2p and (bs or lbh or ts or nd or nr):
            # Set up the block store again so that p2p node can adequately send headers again for everything
            # node might want after a restart
            p2p = self.nodes[0].p2p
            p2p.block_store, p2p.last_block_hash, p2p.tx_store, p2p.n_disconnects, p2p.n_rej_blk_length = (
                bs, lbh, ts, nd, nr)

    def test_block_msgs_around_size(self, lookahead_size, reconnect=False):
        node = self.nodes[0]
        rej_ct = node.p2p.message_count['reject']
        n_rej_blk_length = node.p2p.n_rej_blk_length

        # Block-like messages are limited by 2 * the current "lookahead" block size
        limit = lookahead_size * 2

        # Critical size - 1, should just be a reject message (bad block)
        node.p2p.send_dummy_block_msg(limit - 1)
        wait_until(lambda: node.p2p.message_count['reject'] == rej_ct + 1, timeout=30)

        # At critical size, same
        node.p2p.send_dummy_block_msg(limit)
        wait_until(lambda: node.p2p.message_count['reject'] == rej_ct + 2, timeout=30)

        # Past critical size, should yield a disconnect due to over-sized block msg
        with node.assert_debug_log(expected_msgs=['Oversized header detected'], timeout=30):
            nd = node.p2p.n_disconnects
            node.p2p.send_dummy_block_msg(limit + 1)
            wait_until(lambda: node.p2p.n_disconnects == nd + 1, timeout=30)

        # We should have gotten a disconnect early in the pipeline, and not gotten to the CheckBlock() phase to
        # even receive a reject message related to block length. Ensure that is the case.
        assert_equal(n_rej_blk_length, node.p2p.n_rej_blk_length)

        if reconnect:
            self.reconnect_p2p()

    def run_test(self):
        node = self.nodes[0]
        addr, _ = node.get_deterministic_priv_key()
        self.bootstrap_p2p()

        # Node default state, ABLA not activated, it should have a modest lookahead limit a little bit larger
        # than 32 MB, but smaller than 36MB
        default_lookahead = self.get_lookahead_limit()
        assert_greater_than(default_lookahead, DEFAULT_CONSENSUS_BLOCK_SIZE)
        assert_greater_than(4 * ONE_MEGABYTE + DEFAULT_CONSENSUS_BLOCK_SIZE, default_lookahead)

        # Kick node out of IBD, and check log that the "lookahead" limit is what we expect for a non-ABLA-activated node
        with node.assert_debug_log(expected_msgs=[f'lookahead-blocksize-guess for tip height 1 to {default_lookahead}'],
                                   timeout=30):
            node.generatetoaddress(1, addr)
            assert_equal(self.get_abla_activation_height(), None)

        # Just check defaults are what we expect for the p2p
        assert_equal(node.p2p.message_count['reject'], 0)
        assert_equal(node.p2p.n_disconnects, 0)

        # Test block messages around size: default_lookahead
        self.test_block_msgs_around_size(default_lookahead)

        # Activate upgrade 10
        current_height = self.get_height()

        self.restart_node(0, extra_args=self.base_extra_args + [f'-upgrade10activationheight={current_height + 1}'])
        self.reconnect_p2p()

        assert self.get_abla_activation_height() is None

        # Mine until activated
        while self.get_abla_activation_height() is None:
            node.generatetoaddress(1, addr)

        assert_equal(self.get_abla_activation_height(), self.get_header()['height'])

        # Mine a bunch of full blocks to raise blocksize limit (and consequently the lookahead limit)
        node.fillmempool(300)
        assert_greater_than(node.getmempoolinfo()['bytes'], DEFAULT_CONSENSUS_BLOCK_SIZE)
        prev_lookahead = default_lookahead
        loop_ct = 0
        while node.getmempoolinfo()['bytes'] - ONE_MEGABYTE >= self.get_header()['ablastate']['nextblocksizelimit']:
            node.generatetoaddress(1, addr)

            # Confirm that blocksize limit grew, and that lookahead limit grew
            cur_abla_state = AblaState.FromRpcDict(self.get_header())
            assert_greater_than(cur_abla_state.GetNextBlockSizeLimit(), DEFAULT_CONSENSUS_BLOCK_SIZE)
            current_lookahead = self.get_lookahead_limit(cur_abla_state)
            assert_greater_than(current_lookahead, prev_lookahead)

            self.log.info(f"new lookahead size: {current_lookahead}")

            # Test that the new lookahead limit applied to block messages behaves as we expect
            self.test_block_msgs_around_size(current_lookahead, reconnect=True)

            prev_lookahead = current_lookahead
            loop_ct += 1
        assert_greater_than(loop_ct, 0)


if __name__ == '__main__':
    P2PAblaMessageSizeLimitTest().main()
