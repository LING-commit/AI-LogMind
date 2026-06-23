#pragma once
#include <logmind/pipeline.h>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <unordered_map>
#include <chrono>

namespace logmind {

class FileWatcher {
public:
    explicit FileWatcher(Pipeline& pipeline,
                         std::chrono::milliseconds poll_interval = std::chrono::seconds(1));

    void watch(const std::string& path);
    void watch_directory(const std::string& dir, const std::string& pattern = "");
    void add_patterns(const std::vector<std::string>& patterns);

    void start();
    void stop();
    bool is_running() const { return m_running; }

private:
    void inotify_loop();
    void poll_loop();

    Pipeline& m_pipeline;
    std::chrono::milliseconds m_poll_interval;

    std::thread    m_thread;
    std::atomic<bool> m_running{false};

    // ── inotify ──
    int  m_inotify_fd = -1;
    std::map<int, std::string> m_watch_map;

    // ── fallback 轮询 ──
    std::unordered_map<std::string, uintmax_t> m_file_sizes;

    // ── 监听列表 ──
    std::vector<std::string> m_watch_paths;
};

} // namespace logmind
