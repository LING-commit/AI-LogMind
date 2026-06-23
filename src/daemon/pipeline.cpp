#include <logmind/pipeline.h>
#include <logmind/log_buffer.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace logmind {

Pipeline::Pipeline(PluginLoader& loader)
    : m_loader(loader)
{
}

void Pipeline::process_raw(const std::string& source_name,
                            const std::string& raw_line)
{
    auto entries = run_collectors(source_name, raw_line);
    if (entries.empty()) return;

    entries = run_analyzers(std::move(entries));
    if (entries.empty()) return;

    run_alerts(entries);

    if (m_output_cb) {
        m_output_cb(entries);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_processed += entries.size();
}

void Pipeline::process_entries(std::vector<LogEntry> entries) {
    if (entries.empty()) return;

    entries = run_analyzers(std::move(entries));
    if (entries.empty()) return;

    run_alerts(entries);

    if (m_output_cb) {
        m_output_cb(entries);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_processed += entries.size();
}

std::vector<LogEntry> Pipeline::run_collectors(
    const std::string& source, const std::string& raw_line)
{
    auto collectors = m_loader.collectors();
    std::vector<LogEntry> result;

    // 找到匹配的采集器
    int best_match = 0;
    ICollector* best_collector = nullptr;

    for (auto* c : collectors) {
        int score = c->match(source);
        if (score > best_match) {
            best_match = score;
            best_collector = c;
        }
    }

    if (!best_collector) {
        LogEntry fallback;
        fallback.source    = source;
        fallback.raw       = raw_line;
        fallback.message   = raw_line;
        fallback.timestamp = std::chrono::system_clock::now();
        fallback.level     = LogLevel::INFO;
        fallback.module    = "fallback";
        fallback.meta["source_file"] = source;

        // 从原始行中猜测日志级别
        std::string upper;
        for (auto c : raw_line) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        if (upper.find("FATAL") != std::string::npos)      fallback.level = LogLevel::FATAL;
        else if (upper.find("ERROR") != std::string::npos) fallback.level = LogLevel::ERROR;
        else if (upper.find("WARN")  != std::string::npos) fallback.level = LogLevel::WARN;
        else if (upper.find("DEBUG") != std::string::npos) fallback.level = LogLevel::DEBUG;

        result.push_back(std::move(fallback));
        return result;
    }

    std::istringstream stream(raw_line);
    auto parse_result = best_collector->parse(stream);

    for (auto& entry : parse_result.entries) {
        entry.source = best_collector->name();
        entry.raw    = raw_line;
        entry.meta["source_file"] = source;
    }

    return std::move(parse_result.entries);
}

std::vector<LogEntry> Pipeline::run_analyzers(std::vector<LogEntry> entries) {
    auto analyzers = m_loader.analyzers();
    for (auto* analyzer : analyzers) {
        try {
            entries = analyzer->analyze(std::move(entries));
            if (entries.empty()) break;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.total_errors++;
        }
    }
    return entries;
}

void Pipeline::run_alerts(const std::vector<LogEntry>& entries) {
    if (entries.empty()) return;
    auto alerts = m_loader.alerts();
    for (auto* alert : alerts) {
        try {
            AlertContext ctx;
            ctx.entries    = entries;
            ctx.severity   = "info";
            ctx.summary    = "Alert triggered by pipeline";
            ctx.triggered_at = std::chrono::system_clock::now();
            ctx.source_plugin = "pipeline";
            alert->send(ctx);

            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.total_alerts++;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.total_errors++;
        }
    }
}

Pipeline::Stats Pipeline::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

} // namespace logmind
