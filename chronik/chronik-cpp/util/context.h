// Copyright (c) 2026 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHRONIK_CPP_UTIL_CONTEXT_H
#define BITCOIN_CHRONIK_CPP_UTIL_CONTEXT_H

#include <node/context.h>
#include <validation.h>

namespace node {
    using NodeContext = ::NodeContext;
}

static CTxMemPool &GetMempool() {
    return g_mempool;
}

#endif // BITCOIN_CHRONIK_CPP_UTIL_CONTEXT_H
