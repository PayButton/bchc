// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <seeder/util.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

namespace {

std::shared_mutex sleep_mutex;
std::condition_variable_any sleep_condition;
std::atomic_bool shutdown_requested = false;

} // namespace

namespace seeder {

bool SleepAndPollShutdownFlag(int64_t nMilliSec) {
    const auto sleepUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds{std::max<int64_t>(0, nMilliSec)};
    std::cv_status status;
    do {
        std::shared_lock lock(sleep_mutex);
        if (seeder::ShutdownRequested()) return false;
        status = sleep_condition.wait_until(lock, sleepUntil);
        if (seeder::ShutdownRequested()) return false;
    } while (status != std::cv_status::timeout);
    return true;
}

bool ShutdownRequested() {
    return shutdown_requested.load();
}

void RequestShutdown() {
    {
        std::unique_lock lock(sleep_mutex);
        shutdown_requested = true;
    }
    sleep_condition.notify_all();
}

} // namespace seeder
