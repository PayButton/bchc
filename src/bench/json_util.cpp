// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/json_util.h>
#include <univalue.h>

bool CheckTxsHavePrevout(const UniValue::Object &blockuv) {
    if (auto *tx_obj = blockuv.locate("tx"); tx_obj && tx_obj->isArray()) {
        for (const auto &tx : tx_obj->get_array()) {
            if (auto *vin_obj = tx.locate("vin"); vin_obj && vin_obj->isArray()) {
                for (const auto &vin : vin_obj->get_array()) {
                    if (vin.locate("coinbase")) {
                        continue;
                    } else if (!vin.locate("prevout")) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}
