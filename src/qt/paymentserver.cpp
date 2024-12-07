// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/paymentserver.h>

#include <cashaddrenc.h>
#include <chainparams.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/policy.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <ui_interface.h>
#include <util/system.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileOpenEvent>
#include <QHash>
#include <QList>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStringList>
#include <QUrlQuery>

#include <cstdlib>
#include <memory>

const int BITCOIN_IPC_CONNECT_TIMEOUT = 1000; // milliseconds

//
// Create a name that is unique for:
//  testnet / non-testnet
//  data directory
//
static QString ipcServerName() {
    QString name("BitcoinQt");

    // Append a simple hash of the datadir
    // Note that GetDataDir(true) returns a different path for -testnet versus
    // main net
    QString ddir(GUIUtil::boostPathToQString(GetDataDir(true)));
    name.append(QString::number(qHash(ddir)));

    return name;
}

//
// We store payment URIs and requests received before the main GUI window is up
// and ready to ask the user to send payment.
//
static QList<QString> savedPaymentRequests;

static std::string ipcParseURI(const QString &arg, const CChainParams &params,
                               bool useCashAddr) {
    const QString scheme = QString::fromStdString(params.CashAddrPrefix());
    if (!arg.startsWith(scheme + ":", Qt::CaseInsensitive)) {
        return {};
    }

    SendCoinsRecipient r;
    if (!GUIUtil::parseBitcoinURI(scheme, arg, &r)) {
        return {};
    }

    return r.address.toStdString();
}

static bool ipcCanParseCashAddrURI(const QString &arg,
                                   const std::string &network) {
    auto tempChainParams = CreateChainParams(network);
    std::string addr = ipcParseURI(arg, *tempChainParams, true);
    return IsValidDestinationString(addr, *tempChainParams);
}

static bool ipcCanParseLegacyURI(const QString &arg,
                                 const std::string &network) {
    auto tempChainParams = CreateChainParams(network);
    std::string addr = ipcParseURI(arg, *tempChainParams, false);
    return IsValidDestinationString(addr, *tempChainParams);
}

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues, and the items in
// savedPaymentRequest will be handled when uiReady() is called.
//
// Warning: ipcSendCommandLine() is called early in init, so don't use "Q_EMIT
// message()", but "QMessageBox::"!
//
void PaymentServer::ipcParseCommandLine(interfaces::Node &node, int argc,
                                        char *argv[]) {
    std::array<const std::string *, 5> networks = {
        {&CBaseChainParams::MAIN, &CBaseChainParams::TESTNET, &CBaseChainParams::TESTNET4, &CBaseChainParams::CHIPNET,
         &CBaseChainParams::REGTEST}};

    const std::string *chosenNetwork = nullptr;

    for (int i = 1; i < argc; i++) {
        QString arg(argv[i]);
        if (arg.startsWith("-")) {
            continue;
        }

        const std::string *itemNetwork = nullptr;

        // Try to parse as a URI
        for (auto net : networks) {
            if (ipcCanParseCashAddrURI(arg, *net)) {
                itemNetwork = net;
                break;
            }

            if (ipcCanParseLegacyURI(arg, *net)) {
                itemNetwork = net;
                break;
            }
        }

        if (itemNetwork == nullptr) {
            // Printing to debug.log is about the best we can do here, the GUI
            // hasn't started yet so we can't pop up a message box.
            qWarning() << "PaymentServer::ipcSendCommandLine: Payment request "
                          "file or URI does not exist or is invalid: "
                       << arg;
            continue;
        }

        chosenNetwork = itemNetwork;
    }

    if (chosenNetwork) {
        node.selectParams(*chosenNetwork);
    }
}

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues, and the items in
// savedPaymentRequest will be handled when uiReady() is called.
//
bool PaymentServer::ipcSendCommandLine() {
    bool fResult = false;
    for (const QString &r : savedPaymentRequests) {
        QLocalSocket *socket = new QLocalSocket();
        socket->connectToServer(ipcServerName(), QIODevice::WriteOnly);
        if (!socket->waitForConnected(BITCOIN_IPC_CONNECT_TIMEOUT)) {
            delete socket;
            socket = nullptr;
            return false;
        }

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);
        out << r;
        out.device()->seek(0);

        socket->write(block);
        socket->flush();
        socket->waitForBytesWritten(BITCOIN_IPC_CONNECT_TIMEOUT);
        socket->disconnectFromServer();

        delete socket;
        socket = nullptr;
        fResult = true;
    }

    return fResult;
}

PaymentServer::PaymentServer(QObject *parent, bool startLocalServer)
    : QObject(parent), saveURIs(true), uriServer(nullptr), optionsModel(nullptr)
{
    // Install global event filter to catch QFileOpenEvents
    // on Mac: sent when you click bitcoincash: links
    // other OSes: helpful when dealing with payment request files
    if (parent) {
        parent->installEventFilter(this);
    }

    QString name = ipcServerName();

    // Clean up old socket leftover from a crash:
    QLocalServer::removeServer(name);

    if (startLocalServer) {
        uriServer = new QLocalServer(this);

        if (!uriServer->listen(name)) {
            // constructor is called early in init, so don't use "Q_EMIT
            // message()" here
            QMessageBox::critical(nullptr, tr("Payment request error"),
                                  tr("Cannot start click-to-pay handler"));
        } else {
            connect(uriServer, &QLocalServer::newConnection, this,
                    &PaymentServer::handleURIConnection);
        }
    }
}

PaymentServer::~PaymentServer() {
}

//
// OSX-specific way of handling bitcoincash: URIs
//
bool PaymentServer::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *fileEvent = static_cast<QFileOpenEvent *>(event);
        if (!fileEvent->file().isEmpty()) {
            handleURIOrFile(fileEvent->file());
        } else if (!fileEvent->url().isEmpty()) {
            handleURIOrFile(fileEvent->url().toString());
        }

        return true;
    }

    return QObject::eventFilter(object, event);
}

void PaymentServer::uiReady() {
    saveURIs = false;
    for (const QString &s : savedPaymentRequests) {
        handleURIOrFile(s);
    }
    savedPaymentRequests.clear();
}

bool PaymentServer::handleURI(const CChainParams &params, const QString &s) {
    const QString scheme = QString::fromStdString(params.CashAddrPrefix());
    if (!s.startsWith(scheme + ":", Qt::CaseInsensitive)) {
        return false;
    }

    QUrlQuery uri((QUrl(s)));
    // payment request URI
    if (uri.hasQueryItem("r")) {
        Q_EMIT message(tr("URI handling"),
                       tr("Cannot process payment request because BIP70 is not supported."),
                       CClientUIInterface::ICON_WARNING);
        return true;
    }

    // normal URI
    SendCoinsRecipient recipient;
    if (GUIUtil::parseBitcoinURI(scheme, s, &recipient)) {
        if (!IsValidDestinationString(recipient.address.toStdString(),
                                      params)) {
            Q_EMIT message(
                tr("URI handling"),
                tr("Invalid payment address %1").arg(recipient.address),
                CClientUIInterface::MSG_ERROR);
        } else {
            Q_EMIT receivedPaymentRequest(recipient);
        }
    } else {
        Q_EMIT message(
            tr("URI handling"),
            tr("URI cannot be parsed! This can be caused by an invalid "
               "Bitcoin Cash address or malformed URI parameters."),
            CClientUIInterface::ICON_WARNING);
    }

    return true;
}

void PaymentServer::handleURIOrFile(const QString &s) {
    if (saveURIs) {
        savedPaymentRequests.append(s);
        return;
    }

    // bitcoincash: CashAddr URI
    if (handleURI(Params(), s)) {
        return;
    }
}

void PaymentServer::handleURIConnection() {
    QLocalSocket *clientConnection = uriServer->nextPendingConnection();

    while (clientConnection->bytesAvailable() < (int)sizeof(quint32)) {
        clientConnection->waitForReadyRead();
    }

    connect(clientConnection, &QLocalSocket::disconnected, clientConnection,
            &QLocalSocket::deleteLater);

    QDataStream in(clientConnection);
    in.setVersion(QDataStream::Qt_4_0);
    if (clientConnection->bytesAvailable() < (int)sizeof(quint16)) {
        return;
    }
    QString msg;
    in >> msg;

    handleURIOrFile(msg);
}

void PaymentServer::setOptionsModel(OptionsModel *_optionsModel) {
    this->optionsModel = _optionsModel;
}
