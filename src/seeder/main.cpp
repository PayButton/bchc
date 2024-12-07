// Copyright (c) 2017-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <clientversion.h>
#include <fs.h>
#include <logging.h>
#include <protocol.h>
#include <random.h>
#include <seeder/bitcoin.h>
#include <seeder/db.h>
#include <seeder/dns.h>
#include <seeder/util.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/defer.h>
#include <util/strencodings.h>
#include <util/syserror.h>
#include <util/system.h>

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <thread>
#include <typeinfo>
#include <utility>

#include <pthread.h>
#include <strings.h> // for strcasecmp
#include <unistd.h>

const std::function<std::string(const char *)> G_TRANSLATION_FUN = nullptr;

//! All globals in this file are private to this translation unit
namespace {

static constexpr bool DEBUG_THREAD_LIFETIMES = false; // set to true to see debug messages for when threads exit

static const int CONTINUE_EXECUTION = -1;

static constexpr int DEFAULT_NUM_THREADS = 96;
static constexpr int DEFAULT_PORT = 53;
static constexpr int DEFAULT_NUM_DNS_THREADS = 4;
static constexpr bool DEFAULT_WIPE_BAN = false;
static constexpr bool DEFAULT_RESEED = false;
static const std::string DEFAULT_EMAIL = "";
static const std::string DEFAULT_NAMESERVER = "";
static const std::string DEFAULT_HOST = "";
static const std::string DEFAULT_TOR_PROXY = "";
static const std::string DEFAULT_IPV4_PROXY = "";
static const std::string DEFAULT_IPV6_PROXY = "";

class CDnsSeedOpts {
public:
    int nThreads            = DEFAULT_NUM_THREADS;
    int nPort               = DEFAULT_PORT;
    int nDnsThreads         = DEFAULT_NUM_DNS_THREADS;
    bool fWipeBan           = DEFAULT_WIPE_BAN;
    bool fReseed            = DEFAULT_RESEED;
    std::string mbox        = DEFAULT_EMAIL;
    std::string ns          = DEFAULT_NAMESERVER;
    std::string host        = DEFAULT_HOST;
    std::string tor         = DEFAULT_TOR_PROXY;
    std::string ipv4_proxy  = DEFAULT_IPV4_PROXY;
    std::string ipv6_proxy  = DEFAULT_IPV6_PROXY;
    std::set<uint64_t> filter_whitelist;

    int ParseCommandLine(int argc, char **argv) {
        SetupSeederArgs();
        std::string error;
        if (!gArgs.ParseParameters(argc, argv, error)) {
            std::fprintf(stderr, "Error parsing command line arguments: %s\n",
                         error.c_str());
            return EXIT_FAILURE;
        }
        if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
            std::string strUsage =
                PACKAGE_NAME " Seeder " + FormatFullVersion() + "\n";
            if (HelpRequested(gArgs)) {
                strUsage +=
                    "\nUsage:  bitcoin-seeder -host=<host> -ns=<ns> "
                    "[-mbox=<mbox>] [-threads=<threads>] [-port=<port>]\n\n" +
                    gArgs.GetHelpMessage();
            }

            std::fprintf(stdout, "%s", strUsage.c_str());
            return EXIT_SUCCESS;
        }

        nThreads = gArgs.GetArg("-threads", DEFAULT_NUM_THREADS);
        nPort = gArgs.GetArg("-port", DEFAULT_PORT);
        nDnsThreads = gArgs.GetArg("-dnsthreads", DEFAULT_NUM_DNS_THREADS);
        fWipeBan = gArgs.GetBoolArg("-wipeban", DEFAULT_WIPE_BAN);
        fReseed = gArgs.GetBoolArg("-reseed", DEFAULT_RESEED);
        mbox = gArgs.GetArg("-mbox", DEFAULT_EMAIL);
        ns = gArgs.GetArg("-ns", DEFAULT_NAMESERVER);
        host = gArgs.GetArg("-host", DEFAULT_HOST);
        tor = gArgs.GetArg("-onion", DEFAULT_TOR_PROXY);
        ipv4_proxy = gArgs.GetArg("-proxyipv4", DEFAULT_IPV4_PROXY);
        ipv6_proxy = gArgs.GetArg("-proxyipv6", DEFAULT_IPV6_PROXY);
        SelectParams(gArgs.GetChainName());

        if (gArgs.IsArgSet("-filter")) {
            // Parse whitelist additions
            std::string flagString = gArgs.GetArg("-filter", "");
            size_t flagstartpos = 0;
            while (flagstartpos < flagString.size()) {
                size_t flagendpos = flagString.find_first_of(',', flagstartpos);
                uint64_t flag = atoi64(flagString.substr(
                    flagstartpos, (flagendpos - flagstartpos)));
                filter_whitelist.insert(flag);
                if (flagendpos == std::string::npos) {
                    break;
                }
                flagstartpos = flagendpos + 1;
            }
        }
        if (filter_whitelist.empty()) {
            filter_whitelist.insert(NODE_NETWORK);
            filter_whitelist.insert(NODE_NETWORK | NODE_BLOOM);
            filter_whitelist.insert(NODE_NETWORK | NODE_XTHIN);
            filter_whitelist.insert(NODE_NETWORK | NODE_BLOOM | NODE_XTHIN);
        }
        return CONTINUE_EXECUTION;
    }

private:
    void SetupSeederArgs() {
        SetupHelpOptions(gArgs);
        gArgs.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-host=<host>", "Hostname of the DNS seed", ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-ns=<ns>", "Hostname of the nameserver", ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-mbox=<mbox>",
                     "E-Mail address reported in SOA records", ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-threads=<threads>",
                     strprintf("Number of crawlers to run in parallel (default: %d)", DEFAULT_NUM_THREADS),
                     ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
        gArgs.AddArg("-dnsthreads=<threads>",
                     strprintf("Number of DNS server threads (default: %d)", DEFAULT_NUM_DNS_THREADS), ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-port=<port>", strprintf("UDP port to listen on (default: %d)", DEFAULT_PORT),
                     ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
        gArgs.AddArg("-onion=<ip:port>", "Tor proxy IP/Port", ArgsManager::ALLOW_ANY,
                     OptionsCategory::CONNECTION);
        gArgs.AddArg("-proxyipv4=<ip:port>", "IPV4 SOCKS5 proxy IP/Port",
                     ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
        gArgs.AddArg("-proxyipv6=<ip:port>", "IPV6 SOCKS5 proxy IP/Port",
                     ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
        gArgs.AddArg("-filter=<f1,f2,...>",
                     "Allow these flag combinations as filters", ArgsManager::ALLOW_ANY,
                     OptionsCategory::OPTIONS);
        gArgs.AddArg("-wipeban", strprintf("Wipe list of banned nodes (default: %d)", DEFAULT_WIPE_BAN), ArgsManager::ALLOW_ANY,
                     OptionsCategory::CONNECTION);
        gArgs.AddArg("-reseed", strprintf("Reseed the database from the fixed seed list (default: %d)", DEFAULT_RESEED), ArgsManager::ALLOW_ANY,
                     OptionsCategory::CONNECTION);
        SetupChainParamsBaseOptions();
    }
};

CAddrDb db;

struct CrawlerArg {
    uint16_t threadNum;
    uint16_t nThreads;
};

static_assert(sizeof(void *) >= sizeof(CrawlerArg));

extern "C" void *ThreadCrawler(void *data) {
    static std::atomic_int extantThreads = 0;
    const CrawlerArg arg = [&]{
        // unpack arg by copying "pointer" bytes into struct CrawlerArg
        CrawlerArg ret;
        std::memcpy(&ret, &data, sizeof(ret));
        return ret;
    }();
    Defer d([&]{
        const int nleft = --extantThreads;
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "Crawler thread %u/%u exit%s\n", arg.threadNum, arg.nThreads,
                         nleft ? strprintf(" (%d threads still alive)", nleft).c_str() : "");
        }
    });
    ++extantThreads;
    FastRandomContext rng;
    do {
        std::vector<CServiceResult> ips;
        db.GetMany(ips, 16);
        int64_t now = std::time(nullptr);
        if (ips.empty()) {
            if ( ! seeder::SleepAndPollShutdownFlag(5000 + rng.randrange(500 * arg.nThreads))) {
                break; // shutdown requested...
            }
            continue;
        }

        std::vector<CAddress> addr;
        for (size_t i = 0; !seeder::ShutdownRequested() && i < ips.size(); ++i) {
            CServiceResult &res = ips[i];
            res.nBanTime = 0;
            res.nClientV = 0;
            res.nHeight = 0;
            res.strClientV = "";
            res.services = NODE_NONE;
            bool getaddr = res.lastAddressRequest + 86400 < now;

            res.fGood = TestNode(res.service, res.nBanTime, res.nClientV,
                                 res.strClientV, res.nHeight,
                                 getaddr ? &addr : nullptr,
                                 res.services, res.checkpointVerified);

            if (res.fGood && getaddr)
                res.lastAddressRequest = now;
        }
        if (seeder::ShutdownRequested()) {
            // Since we may have been interrupted at any time during this operation due to shutdown,
            // give back the ips without reporting any result so as to not adversely affect stats.
            db.SkippedMany(ips);
        } else {
            db.ResultMany(ips);
        }
        db.Add(addr);
    } while (!seeder::ShutdownRequested());
    return nullptr;
}

class CDnsThread final : public DnsServer {
public:
    struct FlagSpecificData {
        int nIPv4 = 0, nIPv6 = 0;
        std::vector<AddrGeneric> cache;
        std::time_t cacheTime = 0;
        unsigned int cacheHits = 0;
    };

    const int id;
    std::map<uint64_t, FlagSpecificData> perflag;
    std::atomic<uint64_t> dbQueries{0};
    std::set<uint64_t> filterWhitelist;
    std::thread threadHandle;
    bool hadError = false;
    FastRandomContext rng; // rng used internally by GetIPList

    void cacheHit(uint64_t requestedFlags, bool force = false) {
        static bool nets[NET_MAX] = {};
        if (!nets[NET_IPV4]) {
            nets[NET_IPV4] = true;
            nets[NET_IPV6] = true;
        }
        std::time_t now = std::time(nullptr);
        FlagSpecificData &thisflag = perflag[requestedFlags];
        thisflag.cacheHits++;
        if (force ||
            thisflag.cacheHits * 400 >
                (thisflag.cache.size() * thisflag.cache.size()) ||
            (thisflag.cacheHits * thisflag.cacheHits * 20 >
                 thisflag.cache.size() &&
             (now - thisflag.cacheTime > 5))) {
            std::set<CNetAddr> ips;
            db.GetIPs(ips, requestedFlags, 1000, nets);
            dbQueries++;
            thisflag.cache.clear();
            thisflag.nIPv4 = 0;
            thisflag.nIPv6 = 0;
            thisflag.cache.reserve(ips.size());
            for (auto &ip : ips) {
                struct in_addr addr;
                struct in6_addr addr6;
                if (ip.GetInAddr(&addr)) {
                    AddrGeneric a;
                    a.v = 4;
                    std::memcpy(&a.data.v4, &addr, 4);
                    thisflag.cache.push_back(a);
                    thisflag.nIPv4++;
                } else if (ip.GetIn6Addr(&addr6)) {
                    AddrGeneric a;
                    a.v = 6;
                    std::memcpy(&a.data.v6, &addr6, 16);
                    thisflag.cache.push_back(a);
                    thisflag.nIPv6++;
                }
            }
            thisflag.cacheHits = 0;
            thisflag.cacheTime = now;
        }
    }

    CDnsThread(CDnsSeedOpts *opts, int idIn)
        : DnsServer(opts->nPort, opts->host.c_str(), opts->ns.c_str(), opts->mbox.c_str()),
          id(idIn), filterWhitelist(opts->filter_whitelist) {}

    ~CDnsThread() override = default;

    uint32_t GetIPList(const char *requestedHostname, AddrGeneric *addr, uint32_t max, bool ipv4, bool ipv6) override;
};

uint32_t CDnsThread::GetIPList(const char *requestedHostname, AddrGeneric *addr, uint32_t max, bool ipv4, bool ipv6) {
    uint64_t requestedFlags = 0;
    int hostlen = std::strlen(requestedHostname);
    if (hostlen > 1 && requestedHostname[0] == 'x' &&
        requestedHostname[1] != '0') {
        char *pEnd;
        uint64_t flags = uint64_t(std::strtoull(requestedHostname + 1, &pEnd, 16));
        if (*pEnd == '.' && pEnd <= requestedHostname + 17 &&
            std::find(this->filterWhitelist.begin(),
                      this->filterWhitelist.end(),
                      flags) != this->filterWhitelist.end()) {
            requestedFlags = flags;
        } else {
            return 0;
        }
    } else if (strcasecmp(requestedHostname, this->host)) {
        return 0;
    }
    this->cacheHit(requestedFlags);
    auto &thisflag = this->perflag[requestedFlags];
    uint32_t size = thisflag.cache.size();
    uint32_t maxmax = (ipv4 ? thisflag.nIPv4 : 0) + (ipv6 ? thisflag.nIPv6 : 0);
    if (max > size) {
        max = size;
    }
    if (max > maxmax) {
        max = maxmax;
    }
    uint32_t i = 0;
    while (i < max) {
        uint32_t j = i + this->rng.randrange(size - i);
        do {
            bool ok = (ipv4 && thisflag.cache[j].v == 4) ||
                      (ipv6 && thisflag.cache[j].v == 6);
            if (ok) {
                break;
            }
            j++;
            if (j == size) {
                j = i;
            }
        } while (1);
        addr[i] = thisflag.cache[j];
        thisflag.cache[j] = thisflag.cache[i];
        thisflag.cache[i] = addr[i];
        i++;
    }
    return max;
}

std::vector<std::unique_ptr<CDnsThread>> dnsThreads;

void ThreadDNS(CDnsThread *thread) {
    Defer d([&]{
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "ThreadDNS %d exit\n", thread->id);
        }
    });
    const auto optError = thread->run();
    if ((thread->hadError = bool(optError))) {
        std::fprintf(stderr, "\nWARNING: DNS thread %d exited with error: %s\n", thread->id, optError->c_str());
        seeder::RequestShutdown();
    }
}

bool StatCompare(const CAddrReport &a, const CAddrReport &b) noexcept {
    if (a.uptime[4] == b.uptime[4]) {
        if (a.uptime[3] == b.uptime[3]) {
            return a.clientVersion > b.clientVersion;
        } else {
            return a.uptime[3] > b.uptime[3];
        }
    } else {
        return a.uptime[4] > b.uptime[4];
    }
}

void SaveAllToDisk() {
    auto PrintCantOpenMsg = [](const char *fname) {
        std::fprintf(stderr, "WARNING: Unable to open file '%s': %s\n", fname, SysErrorString(errno).c_str());
    };
    std::vector<CAddrReport> v = db.GetAll();
    std::sort(v.begin(), v.end(), StatCompare);
    FILE *f = fsbridge::fopen("dnsseed.dat.new", "w+");
    if (f) {
        try {
            {
                CAutoFile cf(f, SER_DISK, CLIENT_VERSION);
                cf << db;
            }
            std::rename("dnsseed.dat.new", "dnsseed.dat");
        } catch (const std::exception &e) {
            std::fprintf(stderr, "WARNING: Unable to save dnsseed.dat, caught exception (%s): %s\n",
                         typeid(e).name(), e.what());
        }
    } else {
        // This may happen if we run out of file descriptors
        PrintCantOpenMsg("dnsseed.dat.new");
        return;
    }
    FILE *d = fsbridge::fopen("dnsseed.dump", "w");
    if (!d) {
        // This may happen if we run out of file descriptors
        PrintCantOpenMsg("dnsseed.dump");
        return;
    }
    std::fprintf(d, "# address                                        good  "
                    "lastSuccess    %%(2h)   %%(8h)   %%(1d)   %%(7d)  "
                    "%%(30d)  blocks      svcs  version\n");
    double stat[5] = {0, 0, 0, 0, 0};
    for (const CAddrReport &rep : v) {
        std::fprintf(
            d,
            "%-47s  %4d  %11" PRId64
            "  %6.2f%% %6.2f%% %6.2f%% %6.2f%% %6.2f%%  %6i  %08" PRIx64
            "  %5i \"%s\"\n",
            rep.ip.ToString().c_str(), rep.reliableness == Reliableness::OK ? 1 : 0, rep.lastSuccess,
            100.0 * rep.uptime[0], 100.0 * rep.uptime[1],
            100.0 * rep.uptime[2], 100.0 * rep.uptime[3],
            100.0 * rep.uptime[4], rep.blocks, rep.services,
            rep.clientVersion, rep.clientSubVersion.c_str());
        stat[0] += rep.uptime[0];
        stat[1] += rep.uptime[1];
        stat[2] += rep.uptime[2];
        stat[3] += rep.uptime[3];
        stat[4] += rep.uptime[4];
    }
    std::fclose(d);
    FILE *ff = fsbridge::fopen("dnsstats.log", "a");
    if (!ff) {
        // This may happen if we run out of file descriptors
        PrintCantOpenMsg("dnsstats.log");
        return;
    }
    std::fprintf(ff, "%llu %g %g %g %g %g\n",
                 (unsigned long long)(std::time(nullptr)), stat[0], stat[1],
                 stat[2], stat[3], stat[4]);
    std::fclose(ff);
}

void ThreadDumper() {
    Defer cleanup([]{
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "ThreadDumper exit\n");
        }
    });
    int count = 0;
    do {
        // First 100s, than 200s, 400s, 800s, 1600s, and then 3200s forever
        if ( ! seeder::SleepAndPollShutdownFlag(100'000 << count)) {
            break;
        }
        if (count < 5) {
            count++;
        }
        SaveAllToDisk();
    } while (1);
}

void ThreadStats() {
    Defer d([]{
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "ThreadStats exit\n");
        }
    });
    bool first = true;
    size_t lastLineLength = 0;
    auto stdoutIsTerminal = isatty(fileno(stdout)) == 1;

    do {
        char c[256];
        std::time_t tim = std::time(nullptr);
        struct tm *tmp = std::localtime(&tim);
        std::strftime(c, 256, "[%y-%m-%d %H:%M:%S]", tmp);
        CAddrDbStats stats;
        db.GetStats(stats);
        if (stdoutIsTerminal) {
            if (first) {
                first = false;
                // ANSI: create 3 newlines, then move cursor up 3 lines
                std::fprintf(stdout, "\n\n\n\x1b[3A");
            } else {
                // ANSI: delete current line, restore cursor position to saved
                std::fprintf(stdout, "\x1b[2K\x1b[u");
            }
            std::fprintf(stdout, "\x1b[s"); // ANSI: save cursor position
        }
        uint64_t requests = 0;
        uint64_t queries = 0;
        for (const auto &dnsThread : dnsThreads) {
            if (!dnsThread)
                continue;
            requests += dnsThread->nRequests;
            queries += dnsThread->dbQueries;
        }
        // pad the line with spaces to ensure old text from the end is cleared
        const auto line = strprintf("%s %i/%i available (%i tried in %is, %i new, %i active), %i "
                                    "banned; %llu DNS requests, %llu db queries",
                                    c, stats.nGood, stats.nAvail, stats.nTracked, stats.nAge,
                                    stats.nNew, stats.nAvail - stats.nTracked - stats.nNew,
                                    stats.nBanned, (unsigned long long)requests,
                                    (unsigned long long)queries);
        const size_t padLen = line.size() < lastLineLength ? lastLineLength - line.size() : 0;
        const std::string pad(padLen, ' ');
        lastLineLength = line.length();
        std::fprintf(stdout, "%s%s\n", line.c_str(), stdoutIsTerminal ? pad.c_str() : "");
        if ( ! seeder::SleepAndPollShutdownFlag(stdoutIsTerminal ? 1000 : 10'000)) {
            break; // shutdown requested...
        }
    } while (1);
}

static constexpr unsigned int MAX_HOSTS_PER_SEED = 128;

void ThreadSeeder() {
    Defer d([]{
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "ThreadSeeder exit\n");
        }
    });
    do {
        for (const std::string &seed : Params().DNSSeeds()) {
            if (seeder::ShutdownRequested()) break;
            std::vector<CNetAddr> ips;
            LookupHost(seed.c_str(), ips, MAX_HOSTS_PER_SEED, true);
            for (auto &ip : ips) {
                db.Add(CAddress(CService(ip, GetDefaultPort()), ServiceFlags()), true);
            }
        }
        if ( ! seeder::SleepAndPollShutdownFlag(1800'000)) {
            break; // shutdown requested...
        }
    } while (1);
}

int asyncSignalPipes[2] = {-1, -1};

extern "C" void signalHandler(int sig) {
    // This is one of the few things that is safe to do in a signal handler, hence this pipe mechanism to notify
    // ThreadAppShutdownNotifier.
    auto ign [[maybe_unused]] = write(asyncSignalPipes[1], &sig, sizeof(sig));
}

void ThreadAppShutdownNotifier() {
    Defer d([]{
        if constexpr (DEBUG_THREAD_LIFETIMES) {
            std::fprintf(stderr, "ThreadAppShutdownNotifier exit\n");
        }
    });
    int ctr = 0;
    constexpr int maxctr = 5;
    do {
        int sig = 0, res;
        res = read(asyncSignalPipes[0], &sig, sizeof(sig));
        if (res == sizeof(sig)) {
            ++ctr;
            std::fprintf(stdout, "\n--- Caught signal %d (%d/%d), exiting ...\n", sig, ctr, maxctr);
            seeder::RequestShutdown();
            if (ctr >= maxctr) {
                std::fprintf(stdout, "--- Too many signals caught, aborting program.\n");
                std::abort();
            }
        } else {
            if (res < 0) {
                perror("read");
            } else if (res == 1 && reinterpret_cast<char *>(&sig)[0] == 'x') {
                // app signaled us to exit
                return;
            } else {
                std::fprintf(stderr, "\nWARNING: ThreadAppShutdownNotifier got unexepected return from read():"
                                     " %d (read bytes: %x)\n", res, sig);
            }
        }
    } while (1);
}

} // namespace

int main(int argc, char **argv) {
    // The logger dump everything on the console by default.
    LogInstance().m_print_to_console = true;

    std::setbuf(stdout, nullptr);
    CDnsSeedOpts opts;
    int parseResults = opts.ParseCommandLine(argc, argv);
    if (parseResults != CONTINUE_EXECUTION) {
        return parseResults;
    }

    std::fprintf(stdout, "Supporting whitelisted filters: ");
    for (std::set<uint64_t>::const_iterator it = opts.filter_whitelist.begin();
         it != opts.filter_whitelist.end(); it++) {
        if (it != opts.filter_whitelist.begin()) {
            std::fprintf(stdout, ",");
        }
        std::fprintf(stdout, "0x%lx", (unsigned long)*it);
    }
    std::fprintf(stdout, "\n");
    if (!opts.tor.empty()) {
        CService service(LookupNumeric(opts.tor.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using Tor proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_ONION, proxyType(service));
        }
    }
    if (!opts.ipv4_proxy.empty()) {
        CService service(LookupNumeric(opts.ipv4_proxy.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using IPv4 proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_IPV4, proxyType(service));
        }
    }
    if (!opts.ipv6_proxy.empty()) {
        CService service(LookupNumeric(opts.ipv6_proxy.c_str(), 9050));
        if (service.IsValid()) {
            std::fprintf(stdout, "Using IPv6 proxy at %s\n",
                         service.ToStringIPPort().c_str());
            SetProxy(NET_IPV6, proxyType(service));
        }
    }
    bool fDNS = true;
    std::fprintf(stdout, "Using %s.\n", gArgs.GetChainName().c_str());
    if (opts.ns.empty()) {
        std::fprintf(stdout, "No nameserver set. Not starting DNS server.\n");
        fDNS = false;
    }
    if (fDNS && opts.host.empty()) {
        std::fprintf(stderr, "No hostname set. Please use -host.\n");
        return EXIT_FAILURE;
    }
    if (fDNS && opts.mbox.empty()) {
        std::fprintf(stderr, "No e-mail address set. Please use -mbox.\n");
        return EXIT_FAILURE;
    }
    FILE *f = fsbridge::fopen("dnsseed.dat", "r");
    if (f) {
        std::fprintf(stdout, "Loading dnsseed.dat...");
        try {
            CAutoFile cf(f, SER_DISK, CLIENT_VERSION);
            cf >> db;
            if (opts.fWipeBan) {
                db.banned.clear();
                std::fprintf(stdout, "Ban list wiped...");
            }
            std::fprintf(stdout, "done\n");
        } catch (const std::exception &e) {
            std::fprintf(stderr, "WARNING: Unable to load dnsseed.dat, caught exception (%s): %s\n",
                         typeid(e).name(), e.what());
            std::fprintf(stdout, "dnsseed.dat is either from a different version of this program or is corrupted.\n"
                                 "Please delele all data files to wipe the seeder database and restart.\n");
            return EXIT_FAILURE;
        }
    }

    // Set up shutdown notifier thread
    if (pipe(asyncSignalPipes) != 0) { // for asynch-signal-safe notification
        perror("pipe");
        return EXIT_FAILURE;
    }
    std::thread threadShutdownNotifier(ThreadAppShutdownNotifier);
    Defer cleanupShutdownNotifier([&]{
        // Tell the threadShutdownNotifier to exit; also clean up the pipes.
        auto ign [[maybe_unused]] = write(asyncSignalPipes[1], "x", 1);
        threadShutdownNotifier.join();
        for (int &fd : asyncSignalPipes) {
            if (fd > -1) {
                close(fd);
                fd = -1;
            }
        }
    });

    // Set up signal handler
    std::vector<std::pair<int, void (*)(int)>> signalsToRestore{{
        {SIGINT , std::signal(SIGINT , signalHandler)},
        {SIGTERM, std::signal(SIGTERM, signalHandler)},
        {SIGQUIT, std::signal(SIGQUIT, signalHandler)},
        {SIGHUP , std::signal(SIGHUP , signalHandler)},
        {SIGPIPE, std::signal(SIGPIPE, SIG_IGN)},
    }};
    Defer restoreSigs([&]{
        for (const auto & [sig, origHandler] : signalsToRestore) {
            std::signal(sig, origHandler);
        }
        signalsToRestore.clear();
    });

    // Start main app threads
    CAddrDbStats dbStats;
    db.GetStats(dbStats);
    if (opts.fReseed || dbStats.nAvail < 1) {
        // The database is empty or reseed was requested, fill it with fixed seeds
        for (const SeedSpec6 &seed : Params().FixedSeeds()) {
            db.Add(CAddress(seed, ServiceFlags()), true);
        }
    }
    std::thread threadSeed, threadDump, threadStats;
    if (fDNS) {
        std::fprintf(stdout, "Starting %i DNS threads for %s on %s (port %i)...",
                     opts.nDnsThreads, opts.host.c_str(), opts.ns.c_str(),
                     opts.nPort);
        dnsThreads.reserve(std::max(0, opts.nDnsThreads));
        for (int i = 0; i < opts.nDnsThreads; i++) {
            auto &dnsThread = dnsThreads.emplace_back(std::make_unique<CDnsThread>(&opts, i));
            dnsThread->threadHandle = std::thread(ThreadDNS, dnsThread.get());
            std::fprintf(stdout, ".");
        }
        std::fprintf(stdout, "done\n");
    }
    std::fprintf(stdout, "Starting seeder...");
    threadSeed = std::thread(ThreadSeeder);
    std::fprintf(stdout, "done\n");
    std::fprintf(stdout, "Starting %i crawler threads...", opts.nThreads);
    pthread_attr_t attr_crawler;
    pthread_attr_init(&attr_crawler);
    pthread_attr_setstacksize(&attr_crawler, 0x20000);
    std::vector<pthread_t> crawlerThreads;
    crawlerThreads.resize(std::max(0, opts.nThreads), pthread_t{});
    assert(size_t(opts.nThreads) <= std::numeric_limits<uint16_t>::max());
    for (size_t i = 0; i < crawlerThreads.size(); ++i) {
        auto &thread = crawlerThreads[i];
        const CrawlerArg crawlerArg = { /*.threadNum = */uint16_t(i), /* .nThreads = */ uint16_t(opts.nThreads) };
        void *arg{};
        std::memcpy(&arg, &crawlerArg, sizeof(crawlerArg)); // stuff raw bytes of CrawlerArg into a void * "pointer"
        pthread_create(&thread, &attr_crawler, ThreadCrawler, arg);
    }
    pthread_attr_destroy(&attr_crawler);
    std::fprintf(stdout, "done\n");
    threadStats = std::thread(ThreadStats);
    threadDump = std::thread(ThreadDumper);
    threadDump.join();
    threadStats.join();
    for (const auto &thread : crawlerThreads) {
        void *res;
        pthread_join(thread, &res);
    }
    threadSeed.join();
    DnsServer::Shutdown();
    bool hadError = false;
    for (auto &dnsThread : dnsThreads) {
        dnsThread->threadHandle.join();
        hadError |= dnsThread->hadError;
    }
    SaveAllToDisk(); // Save to disk one last time after all threads are stopped
    return hadError ? EXIT_FAILURE : EXIT_SUCCESS;
}
