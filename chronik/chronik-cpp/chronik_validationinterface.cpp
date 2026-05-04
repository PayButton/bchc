// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#include <blockindex.h>
#include <chronik-cpp/util/hash.h>
#include <chronik_lib/src/ffi.rs.h>
//#include <kernel/chain.h>
#include <logging.h>
#include <primitives/block.h>
#include <txmempool.h>
#include <validationinterface.h>

#include <coins.h>
#include <chronik-cpp/util/context.h> // ABC: node/context.h

namespace chronik {

/**
 * CValidationInterface connecting bitcoind events to Chronik
 */
class ChronikValidationInterface final : public CValidationInterface {
public:
    ChronikValidationInterface(const node::NodeContext &node,
                               rust::Box<chronik_bridge::Chronik> chronik_box)
        : m_node(node), m_chronik(std::move(chronik_box)) {}

    void Register() { RegisterValidationInterface(this); }

    void Unregister() { UnregisterValidationInterface(this); }

    bool StopChronik() {
        try {
            m_chronik->stop();
        } catch (const std::exception &e) {
            LogPrintf("Error stopping Chronik: %s\n", e.what());
            return false;
        }
        return true;
    }

private:
    rust::Box<chronik_bridge::Chronik> m_chronik;
    const node::NodeContext &m_node;

    void TransactionAddedToMempool(
        const CTransactionRef &ptx,
        std::shared_ptr<const std::vector<Coin>> spent_coins) override {
        const TxMempoolInfo info = GetMempool().info(ptx->GetId());
        m_chronik->handle_tx_added_to_mempool(*ptx, *spent_coins,
                                              info.nTime);
    }

    void TransactionRemovedFromMempool(const CTransactionRef &ptx) override {
        m_chronik->handle_tx_removed_from_mempool(
            chronik::util::HashToArray(ptx->GetId()));
    }

    void BlockConnected(
            const std::shared_ptr<const CBlock> &block,
            const CBlockIndex *pindex,
            const std::vector<CTransactionRef> &txnConflicted) override {
        for (const CTransactionRef &ptx : txnConflicted) {
            m_chronik->handle_tx_removed_from_mempool(
                chronik::util::HashToArray(ptx->GetId()));
        }
        // We can safely pass T& here as Rust guarantees us that no references
        // can be kept after the below function call completed.
        m_chronik->handle_block_connected(*block, *pindex);
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock> &block,
                           const CBlockIndex *pindex) override {
        // See BlockConnected for safety
        m_chronik->handle_block_disconnected(*block, *pindex);
    }

    /*
    void BlockFinalized(const CBlockIndex *pindex) override {
        m_chronik->handle_block_finalized(*pindex);
    }

    void BlockInvalidated(const CBlockIndex *pindex,
                          const std::shared_ptr<const CBlock> &block) override {
        m_chronik->handle_block_invalidated(*block, *pindex);
    }

    void TransactionFinalized(const CTransactionRef &tx) override {
        m_chronik->handle_tx_finalized(chronik::util::HashToArray(tx->GetId()));
    }

    void TransactionInvalidated(
        const CTransactionRef &tx,
        std::shared_ptr<const std::vector<Coin>> spent_coins) override {
        m_chronik->handle_tx_invalidated(*tx, *spent_coins);
    }*/
};

std::unique_ptr<ChronikValidationInterface> g_chronik_validation_interface;

void StartChronikValidationInterface(
    const node::NodeContext &node,
    rust::Box<chronik_bridge::Chronik> chronik_box) {
    g_chronik_validation_interface =
        std::make_unique<ChronikValidationInterface>(node,
                                                     std::move(chronik_box));
    g_chronik_validation_interface->Register();
}

void StopChronikValidationInterface() {
    if (g_chronik_validation_interface) {
        g_chronik_validation_interface->Unregister();
        g_chronik_validation_interface->StopChronik();
        // Reset so the Box is dropped and all handles are released.
        g_chronik_validation_interface.reset();
    }
}

} // namespace chronik
