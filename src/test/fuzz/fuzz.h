// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <span.h>

#include <cstdint>
#include <functional>
#include <string>

const std::function<std::string(const char *)> G_TRANSLATION_FUN = nullptr;

void test_one_input(Span<const uint8_t> buffer);
