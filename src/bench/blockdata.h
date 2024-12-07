// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chain.h>
#include <primitives/block.h>
#include <primitives/blockhash.h>

#include <cstdint>
#include <vector>

/**
 * A collection of block details to simplify benchmarks that call `blockToJSON`
 */
class BlockData {
private:
    CBlockIndex prevBlockIndex;
    BlockHash blockHash;
    BlockHash prevBlockHash;

public:
    CBlock block;
    CBlockIndex blockIndex;

    /**
     * Gathers data for the block at height `blockHeight` from the corresponding files within
     * src/bench/data, if available.  Undo data for the block is also prepared and written to disk.
     */
    BlockData(int blockHeight);
    ~BlockData();
};
