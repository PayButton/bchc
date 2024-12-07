// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <util/time.h>

static void BenchTimeDeprecated(benchmark::State &state) {
    BENCHMARK_LOOP {
        (void)GetTime();
    }
}

static void BenchTimeMock(benchmark::State &state) {
    SetMockTime(111);
    BENCHMARK_LOOP {
        (void)GetTime<std::chrono::seconds>();
    }
    SetMockTime(0);
}

static void BenchTimeMillis(benchmark::State &state) {
    BENCHMARK_LOOP {
        (void)GetTime<std::chrono::milliseconds>();
    }
}

static void BenchTimeMillisSys(benchmark::State &state) {
    BENCHMARK_LOOP {
        (void)GetTimeMillis();
    }
}

BENCHMARK(BenchTimeDeprecated, 100000000);
BENCHMARK(BenchTimeMillis, 6000000);
BENCHMARK(BenchTimeMillisSys, 6000000);
BENCHMARK(BenchTimeMock, 300000000);
