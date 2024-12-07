// Copyright (c) 2016-2019 The Bitcoin Core developers
// Copyright (c) 2020-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/blockdata.h>
#include <bench/json_util.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
#include <rpc/blockchain.h>
#include <rpc/protocol.h>

#include <univalue.h>

static void JSONReadWriteBlock(int blockHeight, unsigned int pretty, bool write, benchmark::State &state,
                               TxVerbosity verbosity) {
    SelectParams(CBaseChainParams::MAIN);
    BlockData blockData(blockHeight);

    const auto blockuv = blockToJSON(GetConfig(), blockData.block, &blockData.blockIndex, &blockData.blockIndex,
                                     verbosity);
    if (verbosity == TxVerbosity::SHOW_DETAILS_AND_PREVOUT) {
        assert(CheckTxsHavePrevout(blockuv));
    }

    if (write) {
        BENCHMARK_LOOP {
            (void)UniValue::stringify(blockuv, pretty);
        }
    } else {
        std::string json = UniValue::stringify(blockuv, pretty);
        BENCHMARK_LOOP {
            UniValue uv;
            if (!uv.read(json))
                throw std::runtime_error("UniValue lib failed to parse its own generated string.");
        }
    }
}

static void JSONReadBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 0, false, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONReadBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 0, false, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONWriteBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 0, true, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONWriteBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 0, true, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONWritePrettyBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 4, true, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONWritePrettyBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 4, true, state, TxVerbosity::SHOW_DETAILS);
}
static void JSONReadVeryVerboseBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 0, false, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void JSONReadVeryVerboseBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 0, false, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void JSONWriteVeryVerboseBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 0, true, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void JSONWriteVeryVerboseBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 0, true, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void JSONWriteVeryVerbosePrettyBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(413567, 4, true, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}
static void JSONWriteVeryVerbosePrettyBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(556034, 4, true, state, TxVerbosity::SHOW_DETAILS_AND_PREVOUT);
}

BENCHMARK(JSONReadBlock_1MB, 18);
BENCHMARK(JSONReadBlock_32MB, 1);
BENCHMARK(JSONWriteBlock_1MB, 52);
BENCHMARK(JSONWriteBlock_32MB, 1);
BENCHMARK(JSONWritePrettyBlock_1MB, 47);
BENCHMARK(JSONWritePrettyBlock_32MB, 1);
BENCHMARK(JSONReadVeryVerboseBlock_1MB, 18);
BENCHMARK(JSONReadVeryVerboseBlock_32MB, 1);
BENCHMARK(JSONWriteVeryVerboseBlock_1MB, 52);
BENCHMARK(JSONWriteVeryVerboseBlock_32MB, 1);
BENCHMARK(JSONWriteVeryVerbosePrettyBlock_1MB, 47);
BENCHMARK(JSONWriteVeryVerbosePrettyBlock_32MB, 1);
