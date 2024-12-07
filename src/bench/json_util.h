// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <univalue.h>

/**
 * Returns true if all vins of all transactions in the JSON object for a block have "prevout" keys
 */
bool CheckTxsHavePrevout(const UniValue::Object &blockuv);
