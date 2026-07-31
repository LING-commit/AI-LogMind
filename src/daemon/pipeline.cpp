#include <logmind/pipeline.h>
#include <logmind/log_buffer.h>

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <thread>

namespace logmind {

namespace {

constexpr size_t kAlertKeyMaxLen = 192;
constexpr auto   kQpsWindow      = std::chrono::seconds(5);

bool level_at_least(LogLevel level, LogLevel threshold) {
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(threshold);
}

} // namespace

// 异步模式下的一个分片队列 + 消费线程
struct Pipeline::Worker {
    std::mutex              mutex;
    std::condition_variable cv;
    std::deque<std::pair<std::string, std::string>> queue;
    std::thread             thread;
    bool                    stopping = false;
    bool                    busy     = false;
};

Pipeline::Pipeline(PluginLoader& loader)
    : m_loader(loader)
    , m_rate_window_start(std::chrono::system_clock::now())
    , m_qps_window_start(std::chrono::steady_clock::now())
{
}

Pipeline::~Pipeline() {
    stop_workers();
}

void Pipeline::set_output_callback(LogCallback cb) {
    std::lock_guard<std::mutex> lock(m_output_mutex);
    m_output_cb = std::move(cb);
}

void Pipeline::set_alert_rule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(m_alert_mutex);
    m_alert_rule = rule;
    m_recent_alerts.clear();
}

Pipeline::AlertRule Pipeline::alert_rule() const {
    std::lock_guard<std::mutex> lock(m_alert_mutex);
    return m_alert_rule;
}

void Pipeline::process_raw(const std::string& source_name, const std::string& raw_line) {
    if (m_async.load(std::memory_order_acquire)) {
        // 按 source 哈希分片：同一个文件的行始终落到同一个 worker，顺序不乱
        const size_t shard = std::hash<std::string>{}(source_name) % m_workers.size();
        Worker& worker = *m_workers[shard];

        std::lock_guard<std::mutex> lock(worker.mutex);
        if (worker.queue.size() >= m_queue_capacity) {
            // 队列满时丢弃并计数，而不是反压住采集线程
            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            ++m_stats.total_dropped;
            return;
        }
        worker.queue.emplace_back(source_name, raw_line);
        worker.cv.notify_one();
        return;
    }
    process_raw_now(source_name, raw_line);
}

void Pipeline::process_raw_now(const std::string& source_name, const std::string& raw_line) {
    // 每行只取一次插件快照。旧实现分别调用 collectors()/analyzers()/alerts()，
    // 每个都要遍历插件表 + dynamic_cast + 堆分配一个 vector，全在逐行路径上。
    const PluginSetPtr plugins = m_loader.snapshot();
    if (!plugins) return;

    auto entries = run_collectors(plugins, source_name, raw_line);
    if (entries.empty()) return;

    entries = run_analyzers(*plugins, std::move(entries));
    if (entries.empty()) return;

    run_alerts(*plugins, entries);

    const size_t count = entries.size();
    {
        std::lock_guard<std::mutex> lock(m_output_mutex);
        if (m_output_cb) m_output_cb(std::move(entries));
    }
    note_processed(count);
}

void Pipeline::start_workers(size_t thread_count, size_t queue_capacity) {
    if (m_async.load(std::memory_order_acquire)) return;
    if (thread_count == 0) return;

    m_queue_capacity = (queue_capacity == 0) ? 1 : queue_capacity;
    m_workers.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        m_workers.push_back(std::make_unique<Worker>());
    }
    // 先把 worker 建好再置位，否则 process_raw 可能看到空的 m_workers
    m_async.store(true, std::memory_order_release);

    for (auto& worker : m_workers) {
        worker->thread = std::thread(&Pipeline::worker_loop, this, std::ref(*worker));
    }
}

void Pipeline::stop_workers() {
    if (!m_async.exchange(false, std::memory_order_acq_rel)) return;

    for (auto& worker : m_workers) {
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->stopping = true;
        }
        worker->cv.notify_all();
    }
    for (auto& worker : m_workers) {
        if (worker->thread.joinable()) worker->thread.join();
    }
    m_workers.clear();
}

void Pipeline::flush() {
    for (auto& worker : m_workers) {
        std::unique_lock<std::mutex> lock(worker->mutex);
        worker->cv.wait(lock, [&] {
            return (worker->queue.empty() && !worker->busy) || worker->stopping;
        });
    }
}

void Pipeline::worker_loop(Worker& worker) {
    for (;;) {
        std::pair<std::string, std::string> job;
        {
            std::unique_lock<std::mutex> lock(worker.mutex);
            worker.cv.wait(lock, [&] { return !worker.queue.empty() || worker.stopping; });
            if (worker.queue.empty()) {
                if (worker.stopping) return;
                continue;
            }
            job = std::move(worker.queue.front());
            worker.queue.pop_front();
            worker.busy = true;
        }

        process_raw_now(job.first, job.second);

        {
            std::lock_guard<std::mutex> lock(worker.mutex);
            worker.busy = false;
        }
        worker.cv.notify_all();   // 唤醒可能在 flush() 里等待的线程
    }
}

void Pipeline::process_entries(std::vector<LogEntry> entries) {
    if (entries.empty()) return;

    const PluginSetPtr plugins = m_loader.snapshot();
    if (!plugins) return;

    entries = run_analyzers(*plugins, std::move(entries));
    if (entries.empty()) return;

    run_alerts(*plugins, entries);

    const size_t count = entries.size();
    {
        std::lock_guard<std::mutex> lock(m_output_mutex);
        if (m_output_cb) m_output_cb(std::move(entries));
    }
    note_processed(count);
}

ICollector* Pipeline::pick_collector(const PluginSetPtr& plugins, const std::string& source) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);

    // 快照换了就整体丢弃缓存——里面的指针可能已经指向被卸载的插件。
    // 同时把快照存下来：既是「代号」，也让缓存里的裸指针在缓存有效期内一直有效。
    if (m_cache_generation != plugins) {
        m_collector_cache.clear();
        m_cache_generation = plugins;
    }

    auto it = m_collector_cache.find(source);
    if (it != m_collector_cache.end()) return it->second;

    int         best_score     = 0;
    ICollector* best_collector = nullptr;
    for (auto* c : plugins->collectors) {
        const int score = c->match(source);
        if (score > best_score) {
            best_score     = score;
            best_collector = c;
        }
    }

    m_collector_cache.emplace(source, best_collector);   // nullptr 也要缓存，避免反复打分
    return best_collector;
}

std::vector<LogEntry> Pipeline::run_collectors(const PluginSetPtr& plugins,
                                              const std::string& source,
                                              const std::string& raw_line) {
    ICollector* collector = pick_collector(plugins, source);

    if (!collector) {
        LogEntry fallback;
        fallback.source    = source;
        fallback.raw       = raw_line;
        fallback.message   = raw_line;
        fallback.timestamp = std::chrono::system_clock::now();
        fallback.level     = LogLevel::INFO;
        fallback.module    = "fallback";
        fallback.meta["source_file"] = source;

        // 没有采集器匹配时，从原始行里猜一个级别
        auto has = [&raw_line](const char* needle) {
            return std::search(raw_line.begin(), raw_line.end(),
                               needle, needle + std::strlen(needle),
                               [](char a, char b) {
                                   return std::toupper(static_cast<unsigned char>(a)) ==
                                          static_cast<unsigned char>(b);
                               }) != raw_line.end();
        };
        if      (has("FATAL")) fallback.level = LogLevel::FATAL;
        else if (has("ERROR")) fallback.level = LogLevel::ERROR;
        else if (has("WARN"))  fallback.level = LogLevel::WARN;
        else if (has("DEBUG")) fallback.level = LogLevel::DEBUG;

        std::vector<LogEntry> result;
        result.push_back(std::move(fallback));
        return result;
    }

    ICollector::ParseResult parse_result;
    try {
        // parse_line 而不是自己造 istringstream；采集器可以覆写它来避免流对象
        parse_result = collector->parse_line(raw_line);
    } catch (const std::exception& e) {
        std::cerr << "Collector '" << collector->name()
                  << "' threw: " << e.what() << std::endl;
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        ++m_stats.total_errors;
        return {};
    }

    for (auto& entry : parse_result.entries) {
        if (entry.source.empty()) entry.source = collector->name();
        if (entry.raw.empty())    entry.raw    = raw_line;
        entry.meta["source_file"] = source;
    }
    return std::move(parse_result.entries);
}

std::vector<LogEntry> Pipeline::run_analyzers(const PluginSet& plugins,
                                             std::vector<LogEntry> entries) {
    for (auto* analyzer : plugins.analyzers) {
        try {
            entries = analyzer->analyze(std::move(entries));
            if (entries.empty()) break;
        } catch (const std::exception& e) {
            std::cerr << "Analyzer '" << analyzer->name()
                      << "' threw exception: " << e.what() << std::endl;
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            ++m_stats.total_errors;
        }
    }
    return entries;
}

// 按规则挑出真正需要告警的条目：级别阈值 → 指纹去重 → 全局限流
std::vector<LogEntry> Pipeline::select_alerting(const std::vector<LogEntry>& entries) {
    const auto now = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(m_alert_mutex);
    std::vector<LogEntry> selected;
    size_t suppressed = 0;

    if (now - m_rate_window_start >= std::chrono::minutes(1)) {
        m_rate_window_start = now;
        m_rate_count        = 0;
    }

    for (const auto& entry : entries) {
        if (!level_at_least(entry.level, m_alert_rule.min_level)) continue;

        if (m_alert_rule.max_alerts_per_minute > 0 &&
            m_rate_count >= m_alert_rule.max_alerts_per_minute) {
            ++suppressed;
            continue;
        }

        const std::string key = alert_key(entry);
        auto it = m_recent_alerts.find(key);
        if (it != m_recent_alerts.end() && (now - it->second) < m_alert_rule.dedup_window) {
            ++suppressed;
            continue;
        }
        m_recent_alerts[key] = now;
        ++m_rate_count;
        selected.push_back(entry);
    }

    // 去重表超限时先清掉过期项，还是超就整体丢弃（宁可多告警也不要无界增长）
    if (m_recent_alerts.size() > m_alert_rule.dedup_cache_size) {
        for (auto it = m_recent_alerts.begin(); it != m_recent_alerts.end(); ) {
            it = (now - it->second >= m_alert_rule.dedup_window)
                 ? m_recent_alerts.erase(it) : std::next(it);
        }
        if (m_recent_alerts.size() > m_alert_rule.dedup_cache_size) m_recent_alerts.clear();
    }

    if (suppressed > 0) {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats.total_suppressed += suppressed;
    }
    return selected;
}

void Pipeline::run_alerts(const PluginSet& plugins, const std::vector<LogEntry>& entries) {
    if (entries.empty() || plugins.alerts.empty()) return;

    auto triggered = select_alerting(entries);
    if (triggered.empty()) return;

    LogLevel worst = triggered.front().level;
    for (const auto& e : triggered) {
        if (level_at_least(e.level, worst)) worst = e.level;
    }

    AlertContext ctx;
    ctx.rule_name     = std::string("level>=") + to_string(alert_rule().min_level);
    ctx.severity      = severity_of(worst);
    ctx.summary       = std::to_string(triggered.size()) + " entry(ies) at " +
                        to_string(worst) + ": " + triggered.front().message;
    ctx.triggered_at  = std::chrono::system_clock::now();
    ctx.source_plugin = "pipeline";
    ctx.extra         = {{"matched", triggered.size()}, {"scanned", entries.size()}};
    ctx.entries       = std::move(triggered);

    const auto sent = ctx.entries.size();
    for (auto* alert : plugins.alerts) {
        try {
            alert->send(ctx);
        } catch (const std::exception& e) {
            std::cerr << "Alert '" << alert->name()
                      << "' threw exception: " << e.what() << std::endl;
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            ++m_stats.total_errors;
        }
    }

    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.total_alerts += sent;
}

void Pipeline::note_processed(size_t count) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.total_processed += count;
    m_qps_window_count      += count;

    // 5 秒滑动窗口。qps 字段原先恒为 0.0，从未被赋值过。
    const auto now     = std::chrono::steady_clock::now();
    const auto elapsed = now - m_qps_window_start;
    if (elapsed >= kQpsWindow) {
        const double secs = std::chrono::duration<double>(elapsed).count();
        m_stats.qps = (secs > 0.0)
                    ? static_cast<double>(m_qps_window_count) / secs
                    : 0.0;
        m_qps_window_start = now;
        m_qps_window_count = 0;
    }
}

Pipeline::Stats Pipeline::stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    Stats snapshot = m_stats;

    // 当前窗口还没满时，用已过去的这段时间算即时速率。
    // 否则守护进程启动后的前 5 秒 qps 一直是 0，看起来像没在工作。
    const auto   elapsed = std::chrono::steady_clock::now() - m_qps_window_start;
    const double secs    = std::chrono::duration<double>(elapsed).count();
    if (m_qps_window_count > 0 && secs > 0.0) {
        snapshot.qps = static_cast<double>(m_qps_window_count) / secs;
    }
    return snapshot;
}

// 告警指纹：模块 + 级别 + 归一化后的消息。
// 把数字串统一替换成 '#'，让 "timeout after 30s" 和 "timeout after 45s"
// 落到同一个指纹上，否则去重基本不起作用。
std::string Pipeline::alert_key(const LogEntry& entry) {
    std::string key = entry.module;
    key += '|';
    key += to_string(entry.level);
    key += '|';

    bool in_digits = false;
    for (const char c : entry.message) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            if (!in_digits) {
                key += '#';
                in_digits = true;
            }
            continue;
        }
        in_digits = false;
        key += c;
        if (key.size() >= kAlertKeyMaxLen) break;
    }
    return key;
}

const char* Pipeline::severity_of(LogLevel level) {
    switch (level) {
        case LogLevel::FATAL: return "critical";
        case LogLevel::ERROR: return "error";
        case LogLevel::WARN:  return "warning";
        case LogLevel::INFO:  return "info";
        case LogLevel::DEBUG: return "debug";
    }
    return "info";
}

} // namespace logmind
