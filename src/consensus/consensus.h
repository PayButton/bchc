// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <cstdint>

/** 1MB */
static const uint64_t ONE_MEGABYTE = 1000000;
/** The maximum allowed size for a transaction, in bytes */
static const uint64_t MAX_TX_SIZE = ONE_MEGABYTE;
/** The minimum allowed size for a transaction, in bytes.
 * ABCH: Implements CHIP-2021-01 Minimum Transaction Size. */
static const uint64_t MIN_TX_SIZE = 65;
/** The maximum allowed size for a block, before the UAHF */
static const uint64_t LEGACY_MAX_BLOCK_SIZE = ONE_MEGABYTE;
/** Default setting for maximum allowed size for a block, in bytes.
 * ABCH: Disabled because of CHIP-2023-04 Adaptive Blocksize Limit Algorithm for
 * Bitcoin Cash.
 */
static const uint64_t DEFAULT_MAX_BLOCK_SIZE = 100000000000000;
/** Allowed number of signature check operations per transaction. */
static const uint64_t MAX_TX_SIGCHECKS = 3000;
/**
 * The ratio between the maximum allowable block size and the maximum allowable
 * SigChecks (executed signature check operations) in the block. (network rule).
 */
static const int BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO = 141;
/**
 * Coinbase transaction outputs can only be spent after this number of new
 * blocks (network rule).
 */
static const int COINBASE_MATURITY = 100;
/** Coinbase scripts have their own script size limit. */
static const int MAX_COINBASE_SCRIPTSIG_SIZE = 100;

/** Flags for nSequence and nLockTime locks */
/** Interpret sequence numbers as relative lock-time constraints. */
static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);

/**
 * Compute the maximum number of sigchecks that can be contained in a block
 * given the MAXIMUM block size as parameter. The maximum sigchecks scale
 * linearly with the maximum block size and do not depend on the actual
 * block size. The returned value is rounded down (there are no fractional
 * sigchecks so the fractional part is meaningless).
 */
inline uint64_t GetMaxBlockSigChecksCount(uint64_t maxBlockSize) {
    return maxBlockSize / BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO;
}

#endif // BITCOIN_CONSENSUS_CONSENSUS_H
