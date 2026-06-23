// LogMind smoke test — verifies core headers compile and basic operations work
#include <logmind/log_buffer.h>
#include <logmind/log_entry.h>
#include <logmind/pipeline.h>
#include <logmind/plugin_loader.h>
#include <iostream>
#include <cstdlib>

using namespace logmind;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; failures++; } \
    else { std::cout << "PASS: " << msg << std::endl; } \
} while(0)

int main() {
    std::cout << "=== LogMind Smoke Test ===" << std::endl;

    // 1. LogBuffer create + push
    {
        LogBuffer buf(100);
        CHECK(buf.capacity() == 100, "LogBuffer capacity=100");
        CHECK(buf.size() == 0, "LogBuffer initial size=0");
        CHECK(buf.total_pushed() == 0, "LogBuffer initial pushed=0");

        for (int i = 0; i < 50; i++) {
            LogEntry e;
            e.source = "smoke";
            e.level = LogLevel::INFO;
            e.message = "test " + std::to_string(i);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        CHECK(buf.size() == 50, "LogBuffer size=50 after 50 pushes");
    }

    // 2. LogBuffer capacity enforcement
    {
        LogBuffer buf(10);
        for (int i = 0; i < 100; i++) {
            LogEntry e;
            e.source = "cap";
            e.message = "cap msg " + std::to_string(i);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        CHECK(buf.size() == 10, "LogBuffer capped at 10");
        CHECK(buf.total_pushed() == 100, "LogBuffer pushed=100");
        CHECK(buf.total_discarded() == 90, "LogBuffer discarded=90");
    }

    // 3. LogBuffer query
    {
        LogBuffer buf(1000);
        for (int i = 0; i < 100; i++) {
            LogEntry e;
            e.source = "q";
            e.level = i % 2 == 0 ? LogLevel::ERROR : LogLevel::INFO;
            e.message = "query msg idx=" + std::to_string(i);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        auto results = buf.search("idx=50", 10);
        CHECK(results.size() >= 1, "Search finds matching entry");
    }

    // 4. Pipeline creation
    {
        PluginLoader loader;
        Pipeline pipeline(loader);
        auto stats = pipeline.stats();
        CHECK(stats.total_processed == 0, "Pipeline initial processed=0");
        CHECK(stats.total_errors == 0, "Pipeline initial errors=0");
    }

    // 5. PluginLoader empty dir
    {
        PluginLoader loader;
        CHECK(loader.loaded_count() == 0, "PluginLoader initial count=0");
        bool loaded = loader.load_directory("/nonexistent_path_12345");
        CHECK(!loaded, "PluginLoader returns false for nonexistent dir");
        CHECK(loader.loaded_count() == 0, "PluginLoader count=0 after failed load");
    }

    std::cout << std::endl;
    if (failures == 0) {
        std::cout << "=== All smoke tests PASSED ===" << std::endl;
        return 0;
    } else {
        std::cout << "=== " << failures << " tests FAILED ===" << std::endl;
        return 1;
    }
}
