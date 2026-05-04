// Copyright (c) 2015-2018 The Bitcoin Core developers
// Copyright (c) 2021-2025 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <validationinterface.h>
#include <coins.h>

#include <list>
#include <memory>

class CBlockIndex;
class CZMQAbstractNotifier;

class CZMQNotificationInterface final : public CValidationInterface {
public:
    ~CZMQNotificationInterface();

    std::list<const CZMQAbstractNotifier *> GetActiveNotifiers() const;

    static std::unique_ptr<CZMQNotificationInterface> Create();

protected:
    bool Initialize();
    void Shutdown();

    // CValidationInterface
    void TransactionAddedToMempool(const CTransactionRef &tx, std::shared_ptr<const std::vector<Coin>> spent_coins) override;
    void BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                        const CBlockIndex *pindexConnected,
                        const std::vector<CTransactionRef> &vtxConflicted) override;
    void BlockDisconnected(
        const std::shared_ptr<const CBlock> &pblock,
        const CBlockIndex *pindex) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew,
                         const CBlockIndex *pindexFork,
                         bool fInitialDownload) override;
    void TransactionDoubleSpent(const CTransactionRef &ptxn,
                                const DspId &dspId) override;

private:
    CZMQNotificationInterface();

    void *pcontext = nullptr;
    std::list<std::unique_ptr<CZMQAbstractNotifier>> notifiers;
};

extern std::unique_ptr<CZMQNotificationInterface> g_zmq_notification_interface;
