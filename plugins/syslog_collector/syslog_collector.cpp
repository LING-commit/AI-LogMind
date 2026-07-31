// 示例采集器：解析常见的几种文本日志格式。
//
// 这是 Collector 插件的参考实现，也是 clone 下来第一次运行时能看到效果的最小依赖。
// 有意不用 std::regex：逐行热路径上正则的开销远大于手写解析。
#include <logmind/collector.h>
#include <logmind/version.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool starts_with_digits(const std::string& s, size_t pos, size_t count) {
    if (pos + count > s.size()) return false;
    for (size_t i = 0; i < count; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[pos + i]))) return false;
    }
    return true;
}

// 在 [begin, end) 里找一个被方括号包住的级别名，比如 "[ERROR]"
bool parse_bracketed_level(const std::string& line, size_t& pos, logmind::LogLevel& level) {
    if (pos >= line.size() || line[pos] != '[') return false;

    const size_t close = line.find(']', pos);
    if (close == std::string::npos || close - pos > 8) return false;

    std::string token = line.substr(pos + 1, close - pos - 1);
    std::transform(token.begin(), token.end(), token.begin(), [](char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });

    static const char* const kNames[] = {"DEBUG", "INFO", "WARN", "WARNING", "ERROR", "FATAL"};
    for (const char* name : kNames) {
        if (token == name) {
            level = logmind::log_level_from_string(token == "WARNING" ? "WARN" : token);
            pos   = close + 1;
            return true;
        }
    }
    return false;
}

void skip_spaces(const std::string& line, size_t& pos) {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
}

logmind::LogLevel guess_level(const std::string& line) {
    struct Probe { const char* needle; logmind::LogLevel level; };
    static const Probe kProbes[] = {
        {"FATAL", logmind::LogLevel::FATAL},
        {"PANIC", logmind::LogLevel::FATAL},
        {"ERROR", logmind::LogLevel::ERROR},
        {"WARN",  logmind::LogLevel::WARN},
        {"DEBUG", logmind::LogLevel::DEBUG},
        {"TRACE", logmind::LogLevel::DEBUG},
    };
    for (const auto& probe : kProbes) {
        const size_t len = std::strlen(probe.needle);
        const auto it = std::search(line.begin(), line.end(),
                                    probe.needle, probe.needle + len,
                                    [](char a, char b) {
                                        return std::toupper(static_cast<unsigned char>(a)) ==
                                               static_cast<unsigned char>(b);
                                    });
        if (it != line.end()) return probe.level;
    }
    return logmind::LogLevel::INFO;
}

} // namespace

class SyslogCollector : public logmind::ICollector {
public:
    logmind::PluginManifest manifest() const override {
        logmind::PluginManifest m;
        m.name        = "syslog-collector";
        m.version     = "1.0.0";
        m.description = "Parses ISO-8601, bracketed-level and RFC3164 syslog lines";
        m.author      = "LogMind";
        m.type        = logmind::PluginType::Collector;
        m.license     = "MIT";
        return m;
    }

    bool on_load(const nlohmann::json& config) override {
        // 演示 on_load 拿到自己那段配置（来自 config.json 的 plugins.<name>）
        if (config.contains("extra_patterns") && config["extra_patterns"].is_array()) {
            for (const auto& p : config["extra_patterns"]) {
                if (p.is_string()) m_patterns.push_back(p.get<std::string>());
            }
        }
        return true;
    }

    int match(const std::string& filename) const override {
        if (filename.find("syslog")   != std::string::npos ||
            filename.find("messages") != std::string::npos) return 100;
        if (filename.size() >= 4 &&
            filename.compare(filename.size() - 4, 4, ".log") == 0) return 60;
        return 0;
    }

    std::vector<std::string> watch_patterns() const override { return m_patterns; }

    ParseResult parse(std::istream& stream) override {
        ParseResult result;
        std::string line;
        while (std::getline(stream, line)) {
            ++result.lines_read;
            if (line.empty()) {
                ++result.lines_skipped;
                continue;
            }
            result.entries.push_back(parse_one(line));
            ++result.lines_parsed;
        }
        return result;
    }

    // 覆写掉默认实现，省掉每行一个 istringstream
    ParseResult parse_line(const std::string& line) override {
        ParseResult result;
        result.lines_read = 1;
        if (line.empty()) {
            ++result.lines_skipped;
            return result;
        }
        result.entries.push_back(parse_one(line));
        result.lines_parsed = 1;
        return result;
    }

private:
    logmind::LogEntry parse_one(const std::string& line) const {
        logmind::LogEntry entry;
        entry.raw       = line;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level     = logmind::LogLevel::INFO;

        size_t pos = 0;

        // 形如 2024-01-15T10:23:45 或 2024-01-15 10:23:45 的时间戳
        if (starts_with_digits(line, 0, 4) && line.size() > 10 && line[4] == '-') {
            const size_t sep = line.find_first_of(" \t", 10);
            if (sep != std::string::npos) {
                entry.fields["timestamp_text"] = line.substr(0, sep);
                pos = sep;
                skip_spaces(line, pos);
            }
        }

        // 级别可能是 "[ERROR]" 也可能是裸的 "ERROR"
        if (!parse_bracketed_level(line, pos, entry.level)) {
            const size_t end = line.find_first_of(" \t", pos);
            if (end != std::string::npos && end - pos <= 8) {
                std::string token = line.substr(pos, end - pos);
                std::transform(token.begin(), token.end(), token.begin(), [](char c) {
                    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                });
                if (token == "DEBUG" || token == "INFO" || token == "WARN" ||
                    token == "ERROR" || token == "FATAL") {
                    entry.level = logmind::log_level_from_string(token);
                    pos = end;
                }
            }
        }
        skip_spaces(line, pos);

        // 紧跟着的 [module] 当作模块名
        if (pos < line.size() && line[pos] == '[') {
            const size_t close = line.find(']', pos);
            if (close != std::string::npos && close - pos <= 64) {
                entry.module = line.substr(pos + 1, close - pos - 1);
                pos = close + 1;
                skip_spaces(line, pos);
            }
        }

        // RFC3164：Jan 15 10:23:45 host proc[123]: message
        if (entry.module.empty()) {
            const size_t colon = line.find(':', pos);
            const size_t bracket = line.find('[', pos);
            if (bracket != std::string::npos && colon != std::string::npos &&
                bracket < colon && colon - bracket <= 16) {
                const size_t space = line.rfind(' ', bracket);
                const size_t start = (space == std::string::npos) ? pos : space + 1;
                entry.module = line.substr(start, bracket - start);
                pos = colon + 1;
                skip_spaces(line, pos);
            }
        }

        entry.message = (pos < line.size()) ? line.substr(pos) : line;
        if (entry.module.empty()) entry.module = "unknown";

        // 前面一个格式都没匹配上时（级别还是默认的 INFO），退回全行猜测
        if (entry.level == logmind::LogLevel::INFO) {
            entry.level = guess_level(line);
        }
        return entry;
    }

    std::vector<std::string> m_patterns;
};

extern "C" {

LOGMIND_PLUGIN_EXPORT int plugin_abi_version() {
    return logmind::LOGMIND_ABI_VERSION;
}

LOGMIND_PLUGIN_EXPORT logmind::IPlugin* create_plugin(logmind::CreateContext ctx) {
    (void)ctx;
    return new SyslogCollector();
}

LOGMIND_PLUGIN_EXPORT void destroy_plugin(logmind::IPlugin* plugin) {
    delete plugin;
}

} // extern "C"
