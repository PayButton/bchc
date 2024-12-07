// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

/// Called by main app to load and register all the libauth benches if user speficied `-libauth` on the CLI,
/// `arg` is whatever argument the user passed to `-libauth=<arg>` (may be empty string).
void EnableLibAuthBenches(const std::string &arg);
