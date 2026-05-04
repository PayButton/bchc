// Copyright (c) 2026 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHRONIK_CPP_UTIL_STORAGE_H
#define BITCOIN_CHRONIK_CPP_UTIL_STORAGE_H

class CTxUndo;
struct FlatFilePos;

bool ReadTxUndoFromDisk(CTxUndo &tx, const FlatFilePos &pos);
bool ReadTxFromDisk(CMutableTransaction &tx, const FlatFilePos &pos);

#endif // BITCOIN_CHRONIK_CPP_UTIL_STORAGE_H
