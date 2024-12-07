#!/usr/bin/env python3
# Copyright (c) 2022-2023 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC functions that support CashTokens."""
from decimal import Decimal

from test_framework.messages import COIN, COutPoint, CTransaction, CTxIn, CTxOut, FromHex
from test_framework.test_framework import BitcoinTestFramework
from test_framework.txtools import calc_dust_limit
from test_framework.util import assert_equal, assert_not_equal, assert_raises_rpc_error


class CashTokenRPCTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        # Both nodes have the CashTokens upgrade activated
        self.extra_args = [["-upgrade9activationheight=0"]] * self.num_nodes
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    @staticmethod
    def get_key(utxo):
        return f'{utxo["txid"]}:{utxo["vout"]}'

    @staticmethod
    def get_total_amount(utxos):
        if isinstance(utxos, dict):
            it = utxos.values()
        else:
            it = utxos
        return sum(utxo['amount'] for utxo in it)

    def create_token_genesis_tx(self, node, to_addr, bch_amount, token_amount, nft=None, fee=Decimal('0.00001'),
                                capability='minting'):
        utxos = dict()
        # Find a genesis utxo we can use
        for utxo in node.listunspent():
            if utxo['vout'] == 0:
                utxos[self.get_key(utxo)] = utxo
                token_id = utxo['txid']
                break
        else:
            assert False, "No genesis-capable UTXOs found in node wallet"
        # Add more utxos to meet amount predicate
        while self.get_total_amount(utxos) < bch_amount + fee:
            for utxo in node.listunspent():
                key = self.get_key(utxo)
                if key not in utxos:
                    utxos[key] = utxo
                    break
            else:
                assert False, "Not enough funds"
        u0 = list(utxos.values())[0]
        change_addr = u0['address']
        change_spk = bytes.fromhex(u0['scriptPubKey'])
        token_data = {"category": token_id, "amount": str(token_amount)}
        if nft is not None:
            token_data["nft"] = {"capability": capability, "commitment": nft.hex()}
        amount_in = self.get_total_amount(utxos)
        amount_change = amount_in - bch_amount - fee
        assert amount_change >= 0
        change_outs = []
        if amount_change > Decimal(calc_dust_limit(txout=CTxOut(0, change_spk))) / Decimal(COIN):
            change_outs.append({change_addr: amount_change})
        txhex = node.createrawtransaction(
            [{"txid": u['txid'], "vout": u['vout']} for u in utxos.values()],
            [{to_addr: {"amount": bch_amount, "tokenData": token_data}}] + change_outs
        )
        tx = FromHex(CTransaction(), txhex)
        for i, txout in enumerate(tx.vout):
            dust = calc_dust_limit(txout=txout)
            assert txout.nValue >= dust, f"Dust limit: output {i} is below {dust} ({txout})"
        res = node.signrawtransactionwithwallet(txhex, None, "ALL|FORKID|UTXOS")
        self.log.info(f"signrawtransactionwithwallet result: {res}")
        assert res["complete"]
        txhex = res["hex"]
        fee_sats = int(fee * COIN)
        assert len(txhex) // 2 <= fee_sats, f"Paid too little fee ({fee_sats}) for txn of size {len(txhex) // 2}"
        tx = FromHex(CTransaction(), txhex)
        tx.rehash()
        self.log.info(f"Signed tx: {tx}")
        return tx

    def mine_to_non_wallet_addess(self, nblocks):
        if nblocks <= 0:
            return
        unknown_address = 'mpV7aGShMkJCZgbW7F6iZgrvuPHjZjH9qg'
        self.sync_mempools()
        self.nodes[0].generatetoaddress(nblocks, unknown_address)
        self.sync_all()

    def make_coins_for_each_node(self, ncoins):
        addrs = list()

        # Check that there's no UTXO on any of the nodes
        for node in self.nodes:
            assert_equal(len(node.listunspent()), 0)
            walletinfo = node.getwalletinfo()
            assert_equal(walletinfo['immature_balance'], 0)
            assert_equal(walletinfo['balance'], 0)

        for node in self.nodes:
            # Make some wallet addresses we will be transacting to
            addrs.append(node.getnewaddress())
            # Generate ncoins blocks for each node
            node.generate(ncoins)
            self.sync_all()
            # Check that balances are what we expect
            walletinfo = node.getwalletinfo()
            assert_equal(walletinfo['immature_balance'], 50 * ncoins)
            assert_equal(walletinfo['balance'], 0)

        # Mature the above blocks
        self.mine_to_non_wallet_addess(101)

        for i, node in enumerate(self.nodes):
            # Ensure each node has ncoins UTXOs
            unspent = node.listunspent()
            self.log.info(f'Node {i} unspent: {unspent}')
            assert_equal(len(unspent), 5)
            walletinfo = node.getwalletinfo()
            assert_equal(walletinfo['immature_balance'], 0)
            assert_equal(walletinfo['balance'], 50 * ncoins)

        self.log.info(f"Wallet addresses we will use for each node: {addrs}")
        return addrs

    @classmethod
    def unspents_to_set(cls, utxos):
        return {cls.get_key(u) for u in utxos}

    def run_test(self):
        addrs = self.make_coins_for_each_node(5)

        # Mint a token, sending it to node 1's wallet
        tx = self.create_token_genesis_tx(self.nodes[0], addrs[1], 1, 123456, nft=bytes.fromhex("beeff00d"))

        # Before sending the minted token, ensure wallet on node 1 sees no tokens
        toks_before = self.nodes[1].listunspent(0, None, None, True, {"tokensOnly": True})
        bal_before = self.nodes[1].getbalance()
        self.log.info(f"Balance for node 1, before receiving a token: {bal_before}")
        assert_equal(len(toks_before), 0)
        unspents_before = self.unspents_to_set(self.nodes[1].listunspent())
        assert_not_equal(len(unspents_before), 0)

        # Broadcast
        self.nodes[0].sendrawtransaction(tx.serialize().hex())
        self.sync_all()
        assert tx.hash in self.nodes[0].getrawmempool()
        self.mine_to_non_wallet_addess(1)

        # Token UTXOs aren't normally visible to listunspent, unless explicitly asking for them
        toks = self.nodes[1].listunspent(0, None, None, True, {"tokensOnly": True})
        self.log.info(f"Token unspents for node 1: {toks}")
        assert_not_equal(self.unspents_to_set(toks), self.unspents_to_set(toks_before))

        # Ensure that the token-containing utxo doesn't appear in the default utxo list for this wallet
        assert_equal(self.unspents_to_set(self.nodes[1].listunspent()), unspents_before)

        # Token-containing utxos do add to the wallet balance
        bal = self.nodes[1].getbalance()
        self.log.info(f"Balance node 1 after receiving a token: {bal}")
        assert_equal(bal_before + self.get_total_amount(toks), bal)

        # Check that the token utxo is what we expect
        assert_equal(len(toks), 1)
        assert_equal(toks[0]["txid"], tx.hash)
        assert_equal(toks[0]["vout"], 0)
        token_id_hex = tx.vout[0].tokenData.id_hex
        assert_equal(toks[0]["tokenData"]["category"], token_id_hex)
        assert_equal(toks[0]["tokenData"]["amount"], str(tx.vout[0].tokenData.amount))
        assert_equal(toks[0]["tokenData"]["nft"]["commitment"], "beeff00d")
        assert_equal(toks[0]["tokenData"]["nft"]["capability"], "minting")

        # Check that scantxoutset can find tokens using the "tok()" descriptor
        res = self.nodes[0].scantxoutset("start", [f"tok({token_id_hex})"])
        self.log.info(f"Results from scantxoutset to match token_id {token_id_hex}: {res}")
        assert res["success"]
        assert_equal(len(res['unspents']), 1)
        assert_equal(res['unspents'][0]["tokenData"], toks[0]["tokenData"])

        # Check that one cannot send these token-containing UTXOs using sendtoaddress
        assert_raises_rpc_error(-6, "Insufficient funds",
                                self.nodes[1].sendtoaddress,
                                addrs[0],  # Send to wallet on node 0
                                bal,  # Attempt to send ALL (including token)
                                None, None,  # No wallet labels
                                True,  # Subtract fee from amount
                                0,  # Coin selection algorithm (0 = default BNB)
                                True  # Include unsafe
                                )

        # However, one can empty the wallet leaving just the token utxo
        bal0 = self.nodes[0].getbalance()
        self.log.info(f"Balance on node 0: {bal0}, sending: {bal_before}")
        txid = self.nodes[1].sendtoaddress(addrs[0], bal_before, None, None, True, 0, True)
        fee = self.nodes[1].gettransaction(txid)["fee"]
        self.mine_to_non_wallet_addess(1)
        # Ensure node0 wallet received the non-token funds
        bal0_after = self.nodes[0].getbalance()
        self.log.info(f"Balance on node 0: {bal0_after}")
        assert_equal(bal0_after - bal0, bal_before + fee)

        # Check that just the token UTXO is left in the wqllet on node 1
        assert_equal(self.nodes[1].getbalance(), toks[0]["amount"])
        assert_equal(self.nodes[1].listunspent(), [])
        toks2 = self.nodes[1].listunspent(0, None, None, True, {"includeTokens": True})
        assert_equal(self.unspents_to_set(toks), self.unspents_to_set(toks2))

        # Send the remaining token utxo from node1's wallet to node0's wallet
        assert_equal(self.nodes[0].listunspent(0, None, None, True, {"tokensOnly": True}), [])
        tx_send_token = CTransaction()
        tx_send_token.vin = [CTxIn(COutPoint(tx.sha256, 0), b'', 0xffffffff)]
        nValue = tx.vout[0].nValue - 1000
        spk = bytes.fromhex(self.nodes[0].validateaddress(addrs[0])["scriptPubKey"])
        tokenData = tx.vout[0].tokenData
        tx_send_token.vout = [CTxOut(nValue, spk, tokenData)]
        res = self.nodes[1].signrawtransactionwithwallet(tx_send_token.serialize().hex())
        assert res["complete"]
        txhex = res["hex"]
        tx_send_token = FromHex(CTransaction(), txhex)
        tx_send_token.rehash()
        assert_equal(self.nodes[1].sendrawtransaction(txhex), tx_send_token.hash)
        self.mine_to_non_wallet_addess(1)

        # Check that getrawtransaction with verbosity=2 contains the expected tokenData in the "vin" key
        result = self.nodes[0].getrawtransaction(tx_send_token.hash, 2)
        assert_equal(result["hash"], tx_send_token.hash)
        assert_equal(result["vin"][0]["tokenData"]["category"], tokenData.id_hex)
        assert_equal(result["vin"][0]["tokenData"]["amount"], str(tokenData.amount))  # Impl. quirk: "amount" is str
        assert_equal(result["vin"][0]["tokenData"]["nft"]["commitment"], tokenData.commitment.hex())
        assert_equal(result["vin"][0]["tokenData"]["nft"]["capability"], "minting")

        # Ensure the token got there
        bal0_after_2 = self.nodes[0].getbalance()
        self.log.info(f"Balance on node 0 after receiving a token: {bal0_after_2}")
        assert_equal(bal0_after_2 - bal0_after, Decimal(nValue) / Decimal(COIN))
        toks0 = self.nodes[0].listunspent(0, None, None, True, {"tokensOnly": True})
        assert_equal(len(toks0), 1)
        # Check token data is ok
        assert_equal(toks0[0]["tokenData"], toks[0]["tokenData"])
        # Ensure token is no longer there in node 1's wallet
        assert_equal(self.nodes[1].listunspent(0, None, None, True, {"tokensOnly": True}), [])
        assert_equal(self.nodes[1].getbalance(), 0)


if __name__ == '__main__':
    CashTokenRPCTest().main()
