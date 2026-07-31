#pragma once
#include <logmind/pipeline.h>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace logmind {

// 增量跟踪一批日志文件的新增内容。
//
// 监控的是**目录**而不是单个文件：inotify 的 watch 跟着 inode 走，
// 对单个文件加 watch 的话，logrotate（rename + 新建）之后就一直盯着旧 inode，
// 新文件永远读不到。监控目录并处理 IN_CREATE / IN_MOVED_TO 才能跟上轮转。
class FileWatcher {
public:
    explicit FileWatcher(Pipeline& pipeline,
                         std::chrono::milliseconds poll_interval = std::chrono::seconds(1));
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // 以下三个都必须在 start() 之前调用。
    void watch(const std::string& path);
    void watch_directory(const std::string& dir, const std::string& pattern = "");
    void add_patterns(const std::vector<std::string>& patterns);

    void start();
    void stop();
    bool is_running() const { return m_running; }

private:
    struct FileState {
        uintmax_t   offset  = 0;
        uint64_t    inode   = 0;
        // 还没等到换行符的残段。必须跨读取保留，否则写了一半的行会被
        // 当成完整日志处理掉，剩下半行下次又成为独立一条。
        std::string pending;
    };

    void inotify_loop();
    void poll_loop();

    void register_pattern(const std::string& dir, const std::string& pattern);
    void scan_directory(const std::string& dir, bool prime);
    bool matches(const std::string& dir, const std::string& filename) const;
    void drain_file(const std::string& path);
    void flush_pending(const std::string& path, FileState& state);

    Pipeline& m_pipeline;
    std::chrono::milliseconds m_poll_interval;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    int m_inotify_fd = -1;
    int m_stop_fd    = -1;    // eventfd，用来立刻叫醒阻塞中的 poll()

    std::map<int, std::string> m_watch_map;   // inotify wd → 目录

    mutable std::mutex m_config_mutex;
    std::unordered_map<std::string, std::vector<std::string>> m_dir_patterns;

    // 只由监控线程访问
    std::unordered_map<std::string, FileState> m_files;
};

} // namespace logmind
