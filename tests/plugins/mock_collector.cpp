// Mock Collector Plugin for load/unload stability testing
#include <logmind/plugin.h>
#include <logmind/collector.h>
#include <logmind/version.h>
#include <sstream>
#include <iostream>
#include <cstring>

using namespace logmind;

class MockCollector : public ICollector {
public:
    MockCollector() = default;
    ~MockCollector() override = default;

    PluginManifest manifest() const override {
        return {"mock_collector", "1.0.0", "Mock collector for testing", "test", PluginType::Collector, "MIT"};
    }

    bool on_load(const nlohmann::json& config) override {
        (void)config;
        std::cout << "[mock_collector] on_load called" << std::endl;
        return true;
    }
    void on_unload() override {
        std::cout << "[mock_collector] on_unload called" << std::endl;
    }
    bool on_reload_config(const nlohmann::json& config) override { (void)config; return true; }

    int match(const std::string& source) const override {
        if (source.find("mock") != std::string::npos) return 100;
        return 0;
    }

    ParseResult parse(std::istream& input) override {
        ParseResult result;
        std::string line;
        while (std::getline(input, line)) {
            LogEntry entry;
            entry.level = LogLevel::INFO;
            entry.message = line;
            entry.source = "mock_collector";
            entry.timestamp = std::chrono::system_clock::now();
            result.entries.push_back(std::move(entry));
        }
        return result;
    }

    std::vector<std::string> watch_patterns() const override {
        return {"*.log", "*.txt"};
    }
};

extern "C" {
    int plugin_abi_version() { return LOGMIND_ABI_VERSION; }

    IPlugin* create_plugin(CreateContext ctx) {
        (void)ctx;
        return new MockCollector();
    }

    void destroy_plugin(IPlugin* p) {
        delete static_cast<MockCollector*>(p);
    }
}
