#pragma once
#include <logmind/plugin.h>
#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>
#include <logmind/plugin_loader.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace logmind {

class LogBuffer;

class Pipeline {
public:
    explicit Pipeline(PluginLoader& loader);

    void process_raw(const std::string& source_name,
                     const std::string& raw_line);

    void process_entries(std::vector<LogEntry> entries);

    using LogCallback = std::function<void(std::vector<LogEntry>)>;
    void set_output_callback(LogCallback cb) { m_output_cb = std::move(cb); }

    struct Stats {
        uint64_t total_processed  = 0;
        uint64_t total_errors     = 0;
        uint64_t total_alerts     = 0;
        double   qps              = 0.0;
    };

    Stats stats() const;

private:
    std::vector<LogEntry> run_collectors(const std::string& source,
                                          const std::string& raw);
    std::vector<LogEntry> run_analyzers(std::vector<LogEntry> entries);
    void                  run_alerts(const std::vector<LogEntry>& entries);

    PluginLoader&   m_loader;
    mutable std::mutex m_mutex;
    LogCallback     m_output_cb;

    mutable Stats   m_stats;
};

} // namespace logmind
