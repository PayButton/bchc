// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Unit tests for denial-of-service detection/prevention code

#include <banman.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <net.h>
#include <net_processing.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <serialize.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>

#include <test/util/net.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>

namespace {
struct CConnmanTest : public CConnman {
    using CConnman::CConnman;
    void AddNode(CNode &node) {
        LOCK(cs_vNodes);
        vNodes.push_back(&node);
    }
    void ClearNodes() {
        LOCK(cs_vNodes);
        for (CNode *node : vNodes) {
            delete node;
        }
        vNodes.clear();
    }
};
} // namespace

// Tests these internal-to-net_processing.cpp methods:
extern bool AddOrphanTx(const CTransactionRef &tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);

struct COrphanTx {
    CTransactionRef tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
extern std::map<uint256, COrphanTx>
    mapOrphanTransactions GUARDED_BY(g_cs_orphans);

static CService ip(uint32_t i) {
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

static NodeId id = 0;

void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds);

BOOST_FIXTURE_TEST_SUITE(denialofservice_tests, TestingSetup)

// Test eviction of an outbound peer whose chain never advances
// Mock a node connection, and use mocktime to simulate a peer which never sends
// any headers messages. PeerLogic should decide to evict that outbound peer,
// after the appropriate timeouts.
// Note that we protect 4 outbound nodes from being subject to this logic; this
// test takes advantage of that protection only being applied to nodes which
// send headers with sufficient work.
BOOST_AUTO_TEST_CASE(outbound_slow_chain_eviction) {
    const Config &config = GetConfig();

    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    // Disable inactivity checks for this test to avoid interference
    static_cast<ConnmanTestMsg *>(connman.get())->SetPeerConnectTimeout(99999s);
    auto peerLogic = PeerManager::make(
        config.GetChainParams(), *connman, nullptr, *m_node.scheduler,
        *m_node.chainman, *m_node.mempool, false);

    // Mock an outbound peer
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, ServiceFlags(NODE_NETWORK), INVALID_SOCKET, addr1,
                     /* nKeyedNetGroupIn */ 0, /* nLocalHostNonceIn */ 0,
                     /* nLocalExtraEntropyIn */ 0, CAddress(), /* pszDest */ "",
                     ConnectionType::OUTBOUND_FULL_RELAY,
                     /* inbound_onion */ false);
    dummyNode1.SetCommonVersion(PROTOCOL_VERSION);

    peerLogic->InitializeNode(config, &dummyNode1);
    dummyNode1.fSuccessfullyConnected = true;

    // This test requires that we have a chain with non-zero work.
    {
        LOCK(cs_main);
        BOOST_CHECK(::ChainActive().Tip() != nullptr);
        BOOST_CHECK(::ChainActive().Tip()->nChainWork > 0);
    }

    // Test starts here
    {
        LOCK(dummyNode1.cs_sendProcessing);
        // should result in getheaders
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode1));
    }
    {
        LOCK(dummyNode1.cs_vSend);
        BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
        dummyNode1.vSendMsg.clear();
    }

    int64_t nStartTime = GetTime();
    // Wait 21 minutes
    SetMockTime(nStartTime + 21 * 60);
    {
        LOCK(dummyNode1.cs_sendProcessing);
        // should result in getheaders
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode1));
    }
    {
        LOCK(dummyNode1.cs_vSend);
        BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
    }
    // Wait 3 more minutes
    SetMockTime(nStartTime + 24 * 60);
    {
        LOCK(dummyNode1.cs_sendProcessing);
        // should result in disconnect
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode1));
    }
    BOOST_CHECK(dummyNode1.fDisconnect == true);
    SetMockTime(0);

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode1, dummy);
}

static void AddRandomOutboundPeer(const Config &config,
                                  std::vector<CNode *> &vNodes,
                                  PeerManager &peerLogic,
                                  CConnmanTest *connman) {
    CAddress addr(ip(g_insecure_rand_ctx.randbits(32)), NODE_NONE);
    vNodes.emplace_back(
        new CNode(id++, ServiceFlags(NODE_NETWORK), INVALID_SOCKET, addr,
                  /* nKeyedNetGroupIn */ 0,
                  /* nLocalHostNonceIn */ 0,
                  /* nLocalExtraEntropyIn */ 0, CAddress(),
                  /* pszDest */ "", ConnectionType::OUTBOUND_FULL_RELAY,
                  /* inbound_onion */ false));
    CNode &node = *vNodes.back();
    node.SetCommonVersion(PROTOCOL_VERSION);

    peerLogic.InitializeNode(config, &node);
    node.fSuccessfullyConnected = true;

    connman->AddNode(node);
}

BOOST_AUTO_TEST_CASE(stale_tip_peer_management) {
    const Config &config = GetConfig();

    auto connman = std::make_unique<CConnmanTest>(config, 0x1337, 0x1337);
    auto peerLogic = PeerManager::make(
        config.GetChainParams(), *connman, nullptr, *m_node.scheduler,
        *m_node.chainman, *m_node.mempool, false);

    const Consensus::Params &consensusParams =
        config.GetChainParams().GetConsensus();
    constexpr int max_outbound_full_relay = MAX_OUTBOUND_FULL_RELAY_CONNECTIONS;
    CConnman::Options options;
    options.nMaxConnections = DEFAULT_MAX_PEER_CONNECTIONS;
    options.m_max_outbound_full_relay = max_outbound_full_relay;
    options.nMaxFeeler = MAX_FEELER_CONNECTIONS;

    const auto time_init{GetTime<std::chrono::seconds>()};
    SetMockTime(time_init.count());
    const auto time_later{
        time_init +
        3 * std::chrono::seconds{consensusParams.nPowTargetSpacing} + 1s};
    connman->Init(options);
    std::vector<CNode *> vNodes;

    // Mock some outbound peers
    for (int i = 0; i < max_outbound_full_relay; ++i) {
        AddRandomOutboundPeer(config, vNodes, *peerLogic, connman.get());
    }

    peerLogic->CheckForStaleTipAndEvictPeers();

    // No nodes should be marked for disconnection while we have no extra peers
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    SetMockTime(time_later.count());

    // Now tip should definitely be stale, and we should look for an extra
    // outbound peer
    peerLogic->CheckForStaleTipAndEvictPeers();
    BOOST_CHECK(connman->GetTryNewOutboundPeer());

    // Still no peers should be marked for disconnection
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    // If we add one more peer, something should get marked for eviction
    // on the next check (since we're mocking the time to be in the future, the
    // required time connected check should be satisfied).
    SetMockTime(time_init.count());
    AddRandomOutboundPeer(config, vNodes, *peerLogic, connman.get());
    SetMockTime(time_later.count());

    peerLogic->CheckForStaleTipAndEvictPeers();
    for (int i = 0; i < max_outbound_full_relay; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    // Last added node should get marked for eviction
    BOOST_CHECK(vNodes.back()->fDisconnect == true);

    vNodes.back()->fDisconnect = false;

    // Update the last announced block time for the last
    // peer, and check that the next newest node gets evicted.
    UpdateLastBlockAnnounceTime(vNodes.back()->GetId(), GetTime());

    peerLogic->CheckForStaleTipAndEvictPeers();
    for (int i = 0; i < max_outbound_full_relay - 1; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    BOOST_CHECK(vNodes[max_outbound_full_relay - 1]->fDisconnect == true);
    BOOST_CHECK(vNodes.back()->fDisconnect == false);

    bool dummy;
    for (const CNode *node : vNodes) {
        peerLogic->FinalizeNode(config, *node, dummy);
    }

    connman->ClearNodes();
}

BOOST_AUTO_TEST_CASE(peer_discouragement) {
    const Config &config = GetConfig();

    auto banman = std::make_unique<BanMan>(
        m_args.GetDataDirPath() / "banlist.dat", config.GetChainParams(),
        nullptr, DEFAULT_MISBEHAVING_BANTIME);
    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = PeerManager::make(
        config.GetChainParams(), *connman, banman.get(), *m_node.scheduler,
        *m_node.chainman, *m_node.mempool, false);

    banman->ClearBanned();
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, INVALID_SOCKET, addr1,
                     /* nKeyedNetGroupIn */ 0, /* nLocalHostNonceIn */ 0,
                     /* nLocalExtraEntropyIn */ 0, CAddress(), /* pszDest */ "",
                     ConnectionType::INBOUND, /* inbound_onion */ false);
    dummyNode1.SetCommonVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode1);
    dummyNode1.fSuccessfullyConnected = true;
    // Should be discouraged
    peerLogic->Misbehaving(dummyNode1.GetId(), DISCOURAGEMENT_THRESHOLD,
                           /* message */ "");
    {
        LOCK(dummyNode1.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode1));
    }
    BOOST_CHECK(banman->IsDiscouraged(addr1));
    // Different IP, not discouraged
    BOOST_CHECK(!banman->IsDiscouraged(ip(0xa0b0c001 | 0x0000ff00)));

    CAddress addr2(ip(0xa0b0c002), NODE_NONE);
    CNode dummyNode2(id++, NODE_NETWORK, INVALID_SOCKET, addr2,
                     /* nKeyedNetGroupIn */ 1, /* nLocalHostNonceIn */ 1,
                     /* nLocalExtraEntropyIn */ 1, CAddress(),
                     /* pszDest */ "", ConnectionType::INBOUND,
                     /* inbound_onion */ false);
    dummyNode2.SetCommonVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode2);
    dummyNode2.fSuccessfullyConnected = true;
    peerLogic->Misbehaving(dummyNode2.GetId(), DISCOURAGEMENT_THRESHOLD - 1,
                           /* message */ "");
    {
        LOCK(dummyNode2.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode2));
    }
    // 2 not discouraged yet...
    BOOST_CHECK(!banman->IsDiscouraged(addr2));
    // ... but 1 still should be
    BOOST_CHECK(banman->IsDiscouraged(addr1));
    // 2 reaches discouragement threshold
    peerLogic->Misbehaving(dummyNode2.GetId(), 1, /* message */ "");
    {
        LOCK(dummyNode2.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode2));
    }
    BOOST_CHECK(banman->IsDiscouraged(addr1)); // Expect both 1 and 2
    BOOST_CHECK(banman->IsDiscouraged(addr2)); // to be discouraged now

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode1, dummy);
    peerLogic->FinalizeNode(config, dummyNode2, dummy);
}

BOOST_AUTO_TEST_CASE(DoS_bantime) {
    const Config &config = GetConfig();

    auto banman = std::make_unique<BanMan>(
        m_args.GetDataDirPath() / "banlist.dat", config.GetChainParams(),
        nullptr, DEFAULT_MISBEHAVING_BANTIME);
    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = PeerManager::make(
        config.GetChainParams(), *connman, banman.get(), *m_node.scheduler,
        *m_node.chainman, *m_node.mempool, false);

    banman->ClearBanned();
    int64_t nStartTime = GetTime();
    // Overrides future calls to GetTime()
    SetMockTime(nStartTime);

    CAddress addr(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode(id++, NODE_NETWORK, INVALID_SOCKET, addr,
                    /* nKeyedNetGroupIn */ 4, /* nLocalHostNonceIn */ 4,
                    /* nLocalExtraEntropyIn */ 4, CAddress(), /* pszDest */ "",
                    ConnectionType::INBOUND, /* inbound_onion */ false);
    dummyNode.SetCommonVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode);
    dummyNode.fSuccessfullyConnected = true;

    peerLogic->Misbehaving(dummyNode.GetId(), DISCOURAGEMENT_THRESHOLD,
                           /* message */ "");
    {
        LOCK(dummyNode.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(config, &dummyNode));
    }
    BOOST_CHECK(banman->IsDiscouraged(addr));

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode, dummy);
}

static CTransactionRef RandomOrphan() {
    std::map<uint256, COrphanTx>::iterator it;
    LOCK2(cs_main, g_cs_orphans);
    it = mapOrphanTransactions.lower_bound(InsecureRand256());
    if (it == mapOrphanTransactions.end()) {
        it = mapOrphanTransactions.begin();
    }
    return it->second.tx;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans) {
    CKey key;
    key.MakeNewKey(true);
    FillableSigningProvider keystore;
    BOOST_CHECK(keystore.AddKey(key));

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++) {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(TxId(InsecureRand256()), 0);
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(PKHash(key.GetPubKey()));

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++) {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(txPrev->GetId(), 0);
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(PKHash(key.GetPubKey()));
        BOOST_CHECK(SignSignature(keystore, *txPrev, tx, 0,
                                  SigHashType().withForkId()));

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++) {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(PKHash(key.GetPubKey()));
        tx.vin.resize(2777);
        for (size_t j = 0; j < tx.vin.size(); j++) {
            tx.vin[j].prevout = COutPoint(txPrev->GetId(), j);
        }
        BOOST_CHECK(SignSignature(keystore, *txPrev, tx, 0,
                                  SigHashType().withForkId()));
        // Re-use same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++) {
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;
        }

        BOOST_CHECK(!AddOrphanTx(MakeTransactionRef(tx), i));
    }

    LOCK2(cs_main, g_cs_orphans);
    // Test EraseOrphansFor:
    for (NodeId i = 0; i < 3; i++) {
        size_t sizeBefore = mapOrphanTransactions.size();
        EraseOrphansFor(i);
        BOOST_CHECK(mapOrphanTransactions.size() < sizeBefore);
    }

    // Test LimitOrphanTxSize() function:
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
}

BOOST_AUTO_TEST_SUITE_END()
