#pragma once
#include <logmind/plugin.h>
#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>
#include <logmind/plugin_loader.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace logmind {

class LogBuffer;

class Pipeline {
public:
    explicit Pipeline(PluginLoader& loader);
    // Worker 是不完整类型，析构必须出定义在 .cpp 里
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // 告警触发规则。
    //
    // 旧实现对每一批日志都无条件调用全部 Alert 插件，severity 写死 "info"、
    // summary 写死一句固定文案——README 里说的「根据规则发送告警」其实并不存在。
    struct AlertRule {
        LogLevel             min_level = LogLevel::ERROR;   // 低于此级别不告警
        std::chrono::seconds dedup_window{60};              // 同一指纹在窗口内只告一次
        size_t               max_alerts_per_minute = 60;    // 全局限流，0 = 不限
        size_t               dedup_cache_size      = 4096;
    };

    void set_alert_rule(const AlertRule& rule);
    AlertRule alert_rule() const;

    void process_raw(const std::string& source_name,
                     const std::string& raw_line);

    void process_entries(std::vector<LogEntry> entries);

    // ── 异步模式 ──
    // 开启后 process_raw() 只把行投递到有界队列，实际处理交给 worker 线程，
    // 采集线程不再被 analyzer/alert/落库阻塞。
    // 按 source 分片，保证同一个来源的行在同一个 worker 上顺序处理。
    // 不调用的话就是同步处理（默认）。
    void start_workers(size_t thread_count, size_t queue_capacity = 8192);
    void stop_workers();
    void flush();                     // 等到队列排空

    using LogCallback = std::function<void(std::vector<LogEntry>)>;
    void set_output_callback(LogCallback cb);

    struct Stats {
        uint64_t total_processed  = 0;
        uint64_t total_errors     = 0;
        uint64_t total_alerts     = 0;
        uint64_t total_suppressed = 0;   // 被去重/限流挡掉的告警数
        uint64_t total_dropped    = 0;   // 队列满时丢弃的行数
        double   qps              = 0.0;
    };

    Stats stats() const;

private:
    struct Worker;

    void process_raw_now(const std::string& source_name, const std::string& raw_line);
    void worker_loop(Worker& worker);

    std::vector<LogEntry> run_collectors(const PluginSetPtr& plugins,
                                         const std::string& source,
                                         const std::string& raw);
    std::vector<LogEntry> run_analyzers(const PluginSet& plugins,
                                         std::vector<LogEntry> entries);
    void                  run_alerts(const PluginSet& plugins,
                                     const std::vector<LogEntry>& entries);

    ICollector* pick_collector(const PluginSetPtr& plugins, const std::string& source);
    std::vector<LogEntry> select_alerting(const std::vector<LogEntry>& entries);
    void note_processed(size_t count);

    static std::string alert_key(const LogEntry& entry);
    static const char* severity_of(LogLevel level);

    PluginLoader& m_loader;

    // ── source → 采集器 的缓存 ──
    // 旧实现每行都要对所有采集器重跑一遍 match() 打分。
    // 插件快照一换（加载/卸载/启停）缓存就整体失效。
    mutable std::mutex                            m_cache_mutex;
    PluginSetPtr                                  m_cache_generation;
    std::unordered_map<std::string, ICollector*>  m_collector_cache;

    // ── 告警状态 ──
    mutable std::mutex m_alert_mutex;
    AlertRule          m_alert_rule;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_recent_alerts;
    std::chrono::system_clock::time_point m_rate_window_start{};
    size_t                                m_rate_count = 0;

    // ── 统计 ──
    mutable std::mutex m_stats_mutex;
    Stats              m_stats;
    std::chrono::steady_clock::time_point m_qps_window_start{};
    uint64_t                              m_qps_window_count = 0;

    mutable std::mutex m_output_mutex;
    LogCallback        m_output_cb;

    // ── 异步 worker ──
    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<bool>                    m_async{false};
    size_t                               m_queue_capacity = 0;
};

} // namespace logmind
