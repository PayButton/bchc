// Copyright (c) 2019-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#pragma once

class QString;

/**
 * Press "Ok" button in message box dialog.
 *
 * @param text - Optionally store dialog text.
 * @param msec - Number of milliseconds to pause before triggering the callback.
 */
void ConfirmMessage(QString *text = nullptr, int msec = 0);
