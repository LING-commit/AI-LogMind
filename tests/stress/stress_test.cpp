// LogMind Performance Stress Test - C++ version
// Directly uses LogBuffer and Pipeline for high-throughput testing
// No D-Bus overhead, measures core processing performance

#include <logmind/log_buffer.h>
#include <logmind/log_entry.h>
#include <logmind/pipeline.h>
#include <logmind/plugin_loader.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>

using namespace logmind;

static LogEntry make_entry(int id, const std::string& source) {
    LogEntry e;
    e.source = source;
    e.level = LogLevel::INFO;
    e.message = "Test log entry #" + std::to_string(id) +
                " user=alice action=login ip=192.168.1." + std::to_string(id % 256);
    e.raw = e.message;
    e.timestamp = std::chrono::system_clock::now();
    e.fields["id"] = id;
    e.tags["test"] = true;
    return e;
}

// ── Test 1: LogBuffer push throughput ──
static void test_buffer_push_throughput() {
    std::cout << "--- Test 1: LogBuffer push throughput ---" << std::endl;

    const size_t N = 100000;
    LogBuffer buf(N);

    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < N; i++) {
        buf.push(make_entry(i, "perf-test"));
    }
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double tps = static_cast<double>(N) * 1000.0 / static_cast<double>(ms);

    std::cout << "  Pushed " << N << " entries in " << ms << "ms" << std::endl;
    std::cout << "  Throughput: " << static_cast<int>(tps) << " logs/sec" << std::endl;
    std::cout << "  Buffer size: " << buf.size()
              << ", total_pushed: " << buf.total_pushed()
              << ", total_discarded: " << buf.total_discarded() << std::endl;
    std::cout << std::endl;
}

// ── Test 2: LogBuffer capacity enforcement ──
static void test_buffer_capacity() {
    std::cout << "--- Test 2: LogBuffer capacity enforcement ---" << std::endl;

    const size_t CAP = 1000;
    LogBuffer buf(CAP);

    for (int i = 0; i < 5000; i++) {
        buf.push(make_entry(i, "cap-test"));
    }

    std::cout << "  Pushed 5000 entries, capacity=" << CAP << std::endl;
    std::cout << "  Buffer size: " << buf.size() << " (should be " << CAP << ")" << std::endl;
    std::cout << "  Total discarded: " << buf.total_discarded()
              << " (should be 4000)" << std::endl;

    bool ok = (buf.size() == CAP) && (buf.total_discarded() == 4000);
    std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;
}

// ── Test 3: LogBuffer query throughput ──
static void test_buffer_query_throughput() {
    std::cout << "--- Test 3: LogBuffer query throughput ---" << std::endl;

    LogBuffer buf(100000);
    for (int i = 0; i < 100000; i++) {
        buf.push(make_entry(i, "query-test"));
    }

    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(1);
    auto end   = now + std::chrono::hours(1);

    const int Q = 10000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < Q; i++) {
        auto results = buf.query_by_time(start, end, 100);
        (void)results;
    }
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double qps = static_cast<double>(Q) * 1000.0 / static_cast<double>(ms);

    std::cout << "  " << Q << " queries over 100k buffer in " << ms << "ms" << std::endl;
    std::cout << "  Query throughput: " << static_cast<int>(qps) << " queries/sec" << std::endl;
    std::cout << std::endl;
}

// ── Test 4: LogBuffer search throughput ──
static void test_buffer_search_throughput() {
    std::cout << "--- Test 4: LogBuffer search throughput ---" << std::endl;

    LogBuffer buf(100000);
    for (int i = 0; i < 100000; i++) {
        buf.push(make_entry(i, "search-test"));
    }

    const int Q = 5000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < Q; i++) {
        auto results = buf.search("192.168.1.128", 10);
        (void)results;
    }
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double qps = static_cast<double>(Q) * 1000.0 / static_cast<double>(ms);

    std::cout << "  " << Q << " searches over 100k buffer in " << ms << "ms" << std::endl;
    std::cout << "  Search throughput: " << static_cast<int>(qps) << " searches/sec" << std::endl;
    std::cout << std::endl;
}

// ── Test 5: Concurrent push throughput ──
static void test_concurrent_push() {
    std::cout << "--- Test 5: Concurrent push (4 threads × 25000) ---" << std::endl;

    const size_t TOTAL = 100000;
    const int THREADS = 4;
    const size_t PER_THREAD = TOTAL / THREADS;
    LogBuffer buf(TOTAL);

    std::atomic<int64_t> push_ok{0};
    std::atomic<int64_t> push_fail{0};

    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < PER_THREAD; i++) {
                buf.push(make_entry(t * PER_THREAD + i, "thread-" + std::to_string(t)));
                push_ok++;
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double tps = static_cast<double>(push_ok.load()) * 1000.0 / static_cast<double>(ms);

    std::cout << "  Pushed " << push_ok.load() << " entries in " << ms << "ms" << std::endl;
    std::cout << "  Concurrent throughput: " << static_cast<int>(tps) << " logs/sec" << std::endl;
    std::cout << "  Buffer size: " << buf.size() << std::endl;
    std::cout << std::endl;
}

// ── Test 6: Callback overhead ──
static void test_callback_overhead() {
    std::cout << "--- Test 6: Callback overhead measurement ---" << std::endl;

    const size_t N = 50000;

    // Without callback
    {
        LogBuffer buf(N);
        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < N; i++) buf.push(make_entry(i, "no-cb"));
        auto t1 = std::chrono::steady_clock::now();
        auto ms_no_cb = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        // With callback
        LogBuffer buf2(N);
        int64_t cb_count = 0;
        buf2.set_on_entry_callback([&](const LogEntry&) { cb_count++; });
        auto t2 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < N; i++) buf2.push(make_entry(i, "with-cb"));
        auto t3 = std::chrono::steady_clock::now();
        auto ms_with_cb = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        std::cout << "  Without callback: " << ms_no_cb / 1000.0 << "ms" << std::endl;
        std::cout << "  With callback:    " << ms_with_cb / 1000.0 << "ms" << std::endl;
        std::cout << "  Callbacks fired:  " << cb_count << std::endl;
        if (ms_no_cb > 0) {
            double overhead = static_cast<double>(ms_with_cb - ms_no_cb) * 100.0 / static_cast<double>(ms_no_cb);
            std::cout << "  Overhead: " << static_cast<int>(overhead) << "%" << std::endl;
        }
    }
    std::cout << std::endl;
}

// ── Test 7: Memory footprint under load ──
static void test_memory_footprint() {
    std::cout << "--- Test 7: Memory footprint under load ---" << std::endl;

    // Small buffer
    {
        LogBuffer buf(1000);
        for (int i = 0; i < 1000; i++) buf.push(make_entry(i, "mem-small"));
        std::cout << "  1KB buffer: size=" << buf.size()
                  << ", pushed=" << buf.total_pushed() << std::endl;
    }

    // Large buffer
    {
        LogBuffer buf(100000);
        for (int i = 0; i < 100000; i++) buf.push(make_entry(i, "mem-large"));
        std::cout << "  100K buffer: size=" << buf.size()
                  << ", pushed=" << buf.total_pushed()
                  << ", discarded=" << buf.total_discarded() << std::endl;
    }

    // Oversized buffer (force eviction)
    {
        LogBuffer buf(500);
        for (int i = 0; i < 50000; i++) buf.push(make_entry(i, "mem-oversize"));
        std::cout << "  500 cap, 50K pushed: size=" << buf.size()
                  << ", pushed=" << buf.total_pushed()
                  << ", discarded=" << buf.total_discarded() << std::endl;
    }
    std::cout << std::endl;
}

// ── Test 8: Pipeline stats accuracy ──
static void test_pipeline_stats() {
    std::cout << "--- Test 8: Pipeline stats accuracy ---" << std::endl;

    PluginLoader loader;
    Pipeline pipeline(loader);
    Pipeline::Stats s;

    s = pipeline.stats();
    std::cout << "  Initial: processed=" << s.total_processed
              << ", errors=" << s.total_errors
              << ", alerts=" << s.total_alerts << std::endl;

    // process_raw with no collectors returns empty, no stats change
    for (int i = 0; i < 100; i++) {
        pipeline.process_raw("test-source", "2026-05-31T10:00:00Z INFO test message");
    }

    s = pipeline.stats();
    std::cout << "  After 100 process_raw (no collectors): processed=" << s.total_processed
              << ", errors=" << s.total_errors << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== LogMind Performance Stress Test (C++) ===" << std::endl;
    std::cout << "Date: 2026-05-31 18:34" << std::endl;
    std::cout << std::endl;

    test_buffer_push_throughput();
    test_buffer_capacity();
    test_buffer_query_throughput();
    test_buffer_search_throughput();
    test_concurrent_push();
    test_callback_overhead();
    test_memory_footprint();
    test_pipeline_stats();

    std::cout << "=== Performance Test Complete ===" << std::endl;
    return 0;
}
