// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>

#include <algorithm>

std::string COutPoint::ToString(bool fVerbose) const {
    const std::string::size_type cutoff = fVerbose ? std::string::npos : 10;
    return strprintf("COutPoint(%s, %u)", txid.ToString().substr(0, cutoff), n);
}

std::string CTxIn::ToString(bool fVerbose) const {
    const std::string::size_type cutoff = fVerbose ? std::string::npos : 24;
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString(fVerbose);
    if (prevout.IsNull()) {
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    } else {
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, cutoff));
    }
    if (nSequence != SEQUENCE_FINAL) {
        str += strprintf(", nSequence=%u", nSequence);
    }
    str += ")";
    return str;
}

std::string CTxOut::ToString(bool fVerbose) const {
    const std::string::size_type cutoff = fVerbose ? std::string::npos : 30;
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s%s)", nValue / COIN,
                     (nValue % COIN) / SATOSHI,
                     HexStr(scriptPubKey).substr(0, cutoff),
                     tokenDataPtr ? (" " + tokenDataPtr->ToString(fVerbose)) : "");
}

CMutableTransaction::CMutableTransaction()
    : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction &tx)
    : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime) {}

static uint256 ComputeCMutableTransactionHash(const CMutableTransaction &tx) {
    return SerializeHash(tx, SER_GETHASH, 0);
}

TxId CMutableTransaction::GetId() const {
    return TxId(ComputeCMutableTransactionHash(*this));
}

TxHash CMutableTransaction::GetHash() const {
    return TxHash(ComputeCMutableTransactionHash(*this));
}

void CMutableTransaction::SortInputsBip69() {
    std::sort(vin.begin(), vin.end(), [](const CTxIn &a, const CTxIn &b){
        // COutPoint operator< does sort in accordance with Bip69, so just use that.
        return a.prevout < b.prevout;
    });
}

void CMutableTransaction::SortOutputsBip69() {
    std::sort(vout.begin(), vout.end(), [](const CTxOut &a, const CTxOut &b){
        if (a.nValue == b.nValue) {
            // Note: prevector operator< does NOT properly order scriptPubKeys lexicographically. So instead we
            // use std::lexicographical_compare
            const auto &spkA = a.scriptPubKey;
            const auto &spkB = b.scriptPubKey;
            if (spkA == spkB) {
                // SPK's equal, drill down to comparing tokenData (see token::OutputData::operator<)
                return a.tokenDataPtr < b.tokenDataPtr;
            }
            return std::lexicographical_compare(spkA.begin(), spkA.end(), spkB.begin(), spkB.end());
        }
        return a.nValue < b.nValue;
    });
}

uint256 CTransaction::ComputeHash() const {
    return SerializeHash(*this, SER_GETHASH, 0);
}

/*static*/ const CTransaction CTransaction::null;

//! This sharedNull is a singleton returned by MakeTransactionRef() (no args).
//! It is a 'fake' shared pointer that points to `null` above, and its deleter
//! is a no-op.
/*static*/ const CTransactionRef CTransaction::sharedNull{&CTransaction::null, [](const CTransaction *){}};

/* private - for constructing the above null value only */
CTransaction::CTransaction() : nVersion{CTransaction::CURRENT_VERSION}, nLockTime{0} {}

/* public */
CTransaction::CTransaction(const CMutableTransaction &tx)
    : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx)
    : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime), hash(ComputeHash()) {}

Amount CTransaction::GetValueOut() const {
    Amount nValueOut = Amount::zero();
    for (const auto &tx_out : vout) {
        nValueOut += tx_out.nValue;
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut)) {
            throw std::runtime_error(std::string(__func__) +
                                     ": value out of range");
        }
    }
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const {
    return ::GetSerializeSize(*this, PROTOCOL_VERSION);
}

std::string CTransaction::ToString(bool fVerbose) const {
    const std::string::size_type cutoff = fVerbose ? std::string::npos : 10;
    std::string str;
    str += strprintf("CTransaction(txid=%s, ver=%d, vin.size=%u, vout.size=%u, "
                     "nLockTime=%u)\n",
                     GetId().ToString().substr(0, cutoff), nVersion, vin.size(),
                     vout.size(), nLockTime);
    for (const auto &nVin : vin) {
        str += "    " + nVin.ToString(fVerbose) + "\n";
    }
    for (const auto &nVout : vout) {
        str += "    " + nVout.ToString(fVerbose) + "\n";
    }
    return str;
}
