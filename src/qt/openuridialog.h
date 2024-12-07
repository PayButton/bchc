// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDialog>

class CChainParams;

namespace Ui {
class OpenURIDialog;
}

class OpenURIDialog : public QDialog {
    Q_OBJECT

public:
    explicit OpenURIDialog(const CChainParams &params, QWidget *parent);
    ~OpenURIDialog();

    QString getURI();

protected Q_SLOTS:
    void accept() override;

private:
    Ui::OpenURIDialog *ui;
    const QString uriScheme;
};
