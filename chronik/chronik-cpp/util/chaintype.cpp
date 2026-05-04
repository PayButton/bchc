// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chronik-cpp/util/chaintype.h>
#include <chainparamsbase.h>

#include <cassert>
#include <optional>
#include <string>

std::string ChainTypeToString(ChainType chain) {
    switch (chain) {
        case ChainType::MAIN:
            return CBaseChainParams::MAIN;
        case ChainType::TESTNET:
            return CBaseChainParams::TESTNET;
        case ChainType::REGTEST:
            return CBaseChainParams::REGTEST;
        case ChainType::TESTNET4:
            return CBaseChainParams::TESTNET4;
        case ChainType::SCALENET:
            return CBaseChainParams::SCALENET;
        case ChainType::CHIPNET:
            return CBaseChainParams::CHIPNET;
    }
    assert(false);
}

std::optional<ChainType> ChainTypeFromString(std::string_view chain) {
    if (chain == CBaseChainParams::MAIN) {
        return ChainType::MAIN;
    } else if (chain == CBaseChainParams::TESTNET) {
        return ChainType::TESTNET;
    } else if (chain == CBaseChainParams::REGTEST) {
        return ChainType::REGTEST;
    } else if (chain == CBaseChainParams::TESTNET4) {
        return ChainType::TESTNET4;
    } else if (chain == CBaseChainParams::SCALENET) {
        return ChainType::SCALENET;
    } else if (chain == CBaseChainParams::CHIPNET) {
        return ChainType::CHIPNET;
    } else {
        return std::nullopt;
    }
}
