// Copyright (c) 2026 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h> // ABC: common/args.h
#include <fs.h>

#include <chronik-cpp/util/chaintype.h> // ABC: util/chaintype.h

#include <string>
#include <variant>

static int64_t GetIntArg(const ArgsManager &args, const std::string &strArg,
                  int64_t nDefault) {
    return args.GetArg(strArg, nDefault);
}

static fs::path GetDataDirBase() {
    return GetDataDir(false);
}

static fs::path GetDataDirNet() {
    return GetDataDir(true);
}

static uint16_t DefaultChronikPort(ChainType chain) {
    switch (chain) {
        case ChainType::MAIN:
            return 8331;
        case ChainType::TESTNET:
            return 18331;
        case ChainType::REGTEST:
        case ChainType::TESTNET4:
        case ChainType::SCALENET:
        case ChainType::CHIPNET:
            return 18442;
    }
}

static uint16_t DefaultChronikElectrumPort(ChainType chain) {
    switch (chain) {
        case ChainType::MAIN:
            return 50002;
        case ChainType::TESTNET:
            return 60002;
        case ChainType::REGTEST:
        case ChainType::TESTNET4:
        case ChainType::SCALENET:
        case ChainType::CHIPNET:
            return 60103;
    }
}
