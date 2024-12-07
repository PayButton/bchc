// Copyright (c) 2010-2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

/**
 * Return system error string from errno value. Use this instead of
 * std::strerror, which is not thread-safe. For network errors use
 * NetworkErrorString from netbase.h instead.
 */
std::string SysErrorString(int err);
