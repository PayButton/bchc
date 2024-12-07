// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <httpserver.h>
#include <rpc/server.h>

#include <any>
#include <map>
#include <string>

class Config;

class HTTPRPCRequestProcessor {
private:
    Config &config;
    RPCServer &rpcServer;

    bool ProcessHTTPRequest(const std::any& context, HTTPRequest *request);

public:
    HTTPRPCRequestProcessor(Config &configIn, RPCServer &rpcServerIn)
        : config(configIn), rpcServer(rpcServerIn) {}

    static bool DelegateHTTPRequest(const std::any& context, HTTPRPCRequestProcessor *requestProcessor,
                                    HTTPRequest *request) {
        return requestProcessor->ProcessHTTPRequest(context, request);
    }
};

/**
 * Start HTTP RPC subsystem.
 * Precondition; HTTP and RPC has been started.
 */
bool StartHTTPRPC(HTTPRPCRequestProcessor &httpRPCRequestProcessor, const std::any& context);

/** Interrupt HTTP RPC subsystem */
void InterruptHTTPRPC();

/**
 * Stop HTTP RPC subsystem.
 * Precondition; HTTP and RPC has been stopped.
 */
void StopHTTPRPC();

/**
 * Start HTTP REST subsystem.
 * Precondition; HTTP and RPC has been started.
 */
void StartREST(const std::any& context);

/** Interrupt RPC REST subsystem */
void InterruptREST();

/**
 * Stop HTTP REST subsystem.
 * Precondition; HTTP and RPC has been stopped.
 */
void StopREST();
