// Copyright (c) 2016-2019 The Bitcoin Core developers
// Copyright (c) 2020-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/blockdata.h>
#include <bench/data.h>
#include <bench/json_util.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <validation.h>
#include <streams.h>
#include <consensus/validation.h>
#include <rpc/blockchain.h>

#include <univalue.h>

static void RPCBlockVerbose(int blockHeight, benchmark::State &state, TxVerbosity verbosity) {
    SelectParams(CBaseChainParams::MAIN);
    BlockData blockData(blockHeight);

    const auto blockuv = blockToJSON(GetConfig(), blockData.block, &blockData.blockIndex, &blockData.blockIndex,
                                     verbosity);
    if (verbosity == TxVerbosity::SHOW_DETAILS_AND_PREVOUT) {
        assert(CheckTxsHavePrevout(blockuv));
    }

    BENCHMARK_LOOP {
        (void)blockToJSON(GetConfig(), blockData.block, &blockData.blockIndex, &blockData.blockIndex,
                          TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
    }
}

static void RPCBlockVerbose_1MB(benchmark::State &state) {
    RPCBlockVerbose(413567, state, TxVerbosity::SHOW_DETAILS);
}
static void RPCBlockVerbose_32MB(benchmark::State &state) {
    RPCBlockVerbose(556034, state, TxVerbosity::SHOW_DETAILS);
}
static void RPCBlockVeryVerbose_1MB(benchmark::State &state) {
    RPCBlockVerbose(413567, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void RPCBlockVeryVerbose_32MB(benchmark::State &state) {
    RPCBlockVerbose(556034, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}

BENCHMARK(RPCBlockVerbose_1MB, 23);
BENCHMARK(RPCBlockVerbose_32MB, 1);
BENCHMARK(RPCBlockVeryVerbose_1MB, 23);
BENCHMARK(RPCBlockVeryVerbose_32MB, 1);
