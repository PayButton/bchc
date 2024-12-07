// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2021-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <limits>

/** 1MB */
inline constexpr uint64_t ONE_MEGABYTE = 1000000;
/** The maximum allowed size for a transaction, in bytes */
inline constexpr uint64_t MAX_TX_SIZE = ONE_MEGABYTE;
/** The minimum allowed size for a transaction, in bytes, after magnetic anomaly but before upgrade 9 */
inline constexpr uint64_t MIN_TX_SIZE_MAGNETIC_ANOMALY = 100;
/** The minimum allowed size for a transaction, in bytes, after upgrade 9 */
inline constexpr uint64_t MIN_TX_SIZE_UPGRADE9 = 65;
/** The maximum allowed size for a block, before the UAHF */
inline constexpr uint64_t LEGACY_MAX_BLOCK_SIZE = ONE_MEGABYTE;
/** Default setting for maximum allowed size for a block, in bytes. Post activation of upgrade 10, this is the minimum
    for the max block size since ABLA will dynamically adjust the blocksize upward based on demand
    (except on testnet3/testnet4 where maximum blocksize remains fixed) */
inline constexpr uint64_t DEFAULT_CONSENSUS_BLOCK_SIZE = 32 * ONE_MEGABYTE;
/**
 *  Maximum consensus blocks size: 2GB. This is a temporary limit
 *  to prevent consensus failure between 32-bit and 64-bit platforms,
 *  until we drop 32-bit platform support altogether, at which point
 *  this constant should be raised well beyond 32-bit addressing limits.
 */
inline constexpr uint64_t MAX_CONSENSUS_BLOCK_SIZE = uint64_t(2000) * ONE_MEGABYTE;
static_assert(MAX_CONSENSUS_BLOCK_SIZE <= std::numeric_limits<unsigned int>::max(),
              "MAX_CONSENSUS_BLOCK_SIZE must fit within an unsigned int due to current block file data format");

/** Allowed number of signature check operations per transaction. */
inline constexpr uint64_t MAX_TX_SIGCHECKS = 3000;
/**
 * The ratio between the maximum allowable block size and the maximum allowable
 * SigChecks (executed signature check operations) in the block. (network rule).
 */
inline constexpr int BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO = 141;
/**
 * Coinbase transaction outputs can only be spent after this number of new
 * blocks (network rule).
 */
inline constexpr int COINBASE_MATURITY = 100;
/** Coinbase scripts have their own script size limit. */
inline constexpr int MAX_COINBASE_SCRIPTSIG_SIZE = 100;

/** Flags for nSequence and nLockTime locks */
/** Interpret sequence numbers as relative lock-time constraints. */
inline constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);
/** Use GetMedianTimePast() instead of nTime for end point timestamp. */
inline constexpr unsigned int LOCKTIME_MEDIAN_TIME_PAST = (1 << 1);

/**
 * Compute the maximum number of sigchecks that can be contained in a block
 * given the MAXIMUM block size as parameter. The maximum sigchecks scale
 * linearly with the maximum block size and do not depend on the actual
 * block size. The returned value is rounded down (there are no fractional
 * sigchecks so the fractional part is meaningless).
 */
inline constexpr uint64_t GetMaxBlockSigChecksCount(uint64_t maxBlockSize) {
    return maxBlockSize / BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO;
}
