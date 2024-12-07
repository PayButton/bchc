#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions."""

from typing import Optional, Tuple, Union
from .script import (
    CScript,
    OP_CHECKSIG,
    OP_DUP,
    OP_EQUALVERIFY,
    OP_HASH160,
    OP_RETURN,
    OP_TRUE,
    OP_NOP,
)
from .messages import (
    CBlock,
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    FromHex,
    ToHex,
    TokenOutputData,
    ser_string,
)
from .txtools import pad_tx
from .util import assert_equal, satoshi_round

# Genesis block data (regtest)
TIME_GENESIS_BLOCK = 1296688602
GENESIS_BLOCK_HASH = "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"
GENESIS_CB_TXID = "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"
GENESIS_CB_PK = (
    "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38"
    "c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"
)
GENESIS_CB_SCRIPT_PUBKEY = CScript.fromhex(f"41{GENESIS_CB_PK}ac")
GENESIS_CB_SCRIPT_SIG = CScript(
    b"\x04\xff\xff\x00\x1d\x01\x04EThe Times 03/Jan/2009 Chancellor on brink of "
    b"second bailout for banks"
)
COINBASE_MATURITY = 100


def create_block(hashprev: Union[int, str], coinbase: Optional[CTransaction], nTime: Optional[int] = None,
                 *, txns=None, ctor=True):
    """Create a block (with regtest difficulty)"""
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time() + 600)
    else:
        assert isinstance(nTime, int)
        block.nTime = nTime
    if isinstance(hashprev, str):
        # Convert hex-encoded headers to an int
        hashprev = int(hashprev, 16)
    block.hashPrevBlock = hashprev
    block.nBits = 0x207fffff  # Will break after a difficulty adjustment...
    if coinbase:
        block.vtx.append(coinbase)
    if txns:
        if ctor:
            txns = sorted(txns, key=lambda x: x.hash)
        block.vtx.extend(txns)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block


def make_conform_to_ctor(block):
    for tx in block.vtx:
        tx.rehash()
    block.vtx = [block.vtx[0]] + \
        sorted(block.vtx[1:], key=lambda tx: tx.get_id())


def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.


def create_coinbase(height, pubkey=None, *, scriptPubKey=None, tokenData=None, pad_to_size=None):
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff),
                              ser_string(serialize_script_num(height)), 0xffffffff))
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = 50 * COIN
    halvings = int(height / 150)  # regtest
    coinbaseoutput.nValue >>= halvings
    if scriptPubKey is not None:
        coinbaseoutput.scriptPubKey = scriptPubKey
    elif pubkey is not None:
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbaseoutput.tokenData = tokenData
    coinbase.vout = [coinbaseoutput]

    if pad_to_size is not None:
        # Caller specified a padding size so pass that down
        pad_tx(coinbase, pad_to_size=pad_to_size)
    else:
        # Caller did not specify a padding size, just use the default for pad_tx() (100 bytes)
        pad_tx(coinbase)

    coinbase.calc_sha256()
    return coinbase


def bu_create_coinbase(height, pubkey=None, scriptPubKey=None, *, pad_to_size=100):
    """BU Version:
       Create a coinbase transaction, assuming no miner fees.
       If pubkey is passed in, the coinbase output will be a P2PK output;
       otherwise an anyone-can-spend output."""
    assert not (pubkey and scriptPubKey), "cannot both have pubkey and custom scriptPubKey"
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff),
                              ser_string(serialize_script_num(height)), 0xffffffff))
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = 50 * COIN
    halvings = int(height / 150)  # regtest
    coinbaseoutput.nValue >>= halvings
    if pubkey is not None:
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        if scriptPubKey is None:
            scriptPubKey = CScript([OP_NOP])
        coinbaseoutput.scriptPubKey = CScript(scriptPubKey)
    coinbase.vout = [coinbaseoutput]

    # Make sure the coinbase is at least pad_to_size bytes
    coinbase_size = len(coinbase.serialize())
    if coinbase_size < pad_to_size:
        coinbase.vin[0].scriptSig += b'x' * (pad_to_size - coinbase_size)

    coinbase.calc_sha256()
    return coinbase


def create_tx_with_script(prevtx, n, script_sig=b"",
                          amount=1, script_pub_key=CScript(),
                          *, token_data=None, pad_to_size=None):
    """Return one-input, one-output transaction object
       spending the prevtx's n-th output with the given amount.

       Can optionally pass scriptPubKey and scriptSig, default is anyone-can-spend output.
    """
    tx = CTransaction()
    assert n < len(prevtx.vout)
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), script_sig, 0xffffffff))
    tx.vout.append(CTxOut(amount, script_pub_key, tokenData=token_data))
    if pad_to_size is not None:
        # Caller specified a padding size so pass that down
        pad_tx(tx, pad_to_size=pad_to_size)
    else:
        # Caller did not specify a padding size, just use the default for pad_tx() (100 bytes)
        pad_tx(tx)
    tx.calc_sha256()
    return tx


def create_transaction(node, txid, to_address, amount, *, token_data=None):
    """ Return signed transaction spending the first output of the
        input txid. Note that the node must be able to sign for the
        output that is being spent, and the node must not be running
        multiple wallets.
    """
    raw_tx = create_raw_transaction(node, txid, to_address, amount, token_data=token_data)
    tx = FromHex(CTransaction(), raw_tx)
    return tx


def add_token_data_to_transaction(tx: Union[str, CTransaction], output_num: int,
                                  token_data: TokenOutputData) -> Tuple[str, CTransaction]:
    if not isinstance(tx, CTransaction):
        rawtx = tx
        tx = CTransaction()
        FromHex(tx, rawtx)
    assert isinstance(token_data, TokenOutputData)
    assert output_num < len(tx.vout)
    tx.vout[output_num].tokenData = token_data
    rawtx = ToHex(tx)
    tx = CTransaction()
    FromHex(tx, rawtx)
    assert tx.vout[output_num].tokenData == token_data, "Verification of token_data re-serialization failed"
    tx.rehash()
    return rawtx, tx


def create_raw_transaction(node, txid, to_address, amount, vout=0, *, sighashtype="ALL|FORKID",
                           token_data=None):
    """ Return raw signed transaction spending an output (the first
        by default) output of the input txid.
        Note that the node must be able to sign for the
        output that is being spent, and the node must not be running
        multiple wallets.
    """
    inputs = [{"txid": txid, "vout": vout}]
    outputs = {to_address: amount}
    rawtx = node.createrawtransaction(inputs, outputs)
    if token_data is not None:
        # Since createrawtransaction API doesn't (yet) support specifying token data, we do it "manually"
        rawtx, tx = add_token_data_to_transaction(rawtx, 0, token_data)
        assert tx.vout[0].nValue == amount, "Unexpected amount for output 0, unable to add token_data"
    signresult = node.signrawtransactionwithwallet(rawtx, None, sighashtype)
    assert_equal(signresult["complete"], True)
    return signresult['hex']


def get_legacy_sigopcount_block(block, fAccurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, fAccurate)
    return count


def get_legacy_sigopcount_tx(tx, fAccurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(fAccurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the
        # moment
        count += CScript(j.scriptSig).GetSigOpCount(fAccurate)
    return count


def create_confirmed_utxos(test_framework, node, count, age=101):
    """
    Helper to create at least "count" utxos
    """
    to_generate = int(0.5 * count) + age
    while to_generate > 0:
        test_framework.generate(node, min(25, to_generate))
        to_generate -= 25
    utxos = node.listunspent()
    iterations = count - len(utxos)
    addr1 = node.getnewaddress()
    addr2 = node.getnewaddress()
    if iterations <= 0:
        return utxos
    for i in range(iterations):
        t = utxos.pop()
        inputs = []
        inputs.append({"txid": t["txid"], "vout": t["vout"]})
        outputs = {}
        outputs[addr1] = satoshi_round(t['amount'] / 2)
        outputs[addr2] = satoshi_round(t['amount'] / 2)
        raw_tx = node.createrawtransaction(inputs, outputs)
        ctx = FromHex(CTransaction(), raw_tx)
        fee = node.calculate_fee(ctx) // 2
        ctx.vout[0].nValue -= fee
        # Due to possible truncation, we go ahead and take another satoshi in
        # fees to ensure the transaction gets through
        ctx.vout[1].nValue -= fee + 1
        signed_tx = node.signrawtransactionwithwallet(ToHex(ctx))["hex"]
        node.sendrawtransaction(signed_tx)

    while (node.getmempoolinfo()['size'] > 0):
        test_framework.generate(node, 1)

    utxos = node.listunspent()
    assert len(utxos) >= count
    return utxos


def mine_big_block(test_framework, node, utxos=None):
    # generate a 66k transaction,
    # and 14 of them is close to the 1MB block limit
    num = 14
    utxos = utxos if utxos is not None else []
    if len(utxos) < num:
        utxos.clear()
        utxos.extend(node.listunspent())
    send_big_transactions(node, utxos, num, 100)
    test_framework.generate(node, 1)


def send_big_transactions(node, utxos, num, fee_multiplier):
    from .cashaddr import decode
    txids = []
    padding = "1" * 512
    addrHash = decode(node.getnewaddress())[2]

    for _ in range(num):
        ctx = CTransaction()
        utxo = utxos.pop()
        txid = int(utxo['txid'], 16)
        ctx.vin.append(CTxIn(COutPoint(txid, int(utxo["vout"])), b""))
        ctx.vout.append(
            CTxOut(int(satoshi_round(utxo['amount'] * COIN)),
                   CScript([OP_DUP, OP_HASH160, addrHash, OP_EQUALVERIFY, OP_CHECKSIG])))
        for i in range(0, 127):
            ctx.vout.append(CTxOut(0, CScript(
                [OP_RETURN, bytes(padding, 'utf-8')])))
        # Create a proper fee for the transaction to be mined
        ctx.vout[0].nValue -= int(fee_multiplier * node.calculate_fee(ctx))
        signresult = node.signrawtransactionwithwallet(
            ToHex(ctx), None, "NONE|FORKID")
        txid = node.sendrawtransaction(signresult["hex"], True)
        txids.append(txid)
    return txids
