// Copyright (c) 2017-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

constexpr int MAX_LABEL_LENGTH = 63;
constexpr int MAX_QUERY_NAME_LENGTH = 255;
// Max size of the null-terminated buffer parse_name() writes to.
constexpr int MAX_QUERY_NAME_BUFFER_LENGTH = MAX_QUERY_NAME_LENGTH + 1;

/// Encapsulates either an IPv4 or IPv6 address, depending on whether member .v == 4 or .v == 6
struct AddrGeneric {
    int v;
    union {
        uint8_t v4[4];
        uint8_t v6[16];
    } data;
};

struct DnsServer {
    const int port;
    const int datattl;
    const int nsttl;
    const char * const host;
    const char * const ns;
    const char * const mbox;
    // stats
    std::atomic_uint64_t nRequests = 0;

    virtual ~DnsServer();

    // Runs the dns server. Doesn't return until it exits (usually at app exit).
    // Returns a nullopt if the server exited ok. Otherwise returns an error string if there was an error.
    std::optional<std::string> run();

    virtual uint32_t GetIPList(const char *requestedHostname, AddrGeneric *addr, uint32_t max, bool ipv4, bool ipv6) = 0;

    // Signals the dns server internals to close the shared socket and abort operations (called on app shutdown)
    static void Shutdown();

protected:
    DnsServer(int port, const char *host, const char *ns, const char *mbox = nullptr, int datattl = 3600, int nsttl = 40000);

private:
    ssize_t handle(const uint8_t *inbuf, size_t insize, uint8_t *outbuf);
};

enum class ParseNameStatus {
    OK,
    // Premature end of input, forward reference, component > 63 char, invalid
    // character
    InputError,
    // Insufficient space in output
    OutputBufferError,
};

ParseNameStatus parse_name(const uint8_t **inpos, const uint8_t *inend,
                           const uint8_t *inbuf, char *buf, size_t bufsize);

//  0: k
// -1: component > 63 characters
// -2: insufficient space in output
// -3: two subsequent dots
int write_name(uint8_t **outpos, const uint8_t *outend, const char *name,
               int offset);
