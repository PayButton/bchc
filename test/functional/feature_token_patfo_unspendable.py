#!/usr/bin/env python3
# Copyright (c) 2022-2023 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that ensures Pre-Activation Token Forgery Outputs (PATFOs)
remain unspendable both pre and post-activation.."""
from collections import defaultdict
from typing import DefaultDict, List, NamedTuple, Tuple, Union

from test_framework import address
from test_framework.blocktools import create_block, create_coinbase
from test_framework.key import ECKey
from test_framework.messages import (
    CBlock,
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    FromHex,
    token,
    TokenOutputData,
    uint256_from_str,
    ser_uint256,
)
from test_framework.p2p import P2PDataStore
from test_framework import schnorr
from test_framework.script import (
    CScript,
    hash160,
    OP_CHECKSIG, OP_DUP, OP_EQUAL, OP_EQUALVERIFY, OP_HASH160, OP_OUTPUTBYTECODE, OP_RETURN,
    SIGHASH_ALL, SIGHASH_FORKID,
    SignatureHashForkId,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_not_equal,
    wait_until,
)

DUST = 546


def uint256_from_hex(h: str) -> int:
    return uint256_from_str(bytes.fromhex(h)[::-1])


def uint256_to_hex(u: int) -> str:
    return ser_uint256(u)[::-1].hex()


class UTXO(NamedTuple):
    outpt: COutPoint
    txout: CTxOut


def sum_values(utxos: List[UTXO]) -> int:
    return sum(txout.nValue for _, txout in utxos)


class TokenPATFOUnspendableTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.base_extra_args = ['-acceptnonstdtxn=0', '-expire=0', '-whitelist=127.0.0.1']
        self.extra_args = [['-upgrade9activationheight=999999999'] + self.base_extra_args]

    @staticmethod
    def create_p2sh_that_tests_outputbytecode(output_index: int, expected_bytecode: bytes) -> Tuple[CScript, CScript]:
        """Creates a p2sh that tests that another output in the txn matches the expected bytecode. Returns
        a tuple of (p2sh_spk, redeem_script)."""
        redeem_script = CScript([output_index, OP_OUTPUTBYTECODE, expected_bytecode, OP_EQUAL])
        p2sh_hash = hash160(redeem_script)
        p2sh_spk = CScript([OP_HASH160, p2sh_hash, OP_EQUAL])
        return p2sh_spk, redeem_script

    def test_op_outputbytecode_corner_case(self, blockhashes, spend_coinbase_blockhash, is_activated):
        """
        OP_OUTPUTBYTECODE Corner-Case:

        Txn with 2 outputs, one has token data, the other spends a p2sh that tests the
        output byte code of the token-data-containing output is what is expected.
        - Pre-activation: the output bytecode should be 100% the entire serialized TOKEN_PREFIX + token_data + spk
        - Post-activation: the output bytecode should omit the token data and just be the spk!
        """
        node = self.nodes[0]
        block = FromHex(CBlock(), node.getblock(spend_coinbase_blockhash, 0))
        block.rehash()
        cb_tx = block.vtx[0]
        cb_tx.calc_sha256()
        utxos = [UTXO(COutPoint(cb_tx.sha256, 0), cb_tx.vout[0])]

        tx_split_cb = self.create_tx(utxos, [CTxOut(COIN, self.spk), CTxOut(sum_values(utxos) - COIN - 1000, self.spk)])
        self.send_txs([tx_split_cb], success=True)
        self.log.info(f"test_op_outputbytecode_corner_case tx hash 0: {tx_split_cb.hash}")

        utxos = [UTXO(COutPoint(tx_split_cb.sha256, 1), tx_split_cb.vout[1])]
        token_genesis_utxo = UTXO(COutPoint(tx_split_cb.sha256, 0), tx_split_cb.vout[0])

        a_token = self.mk_token(token_genesis_utxo.outpt.hash, commitment=b'Just a token...')

        if is_activated:
            # Post-activation we expect the node to separate out the token data from the bytecode
            expected_bytecode = self.spk
        else:
            # Pre-activation the node behaves as a naive node that doesn't know about token data would, so we expect
            # the whole wrapped token-data blob + locking bytecode to appear on the stack as a result of
            # OP_OUTPUTBYTECODE
            expected_bytecode = token.PREFIX_BYTE + a_token.serialize() + self.spk

        p2sh_spk, redeem_script = self.create_p2sh_that_tests_outputbytecode(
            output_index=1, expected_bytecode=expected_bytecode)

        tx_fund_p2sh = self.create_tx(utxos, [CTxOut(COIN, p2sh_spk),
                                              CTxOut(sum_values(utxos) - COIN - 1000, self.spk)])
        self.send_txs([tx_fund_p2sh], success=True)
        self.log.info(f"test_op_outputbytecode_corner_case tx hash 1: {tx_fund_p2sh.hash}")
        assert tx_fund_p2sh.hash in node.getrawmempool()

        self.update_utxos(tx_fund_p2sh, utxos=utxos)
        utxos += [UTXO(COutPoint(tx_fund_p2sh.sha256, 0), tx_fund_p2sh.vout[0]), token_genesis_utxo]
        self.log.info(f"test_op_outputbytecode_corner_case UTXOS: {str(utxos)}")

        tx_test_case = self.create_tx(utxos, [CTxOut(0, CScript([OP_RETURN, b'dummy'])),
                                              CTxOut(sum_values(utxos) - 1000, self.spk, tokenData=a_token)])
        # Fudge scriptSig since create_tx signed for us but we require no signature for this input
        tx_test_case.vin[1].scriptSig = CScript([redeem_script])
        tx_test_case.rehash()

        if not is_activated:
            # This fails to be accepted due to non-standard txns disallowed pre-activation
            self.send_txs([tx_test_case], success=False, reject_reason="txn-tokens-before-activation")
            assert tx_test_case.hash not in node.getrawmempool()
        else:
            self.send_txs([tx_test_case], success=True)
            assert tx_test_case.hash in node.getrawmempool()

        # However, should always work both pre-activation and post-activation (pre-activation it's a PATFO)
        block = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1,
                                  txns=[tx_split_cb, tx_fund_p2sh, tx_test_case])
        self.send_blocks([block])
        assert_equal(block.hash, node.getbestblockhash())  # Ensure it was accepted
        self.log.info(f"test_op_outputbytecode_corner_case block hash: {block.hash}")
        blockhashes.append(block.hash)

    @staticmethod
    def mk_token(token_id: int, *, commitment: bytes = b'Patfo!') -> TokenOutputData:
        return TokenOutputData(id=token_id, amount=1000, commitment=commitment,
                               bitfield=token.Structure.HasAmount | token.Structure.HasNFT |
                                        token.Structure.HasCommitmentLength | token.Capability.Mutable)

    def run_test(self):

        node = self.nodes[0]  # convenience reference to the node
        self.bootstrap_p2p()  # add one p2p connection to the node

        # Setup a private key and address we will use for all transactions
        self.priv_key = ECKey()
        self.priv_key.set(secret=b'PATFOPATFOPATFO!!OFTAPOFTAPOFTAP', compressed=True)
        self.addr = address.key_to_p2pkh(self.priv_key.get_pubkey().get_bytes())
        self.spk = CScript([OP_DUP, OP_HASH160,
                            hash160(self.priv_key.get_pubkey().get_bytes()),
                            OP_EQUALVERIFY, OP_CHECKSIG])

        blockhashes = node.generatetoaddress(103, self.addr)

        self.utxos = []
        block = FromHex(CBlock(), node.getblock(blockhashes[0], 0))
        tx = block.vtx[0]
        tx.calc_sha256()
        self.update_utxos(tx)

        # Create a PATFO tx pre-activation, and try to send it (it should be rejected as non-standard)
        patfo_token = self.mk_token(token_id=self.utxos[0][0].hash)
        tx = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk, tokenData=patfo_token)])
        self.send_txs([tx], success=False, reject_reason="txn-tokens-before-activation")
        assert tx.hash not in node.getrawmempool()

        # Next, put this PATFO into a block
        block = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1, txns=[tx])
        self.send_blocks([block])
        assert_equal(block.hash, node.getbestblockhash())  # Ensure it was accepted
        blockhashes.append(block.hash)
        self.update_utxos(tx)

        # Next, attempt to spend the patfo to mempool (should be rejected)
        tx_send_patfo = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk,
                                                           tokenData=patfo_token)])
        self.send_txs([tx_send_patfo], success=False, reject_reason="txn-tokens-before-activation")
        assert tx_send_patfo.hash not in node.getrawmempool()

        # Like the above, but just burn the token, it should be rejected as non-standard.
        tx_burn_patfo = self.create_tx(self.utxos, [CTxOut(sum_values(self.utxos) - 500, self.spk,
                                                           tokenData=None)])
        self.send_txs([tx_burn_patfo], success=False, reject_reason="bad-txns-nonstandard-inputs")
        assert tx_burn_patfo.hash not in node.getrawmempool()

        # Attempt to mine a block that sends the PATFO.
        block = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1, txns=[tx_send_patfo])
        self.send_blocks([block], success=False, reject_reason="bad-txns-vin-tokenprefix-preactivation")
        assert_not_equal(block.hash, node.getbestblockhash())  # Ensure it was NOT accepted

        # Attempt to mine a block that burns the PATFO.
        block = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1, txns=[tx_burn_patfo])
        self.send_blocks([block], success=False, reject_reason="bad-txns-vin-tokenprefix-preactivation")
        assert_not_equal(block.hash, node.getbestblockhash())  # Ensure it was NOT accepted

        # OP_OUTPUTBYTECODE Corner-Case:
        # Txn with 2 outputs, one is a patfo, the other spends a p2sh that tests the patfo byte code is what is expected
        self.test_op_outputbytecode_corner_case(blockhashes=blockhashes, spend_coinbase_blockhash=blockhashes[1],
                                                is_activated=False)

        height = node.getblockchaininfo()["blocks"] + 1
        nTime = FromHex(CBlock(), self.nodes[0].getblock(blockhashes[-1], 0)).nTime + 1

        def mine_a_block():
            nonlocal height, nTime, block
            block = self.create_block(blockhashes[-1], height=height, nTime=nTime)
            height += 1
            nTime += 1
            self.send_blocks([block])
            assert_equal(node.getbestblockhash(), block.hash)
            blockhashes.append(block.hash)

        fork_base = None

        # --- Activate Upgrade9 ---
        # 1. Set the activation height a bit forward of the current tip's height
        activation_height = height
        # 2. Restart the node, enabling upgrade9
        self.restart_node(0, extra_args=[f"-upgrade9activationheight={activation_height}"] + self.base_extra_args)
        self.reconnect_p2p()

        # Mine blocks until it activates
        while node.getblockchaininfo()["blocks"] < activation_height:
            mine_a_block()
            if fork_base is None:
                fork_base = blockhashes[-1]

        assert fork_base is not None

        # Ensure it activated exactly on this block
        assert_greater_than_or_equal(node.getblockchaininfo()["blocks"], activation_height)
        assert_greater_than(activation_height, node.getblockheader(blockhashes[-2], True)["height"])
        activation_block_hash = node.getblockchaininfo()["bestblockhash"]
        activation_height = node.getblockchaininfo()["blocks"]

        # OP_OUTPUTBYTECODE Corner-Case:
        # Txn with 2 outputs, one is a token, the other spends a p2sh that tests the token byte code is what is expected
        self.test_op_outputbytecode_corner_case(blockhashes=blockhashes, spend_coinbase_blockhash=blockhashes[2],
                                                is_activated=True)

        # Now that Upgrade9 activated, attempt to spend those PATFOs now.. this should still fail but the error message
        # should change to indicate PATFO detection.  We do this twice, once for the block where we just activated
        # upgrade9, and one for the block after, just in the unlikely case that some regressions in the activation
        # height detector in src/validation.cpp happens to have some strange off-by-1 error.
        block_send, block_burn = None, None

        def attempt_to_send_patfo_txns():
            nonlocal block_send, block_burn
            self.send_txs([tx_send_patfo], success=False, reject_reason="bad-txns-vin-token-created-pre-activation")
            assert tx_send_patfo.hash not in node.getrawmempool()

            self.send_txs([tx_burn_patfo], success=False, reject_reason="bad-txns-vin-token-created-pre-activation")
            assert tx_send_patfo.hash not in node.getrawmempool()

            # Do the same with the above two PATFO txns, but do it via putting them directly into a block...
            block_send = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1,
                                           txns=[tx_send_patfo])
            self.send_blocks([block_send], success=False, reject_reason="bad-txns-vin-token-created-pre-activation")
            assert_not_equal(block_send.hash, node.getbestblockhash())  # Ensure it was NOT accepted
            block_burn = self.create_block(blockhashes[-1], height=node.getblockchaininfo()["blocks"] + 1,
                                           txns=[tx_burn_patfo])
            self.send_blocks([block_burn], success=False, reject_reason="bad-txns-vin-token-created-pre-activation")
            assert_not_equal(block_burn.hash, node.getbestblockhash())  # Ensure it was NOT accepted

        for _ in range(2):
            attempt_to_send_patfo_txns()
            mine_a_block()

        # Next, switch chains, undoing the activation, and then re-activate again on the new chain
        node.invalidateblock(fork_base)
        idx = blockhashes.index(fork_base)
        del blockhashes[idx:]  # remove defunct chain
        wait_until(lambda: node.getbestblockhash() == blockhashes[-1], timeout=60)
        height = node.getblockchaininfo()["blocks"] + 1
        # Fudge the time back in order to get a different activation height
        nTime = FromHex(CBlock(), node.getblock(blockhashes[-1], 0)).nTime + 6

        # Ensure we are no longer activated
        assert_greater_than(activation_height, node.getblockheader(blockhashes[-2], True)["height"])
        # Keep mining until upgrade9 activates again on the alternate chain
        while node.getblockchaininfo()["blocks"] < activation_height:
            mine_a_block()

        # Ensure it activated exactly on this block
        assert_greater_than_or_equal(node.getblockchaininfo()["blocks"], activation_height)
        assert_greater_than(activation_height, node.getblockheader(blockhashes[-2], True)["height"])
        activation_block_hash2 = node.getblockchaininfo()["bestblockhash"]
        activation_height2 = node.getblockchaininfo()["blocks"]
        assert_not_equal(activation_block_hash, activation_block_hash2)
        # Ensure the activation height is same now on this new chain (height based activation)
        assert_equal(activation_height, activation_height2)

        # Now, the PATFO test should fail again on this new chain as well just the same
        for i in range(2):
            attempt_to_send_patfo_txns()
            if i == 0:
                # Only mine the first time through here, to leave "block_burn" and "block_send" potentially connectable
                mine_a_block()

        # Re-activate Upgrade9 wayyy in the past
        self.restart_node(0, extra_args=[f"-upgrade9activationheight=0"] + self.base_extra_args)
        self.reconnect_p2p()

        # Now, since we retroactively re-set the Upgrade9 time to way in the past, the PATFO send should succeed
        # since now the PATFO is actually no longer a PATFO (this behavior of the node is not officially supported
        # but is a basic sanity check on the upgrade activation height tracker in src/validation.cpp)
        self.send_txs([tx_send_patfo], success=True)
        assert tx_send_patfo.hash in node.getrawmempool()

        # Finally, mine the conflicting txn (tx_burn_patfo) in block_burn
        # (We must use reconsiderblock since node has this block already)
        node.reconsiderblock(block_burn.hash)
        wait_until(lambda: block_burn.hash == node.getbestblockhash(), timeout=60)
        block_burn.vtx[1].calc_sha256()
        assert_equal(block_burn.vtx[1].hash, tx_burn_patfo.hash)
        self.update_utxos(tx_burn_patfo)

    def update_utxos(self, spend_tx: CTransaction, *, utxos=None):
        """Updates utxos with the effects of spend_tx. If utxos is None, updates self.utxos

        Deletes spent utxos, creates new UTXOs for spend_tx.vout"""
        if utxos is None:
            utxos = self.utxos  # Update the class attribute it not specified which list to update
        i = 0
        spent_ins = set()
        for inp in spend_tx.vin:
            spent_ins.add((inp.prevout.hash, inp.prevout.n))
        # Delete spends
        while i < len(utxos):
            outpt, txout = utxos[i]
            if (outpt.hash, outpt.n) in spent_ins:
                del utxos[i]
                continue
            i += 1
        # Update new unspents
        spend_tx.calc_sha256()
        for i in range(len(spend_tx.vout)):
            txout = spend_tx.vout[i]
            if txout.scriptPubKey == self.spk:
                utxos.append(UTXO(COutPoint(spend_tx.sha256, i), txout))

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

    def create_block(self, prev_block_hash: Union[int, str], height: int, script_pub_key=None, txns=None,
                     nTime=None) -> CBlock:
        if isinstance(prev_block_hash, str):
            prev_block_hash = uint256_from_hex(prev_block_hash)
        block_time = nTime or FromHex(CBlock(), self.nodes[0].getblock(uint256_to_hex(prev_block_hash), 0)).nTime + 1

        # First create the coinbase
        coinbase = create_coinbase(height, scriptPubKey=script_pub_key or self.spk)
        coinbase.rehash()

        txns = txns or []
        block = create_block(prev_block_hash, coinbase, block_time, txns=txns)
        block.solve()
        return block

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

    def send_blocks(self, blocks, success=True, reject_reason=None,
                    request_block=True, reconnect=False, timeout=60):
        """Sends blocks to test node. Syncs and verifies that tip has advanced to most recent block.

        Call with success = False if the tip shouldn't advance to the most recent block."""
        self.nodes[0].p2p.send_blocks_and_test(blocks, self.nodes[0], success=success,
                                               reject_reason=reject_reason, request_block=request_block,
                                               timeout=timeout, expect_disconnect=reconnect)
        if reconnect:
            self.reconnect_p2p()

    def send_txs(self, txs, success=True, reject_reason=None, reconnect=False):
        """Sends txns to test node. Syncs and verifies that txns are in mempool

        Call with success = False if the txns should be rejected."""
        self.nodes[0].p2p.send_txs_and_test(txs, self.nodes[0], success=success, expect_disconnect=reconnect,
                                            reject_reason=reject_reason)
        if reconnect:
            self.reconnect_p2p()


if __name__ == '__main__':
    TokenPATFOUnspendableTest().main()
