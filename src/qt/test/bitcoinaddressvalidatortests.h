// Copyright (c) 2017 The Bitcoin Developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QObject>
#include <QTest>

class BitcoinAddressValidatorTests : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void inputTests();
};
