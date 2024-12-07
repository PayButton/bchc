// Copyright (c) 2021 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin Cash Node developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <functional>

namespace util {
/**
 * A wrapper for do-something-once thread functions.
 */
void TraceThread(const char *thread_name, std::function<void()> thread_func);

} // namespace util
