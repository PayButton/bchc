#!/usr/bin/env python3
# Copyright (c) 2022-2023 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test SIGHASH_UTXOS signing scheme which activates with Upgrade9."""
from collections import defaultdict, namedtuple
from typing import DefaultDict, List

from test_framework import address
from test_framework.key import ECKey
from test_framework.messages import (
    CBlock,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    FromHex,
    TokenOutputData,
    uint256_from_str,
    ser_uint256,
)
from test_framework.p2p import P2PDataStore
from test_framework import schnorr
from test_framework.script import (
    CScript,
    hash160,
    OP_CHECKSIG, OP_DUP, OP_EQUALVERIFY, OP_HASH160,
    SIGHASH_ALL, SIGHASH_ANYONECANPAY, SIGHASH_FORKID, SIGHASH_NONE, SIGHASH_SINGLE, SIGHASH_UTXOS,
    SignatureHashForkId,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    wait_until
)

DUST = 546


def uint256_from_hex(h: str) -> int:
    return uint256_from_str(bytes.fromhex(h)[::-1])


def uint256_to_hex(u: int) -> str:
    return ser_uint256(u)[::-1].hex()


class UTXO(namedtuple("UTXO", "outpt, txout")):
    pass


class SighashUtxosTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.base_extra_args = ['-acceptnonstdtxn=0', '-expire=0', '-whitelist=127.0.0.1']
        self.extra_args = [['-upgrade9activationheight=999999999'] + self.base_extra_args]

    def run_test(self):

        node = self.nodes[0]  # convenience reference to the node
        self.bootstrap_p2p()  # add one p2p connection to the node

        # Setup a private key and address we will use for all transactions
        self.priv_key = ECKey()
        self.priv_key.set(secret=b'SigHashUtxosTest' * 2, compressed=True)
        self.addr = address.key_to_p2pkh(self.priv_key.get_pubkey().get_bytes())
        self.spk = CScript([OP_DUP, OP_HASH160,
                            hash160(self.priv_key.get_pubkey().get_bytes()),
                            OP_EQUALVERIFY, OP_CHECKSIG])

        blockhashes = node.generatetoaddress(101, self.addr)

        self.utxos = []
        block = FromHex(CBlock(), node.getblock(blockhashes[0], 0))
        tx = block.vtx[0]
        tx.calc_sha256()
        self.update_utxos(tx)

        def sum_values(utxos: List[UTXO]) -> int:
            return sum(txout.nValue for _, txout in utxos)

        # First, sign a regular tx and confirm it can be sent
        tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk)],
                            hashtype=SIGHASH_ALL | SIGHASH_FORKID)
        self.send_txs([tx])
        assert tx.hash in node.getrawmempool()
        self.update_utxos(tx)

        # Next, sign a SIGHASH_UTXOS tx and confirm that it fails
        tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk)],
                            hashtype=SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_UTXOS)
        self.send_txs([tx], success=False, reject_reason="Signature hash type missing or not understood")
        assert tx.hash not in node.getrawmempool()

        # Get the current height
        activation_height = node.getblockchaininfo()["blocks"]
        # Restart the node, enabling upgrade9
        expected_mempool = set(node.getrawmempool())
        self.restart_node(0, extra_args=[f"-upgrade9activationheight={activation_height}"] + self.base_extra_args)
        self.reconnect_p2p()
        # Wait for mempool to reload
        wait_until(predicate=lambda: set(node.getrawmempool()) == expected_mempool, timeout=60)

        # Mine 1 block to ensure activation of new Upgrade9 rules, and to confirm the mempool
        blockhashes += node.generatetoaddress(1, self.addr)
        assert len(node.getrawmempool()) == 0

        # Now, the node should accept the SIGHASH_UTXOS txn
        self.send_txs([tx])
        assert tx.hash in node.getrawmempool()

        # Next, mine a block to confirm it, the block should contain the above txn ok
        blockhashes += node.generatetoaddress(1, self.addr)
        assert_equal(node.getblockchaininfo()["bestblockhash"], blockhashes[-1])
        assert len(node.getrawmempool()) == 0
        tx_mined = FromHex(CBlock(), node.getblock(blockhashes[-1], 0)).vtx[1]
        tx_mined.calc_sha256()
        assert_equal(tx.hash, tx_mined.hash)
        self.update_utxos(tx_mined)

        expected_txns = set()

        # Now, try SIGHASH_UTXOS combined with SIGHASH_ANYONECANPAY
        tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk)],
                            hashtype=SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_UTXOS | SIGHASH_ANYONECANPAY)
        self.send_txs([tx], success=False, reject_reason="Signature hash type missing or not understood")
        assert tx.hash not in node.getrawmempool()

        # Try the exact same txn again, this time without SIGHASH_UTXOS (belt-and-suspenders check)
        tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk)],
                            hashtype=SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_ANYONECANPAY)
        self.send_txs([tx])
        assert tx.hash in node.getrawmempool()
        self.update_utxos(tx)
        expected_txns.add(tx.hash)

        # Now try SIGHASH_UTXOS with combinations of various basetypes and signing algorithms
        for sigtype in ('schnorr', 'ecdsa'):
            for basetype in (SIGHASH_ALL, SIGHASH_SINGLE, SIGHASH_NONE):
                tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk)],
                                    hashtype=basetype | SIGHASH_FORKID | SIGHASH_UTXOS,
                                    sigtype=sigtype)
                self.send_txs([tx])
                assert tx.hash in node.getrawmempool()
                self.update_utxos(tx)
                expected_txns.add(tx.hash)

        # Next, create a token-genesis tx, sending 1000 fungibles to 2 outputs
        val_out = sum_values(self.utxos)
        assert self.utxos[0].outpt.n == 0
        token_id = self.utxos[0].outpt.hash
        tx_genesis = self.create_tx(self.utxos,
                                    [CTxOut(nValue=val_out//2 - 500,
                                            scriptPubKey=self.spk,
                                            tokenData=TokenOutputData(id=token_id, amount=1000)),
                                     CTxOut(nValue=val_out // 2,
                                            scriptPubKey=self.spk,
                                            tokenData=TokenOutputData(id=token_id, amount=1000))],
                                    hashtype=SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_UTXOS)
        self.send_txs([tx_genesis])
        assert tx_genesis.hash in node.getrawmempool()
        self.update_utxos(tx_genesis)
        expected_txns.add(tx_genesis.hash)

        # Create a txn spending both tokens to 2 outputs, NOT using SIGHASH_UTXOS
        val_out = sum_values(self.utxos)
        total_fungibles = sum(txout.tokenData.amount for _, txout in self.utxos)
        tx_spend1 = self.create_tx(self.utxos,
                                   [CTxOut(nValue=val_out//2 - 500,
                                           scriptPubKey=self.spk,
                                           tokenData=TokenOutputData(id=token_id, amount=total_fungibles//2 - 1)),
                                    CTxOut(nValue=val_out // 2,
                                           scriptPubKey=self.spk,
                                           tokenData=TokenOutputData(id=token_id, amount=total_fungibles//2 + 1))],
                                   hashtype=SIGHASH_ALL | SIGHASH_FORKID)
        self.send_txs([tx_spend1])
        assert tx_spend1.hash in node.getrawmempool()
        self.update_utxos(tx_spend1)
        expected_txns.add(tx_spend1.hash)

        # Create a txn spending both tokens to 2 outputs, this time using SIGHASH_UTXOS
        val_out = sum_values(self.utxos)
        total_fungibles = sum(txout.tokenData.amount for _, txout in self.utxos)
        tx_spend2 = self.create_tx(self.utxos,
                                   [CTxOut(nValue=val_out//2 - 500,
                                           scriptPubKey=self.spk,
                                           tokenData=TokenOutputData(id=token_id, amount=total_fungibles//2 - 3)),
                                    CTxOut(nValue=val_out // 2,
                                           scriptPubKey=self.spk,
                                           tokenData=TokenOutputData(id=token_id, amount=total_fungibles//2 + 2))],
                                   hashtype=SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_UTXOS)
        self.send_txs([tx_spend2])
        assert tx_spend2.hash in node.getrawmempool()
        self.update_utxos(tx_spend2)
        expected_txns.add(tx_spend2.hash)

        # Next, mine a block to confirm it, the block should contain the above txns ok
        found_txns = set()
        blockhashes += node.generatetoaddress(1, self.addr)
        assert_equal(node.getblockchaininfo()["bestblockhash"], blockhashes[-1])
        assert len(node.getrawmempool()) == 0
        for tx_mined in FromHex(CBlock(), node.getblock(blockhashes[-1], 0)).vtx[1:]:
            tx_mined.calc_sha256()
            found_txns.add(tx_mined.hash)
        assert_equal(expected_txns, found_txns)

    def update_utxos(self, spend_tx: CTransaction):
        """Updates self.utxos with the effects of spend_tx

        Deletes spent utxos, creates new UTXOs for spend_tx.vout"""
        i = 0
        spent_ins = set()
        for inp in spend_tx.vin:
            spent_ins.add((inp.prevout.hash, inp.prevout.n))
        # Delete spends
        while i < len(self.utxos):
            outpt, txout = self.utxos[i]
            if (outpt.hash, outpt.n) in spent_ins:
                del self.utxos[i]
                continue
            i += 1
        # Update new unspents
        spend_tx.calc_sha256()
        for i in range(len(spend_tx.vout)):
            txout = spend_tx.vout[i]
            self.utxos.append(UTXO(COutPoint(spend_tx.sha256, i), txout))

    def create_tx(self, inputs: List[UTXO], outputs: List[CTxOut],
                  *, sign=True, hashtype=SIGHASH_ALL | SIGHASH_FORKID, sigtype='schnorr'):
        """Assumption: all inputs owned by self.priv_key"""
        tx = CTransaction()
        total_value = 0
        total_token_values: DefaultDict[int, int] = defaultdict(int)
        utxos = []
        for outpt, txout in inputs:
            utxos.append(txout)
            total_value += txout.nValue
            if isinstance(txout.tokenData, TokenOutputData):
                total_token_values[txout.tokenData.id] += txout.tokenData.amount
            if outpt.n == 0:
                total_token_values[outpt.hash] = 9223372036854775808
            tx.vin.append(CTxIn(outpt))
        for out in outputs:
            total_value -= out.nValue
            assert total_value >= 0
            if isinstance(out.tokenData, TokenOutputData):
                total_token_values[out.tokenData.id] -= out.tokenData.amount
                assert total_token_values[out.tokenData.id] >= 0
            tx.vout.append(out)
        if sign:
            assert hashtype & SIGHASH_FORKID
            for i in range(len(tx.vin)):
                inp = tx.vin[i]
                utxo = utxos[i]
                # Sign the transaction
                hashbyte = bytes([hashtype & 0xff])
                sighash = SignatureHashForkId(utxo.scriptPubKey, tx, i, hashtype, utxo.nValue, utxos=utxos)
                txsig = b''
                if sigtype == 'schnorr':
                    txsig = schnorr.sign(self.priv_key.get_bytes(), sighash) + hashbyte
                elif sigtype == 'ecdsa':
                    txsig = self.priv_key.sign_ecdsa(sighash) + hashbyte
                inp.scriptSig = CScript([txsig, self.priv_key.get_pubkey().get_bytes()])
        tx.rehash()
        return tx

    def bootstrap_p2p(self):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        self.nodes[0].add_p2p_connection(P2PDataStore())
        # We need to wait for the initial getheaders from the peer before we
        # start populating our blockstore. If we don't, then we may run ahead
        # to the next subtest before we receive the getheaders. We'd then send
        # an INV for the next block and receive two getheaders - one for the
        # IBD and one for the INV. We'd respond to both and could get
        # unexpectedly disconnected if the DoS score for that error is 50.
        self.nodes[0].p2p.wait_for_getheaders(timeout=5)

    def reconnect_p2p(self):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        bs, lbh, ts, p2p = None, None, None, None
        if self.nodes[0].p2ps:
            p2p = self.nodes[0].p2p
            bs, lbh, ts = p2p.block_store, p2p.last_block_hash, p2p.tx_store
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p()
        if p2p and (bs or lbh or ts):
            # Set up the block store again so that p2p node can adequately send headers again for everything
            # node might want after a restart
            p2p = self.nodes[0].p2p
            p2p.block_store, p2p.last_block_hash, p2p.tx_store = bs, lbh, ts

    def send_txs(self, txs, success=True, reject_reason=None, reconnect=False):
        """Sends txns to test node. Syncs and verifies that txns are in mempool

        Call with success = False if the txns should be rejected."""
        self.nodes[0].p2p.send_txs_and_test(txs, self.nodes[0], success=success, expect_disconnect=reconnect,
                                            reject_reason=reject_reason)
        if reconnect:
            self.reconnect_p2p()


if __name__ == '__main__':
    SighashUtxosTest().main()
