// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2019-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <shutdown.h>

#include <consensus/validation.h>
#include <logging.h>
#include <ui_interface.h>
#include <util/system.h>
#include <warnings.h>

#include <atomic>
#include <string>

/** Abort with a message */
bool AbortNode(const std::string &strMessage, const std::string &userMessage) {
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occurred, see "
                                "debug.log for details")
                            : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool AbortNode(CValidationState &state, const std::string &strMessage, const std::string &userMessage) {
    AbortNode(strMessage, userMessage);
    return state.Error(strMessage);
}

static std::atomic<bool> fRequestShutdown(false);

void StartShutdown() {
    fRequestShutdown = true;
}
void AbortShutdown() {
    fRequestShutdown = false;
}
bool ShutdownRequested() {
    return fRequestShutdown;
}
