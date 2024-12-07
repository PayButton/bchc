// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util.h>

#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <consensus/merkle.h>
#include <miner.h>
#include <pow.h>
#include <validation.h>

#include <zlib.h>

#include <cassert>
#include <stdexcept>

CTxIn MineBlock(const Config &config, const CScript &coinbase_scriptPubKey) {
    auto block = PrepareBlock(config, coinbase_scriptPubKey);

    while (!CheckProofOfWork(block->GetHash(), block->nBits,
                             config.GetChainParams().GetConsensus())) {
        ++block->nNonce;
        assert(block->nNonce);
    }

    bool processed{ProcessNewBlock(config, block, true, nullptr)};
    assert(processed);

    return CTxIn{block->vtx[0]->GetId(), 0};
}

std::shared_ptr<CBlock> PrepareBlock(const Config &config,
                                     const CScript &coinbase_scriptPubKey) {
    auto block =
        std::make_shared<CBlock>(BlockAssembler{config, ::g_mempool}
                                     .CreateNewBlock(coinbase_scriptPubKey)
                                     ->block);

    block->nTime = ::ChainActive().Tip()->GetMedianTimePast() + 1;
    block->hashMerkleRoot = BlockMerkleRoot(*block);

    return block;
}

Span<uint8_t> UncompressInPlace(Span<uint8_t> outputBuf, Span<const uint8_t> compressedBytes) {
    const size_t uncompBufSpace = outputBuf.size();
    Span<const Bytef> compressed(reinterpret_cast<const Bytef *>(compressedBytes.data()), compressedBytes.size());
    Bytef *outb = reinterpret_cast<Bytef *>(outputBuf.data());
    unsigned long uncompSzResult = uncompBufSpace;
    const auto r = uncompress(outb, &uncompSzResult, compressed.data(), compressed.size());
    if (r != Z_OK) {
        throw std::runtime_error(strprintf("zlib uncompress returned %i", r));
    }
    assert(uncompSzResult <= uncompBufSpace);
    return outputBuf.subspan(0, uncompSzResult);
}

namespace {
template <typename Container>
Container UncompressGeneric(Span<const uint8_t> compressedBytes, const size_t uncompSz) {
    Container uncompressedData;
    uncompressedData.resize(uncompSz);
    auto span = MakeUInt8Span(uncompressedData);
    span = UncompressInPlace(span, compressedBytes);
    uncompressedData.resize(span.size());
    return uncompressedData;
}
}

std::string UncompressStr(Span<const uint8_t> compressedBytes, size_t uncompressedByteSize) {
    return UncompressGeneric<std::string>(compressedBytes, uncompressedByteSize);
}

std::vector<uint8_t> Uncompress(Span<const uint8_t> compressedBytes, size_t uncompressedByteSize) {
    return UncompressGeneric<std::vector<uint8_t>>(compressedBytes, uncompressedByteSize);
}
