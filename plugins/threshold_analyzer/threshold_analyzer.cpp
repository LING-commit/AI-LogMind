// 示例分析器：按模块统计错误突增。
//
// 演示 Analyzer 插件能做的三件事：给条目打标注（fields）、按规则丢弃条目、
// 以及从 on_load 读取自己的配置。
#include <logmind/analyzer.h>
#include <logmind/version.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

class ThresholdAnalyzer : public logmind::IAnalyzer {
public:
    logmind::PluginManifest manifest() const override {
        logmind::PluginManifest m;
        m.name        = "threshold-analyzer";
        m.version     = "1.0.0";
        m.description = "Flags per-module error bursts within a sliding window";
        m.author      = "LogMind";
        m.type        = logmind::PluginType::Analyzer;
        m.license     = "MIT";
        return m;
    }

    bool on_load(const nlohmann::json& config) override {
        m_window          = std::chrono::seconds(config.value("window_seconds", 60));
        m_threshold       = config.value("error_threshold", size_t{5});
        m_drop_debug      = config.value("drop_debug", false);
        return true;
    }

    bool on_reload_config(const nlohmann::json& new_config) override {
        return on_load(new_config);
    }

    std::vector<logmind::LogEntry> analyze(std::vector<logmind::LogEntry> entries) override {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<logmind::LogEntry> out;
        out.reserve(entries.size());

        for (auto& entry : entries) {
            if (m_drop_debug && entry.level == logmind::LogLevel::DEBUG) {
                continue;   // 返回的条目少于传入的：流水线支持分析器过滤
            }

            if (static_cast<uint8_t>(entry.level) >=
                static_cast<uint8_t>(logmind::LogLevel::ERROR)) {
                const size_t count = record_and_count(entry.module, entry.timestamp);
                if (count >= m_threshold) {
                    entry.fields["burst"]        = true;
                    entry.fields["burst_count"]  = count;
                    entry.fields["burst_window"] = m_window.count();
                }
            }
            out.push_back(std::move(entry));
        }
        return out;
    }

private:
    using TimePoint = std::chrono::system_clock::time_point;

    size_t record_and_count(const std::string& module, TimePoint when) {
        auto& history = m_history[module];
        history.push_back(when);

        // 滑出窗口的记录直接丢掉，保证内存有界
        while (!history.empty() && (when - history.front()) > m_window) {
            history.pop_front();
        }
        return history.size();
    }

    std::mutex m_mutex;
    std::unordered_map<std::string, std::deque<TimePoint>> m_history;

    std::chrono::seconds m_window{60};
    size_t               m_threshold  = 5;
    bool                 m_drop_debug = false;
};

extern "C" {

LOGMIND_PLUGIN_EXPORT int plugin_abi_version() {
    return logmind::LOGMIND_ABI_VERSION;
}

LOGMIND_PLUGIN_EXPORT logmind::IPlugin* create_plugin(logmind::CreateContext ctx) {
    (void)ctx;
    return new ThresholdAnalyzer();
}

LOGMIND_PLUGIN_EXPORT void destroy_plugin(logmind::IPlugin* plugin) {
    delete plugin;
}

} // extern "C"
