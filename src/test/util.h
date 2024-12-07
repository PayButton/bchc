// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <span.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CBlock;
class Config;
class CScript;
class CTxIn;
class CWallet;

// Lower-level utils //

/** Returns the generated coin */
CTxIn MineBlock(const Config &config, const CScript &coinbase_scriptPubKey);
/** Prepare a block to be mined */
std::shared_ptr<CBlock> PrepareBlock(const Config &config,
                                     const CScript &coinbase_scriptPubKey);

// Use zlib to uncompress to a byte buffer. Throws on error, such as if uncompressedByteSize is too small, otherwise
// returns the uncompressed bytes. The returned vector's size is always <= uncompressedByteSize.
std::vector<uint8_t> Uncompress(Span<const uint8_t> compressedBytes, size_t uncompressedByteSize);
// Like the above but return the data as a string. Intended to be used for large JSON data strings embedded in the app,
// hence the std::string return type. The returned string is always <= uncompressedByteSize long.
std::string UncompressStr(Span<const uint8_t> compressedBytes, size_t uncompressedByteSize);
// Use zlib to uncompress to a generic byte buffer in-place. Throws on error such as if outputBuf does not have enough
// space to hold the uncompressed data. Returns a subspan of outputBuf that enecompasses the uncompressed data actually
// written.
Span<uint8_t> UncompressInPlace(Span<uint8_t> outputBuf, Span<const uint8_t> compressedBytes);
