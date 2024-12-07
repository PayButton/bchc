// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

class CValidationState;

/** Abort with a message */
bool AbortNode(const std::string &strMessage, const std::string &userMessage = "");
bool AbortNode(CValidationState &state, const std::string &strMessage, const std::string &userMessage = "");

void StartShutdown();
void AbortShutdown();
bool ShutdownRequested();
