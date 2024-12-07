// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2018-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <consensus/merkle.h>
#include <random.h>
#include <uint256.h>

static void MerkleRoot(benchmark::State &state) {
    FastRandomContext rng(true);
    std::vector<uint256> leaves;
    leaves.resize(9001);
    for (auto &item : leaves) {
        rng.rand256(item);
    }
    BENCHMARK_LOOP {
        bool mutation = false;
        uint256 hash =
            ComputeMerkleRoot(std::vector<uint256>(leaves), &mutation);
        leaves[mutation] = hash;
    }
}

BENCHMARK(MerkleRoot, 800);
