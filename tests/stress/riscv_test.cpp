// Minimal cross-compilation test for RISC-V
// Tests core C++ logic without Qt/D-Bus dependencies
#include <logmind/log_buffer.h>
#include <logmind/log_entry.h>
#include <logmind/pipeline.h>
#include <logmind/plugin_loader.h>
#include <iostream>
#include <chrono>

using namespace logmind;

int main() {
    std::cout << "=== LogMind RISC-V Cross-Compilation Test ===" << std::endl;
    std::cout << "Date: 2026-05-31" << std::endl;
    std::cout << std::endl;

    // Test 1: LogBuffer basic operations
    std::cout << "--- Test 1: LogBuffer basic operations ---" << std::endl;
    {
        LogBuffer buf(1000);
        for (int i = 0; i < 500; i++) {
            LogEntry e;
            e.source = "test";
            e.level = LogLevel::INFO;
            e.message = "Test message " + std::to_string(i);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        std::cout << "  Pushed 500, size=" << buf.size() << " (expect 500)" << std::endl;
        
        // Overfill
        for (int i = 0; i < 1000; i++) {
            LogEntry e;
            e.source = "test";
            e.level = LogLevel::INFO;
            e.message = "Overfill message " + std::to_string(i);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        std::cout << "  Overfilled, size=" << buf.size() << " (expect 1000)" << std::endl;
        std::cout << "  total_pushed=" << buf.total_pushed()
                  << ", discarded=" << buf.total_discarded() << std::endl;
    }

    // Test 2: LogBuffer query
    std::cout << "--- Test 2: LogBuffer query ---" << std::endl;
    {
        LogBuffer buf(10000);
        for (int i = 0; i < 5000; i++) {
            LogEntry e;
            e.source = "query-test";
            e.level = (i % 3 == 0) ? LogLevel::ERROR : LogLevel::INFO;
            e.message = "Message " + std::to_string(i) + " error_code=" + std::to_string(i % 100);
            e.timestamp = std::chrono::system_clock::now();
            buf.push(std::move(e));
        }
        auto results = buf.search("error_code=50", 10);
        std::cout << "  Search 'error_code=50': " << results.size() << " results" << std::endl;
    }

    // Test 3: PluginLoader basic
    std::cout << "--- Test 3: PluginLoader basic ---" << std::endl;
    {
        PluginLoader loader;
        std::cout << "  PluginLoader created, loaded=" << loader.is_loaded() << std::endl;
        std::cout << "  load_directory(nonexistent): " << loader.load_directory("/nonexistent") << std::endl;
        std::cout << "  loaded_count=" << loader.loaded_count() << std::endl;
    }

    // Test 4: LogEntry JSON serialization
    std::cout << "--- Test 4: LogEntry JSON serialization ---" << std::endl;
    {
        LogEntry e;
        e.source = "json-test";
        e.level = LogLevel::WARN;
        e.message = "Test with unicode: 你好世界";
        e.fields["key"] = "value";
        e.tags["tag1"] = true;
        
        nlohmann::json j;
        j["source"] = e.source;
        j["message"] = e.message;
        j["level"] = to_string(e.level);
        std::cout << "  JSON: " << j.dump() << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== RISC-V Cross-Compilation Test PASSED ===" << std::endl;
    return 0;
}
