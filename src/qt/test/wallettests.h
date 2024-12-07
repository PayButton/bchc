// Copyright (c) 2018-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#pragma once

#include <QObject>
#include <QTest>

class WalletTests : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void walletTests();
};
