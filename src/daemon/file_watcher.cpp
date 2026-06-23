#include <logmind/file_watcher.h>
#include <logmind/pipeline.h>
#include <fstream>
#include <iostream>
#include <fnmatch.h>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace logmind {

FileWatcher::FileWatcher(Pipeline& pipeline,
                         std::chrono::milliseconds poll_interval)
    : m_pipeline(pipeline)
    , m_poll_interval(poll_interval)
{
}

void FileWatcher::watch(const std::string& path) {
    m_watch_paths.push_back(path);
}

void FileWatcher::watch_directory(const std::string& dir, const std::string& pattern) {
    if (!std::filesystem::is_directory(dir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (!pattern.empty() &&
            fnmatch(pattern.c_str(), entry.path().filename().c_str(), 0) != 0)
            continue;
        watch(entry.path().string());
    }
}

void FileWatcher::add_patterns(const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        auto star = pattern.find('*');
        auto qpos = pattern.find('?');
        bool has_glob = (star != std::string::npos || qpos != std::string::npos);

        if (has_glob) {
            auto sep = pattern.rfind('/');
            std::string dir = (sep != std::string::npos)
                              ? pattern.substr(0, sep)
                              : ".";
            std::string glob = pattern.substr(sep + 1);
            watch_directory(dir, glob);
        } else {
            watch(pattern);
        }
    }
}

void FileWatcher::start() {
    if (m_running.exchange(true)) return;

    // 初始化所有文件偏移到当前大小（只读新增行）
    for (const auto& path : m_watch_paths) {
        try {
            m_file_sizes[path] = std::filesystem::file_size(path);
        } catch (...) {
            m_file_sizes[path] = 0;
        }
    }

#ifdef __linux__
    m_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (m_inotify_fd >= 0) {
        for (const auto& path : m_watch_paths) {
            int wd = inotify_add_watch(m_inotify_fd, path.c_str(),
                                        IN_MODIFY | IN_CREATE);
            if (wd >= 0) {
                m_watch_map[wd] = path;
            }
        }
        m_thread = std::thread(&FileWatcher::inotify_loop, this);
        return;
    }
#endif

    // fallback: 轮询模式（偏移已在上面初始化）
    m_thread = std::thread(&FileWatcher::poll_loop, this);
}

void FileWatcher::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }

#ifdef __linux__
    if (m_inotify_fd >= 0) {
        for (const auto& [wd, path] : m_watch_map) {
            (void)path;
            inotify_rm_watch(m_inotify_fd, wd);
        }
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }
#endif
}

void FileWatcher::inotify_loop() {
#ifdef __linux__
    char buffer[4096];
    while (m_running) {
        ssize_t len = read(m_inotify_fd, buffer, sizeof(buffer));
        if (len < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        char* ptr = buffer;
        while (ptr < buffer + len) {
            auto* event = reinterpret_cast<struct inotify_event*>(ptr);
            auto it = m_watch_map.find(event->wd);
            if (it != m_watch_map.end()) {
                auto& path = it->second;
                std::ifstream file(path);
                if (!file.is_open()) { ptr += sizeof(struct inotify_event) + event->len; continue; }
                auto& offset = m_file_sizes[path];
                file.seekg(static_cast<std::streamoff>(offset));
                std::string line;
                while (std::getline(file, line)) {
                    m_pipeline.process_raw(path, line);
                }
                file.clear();
                auto pos = file.tellg();
                if (pos > 0) offset = static_cast<uintmax_t>(pos);
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
#endif
}

void FileWatcher::poll_loop() {
    while (m_running) {
        for (auto& [path, last_size] : m_file_sizes) {
            try {
                uintmax_t current_size = std::filesystem::file_size(path);
                if (current_size > last_size) {
                    std::ifstream file(path);
                    if (file.is_open()) {
                        file.seekg(static_cast<std::streamoff>(last_size));
                        std::string line;
                        while (std::getline(file, line)) {
                            m_pipeline.process_raw(path, line);
                        }
                        last_size = current_size;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "FileWatcher error: " << e.what() << std::endl;
            }
        }
        std::this_thread::sleep_for(m_poll_interval);
    }
}

} // namespace logmind
