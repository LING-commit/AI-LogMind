#include <logmind/file_watcher.h>
#include <logmind/pipeline.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <fnmatch.h>
#include <sys/stat.h>

#ifdef __linux__
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace logmind {

namespace {

// 单行上限。超过就强制切一刀，免得把二进制文件或异常长的行无限缓冲下去。
constexpr size_t kMaxLineBytes = 1u << 20;   // 1 MiB
constexpr size_t kReadChunk    = 64u * 1024u;

std::string parent_dir_of(const std::string& path) {
    const auto sep = path.rfind('/');
    if (sep == std::string::npos) return ".";
    if (sep == 0) return "/";
    return path.substr(0, sep);
}

std::string filename_of(const std::string& path) {
    const auto sep = path.rfind('/');
    return (sep == std::string::npos) ? path : path.substr(sep + 1);
}

std::string join_path(const std::string& dir, const std::string& name) {
    if (dir.empty() || dir == ".") return name;
    if (dir.back() == '/')          return dir + name;
    return dir + "/" + name;
}

} // namespace

FileWatcher::FileWatcher(Pipeline& pipeline, std::chrono::milliseconds poll_interval)
    : m_pipeline(pipeline)
    , m_poll_interval(poll_interval)
{
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::register_pattern(const std::string& dir, const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    auto& patterns = m_dir_patterns[dir];
    if (std::find(patterns.begin(), patterns.end(), pattern) == patterns.end()) {
        patterns.push_back(pattern);
    }
}

void FileWatcher::watch(const std::string& path) {
    // 单个文件也按「父目录 + 精确文件名」注册，这样轮转后能跟上新文件
    register_pattern(parent_dir_of(path), filename_of(path));
}

void FileWatcher::watch_directory(const std::string& dir, const std::string& pattern) {
    register_pattern(dir, pattern.empty() ? "*" : pattern);
}

void FileWatcher::add_patterns(const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        const bool has_glob = pattern.find_first_of("*?[") != std::string::npos;
        if (has_glob) {
            watch_directory(parent_dir_of(pattern), filename_of(pattern));
        } else {
            watch(pattern);
        }
    }
}

bool FileWatcher::matches(const std::string& dir, const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    auto it = m_dir_patterns.find(dir);
    if (it == m_dir_patterns.end()) return false;
    for (const auto& pattern : it->second) {
        if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) return true;
    }
    return false;
}

// prime=true 表示这是启动时的首次扫描：把偏移直接设到文件末尾，只读之后的新增内容。
// prime=false 用于运行期发现的新文件，从头读。
void FileWatcher::scan_directory(const std::string& dir, bool prime) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;

        const std::string name = entry.path().filename().string();
        if (!matches(dir, name)) continue;

        const std::string path = entry.path().string();
        if (m_files.count(path)) continue;   // 已经在跟踪

        FileState state;
        struct stat sb {};
        if (::stat(path.c_str(), &sb) == 0) {
            state.inode  = static_cast<uint64_t>(sb.st_ino);
            state.offset = prime ? static_cast<uintmax_t>(sb.st_size) : 0;
        }
        m_files.emplace(path, std::move(state));

        if (!prime) drain_file(path);
    }
}

void FileWatcher::flush_pending(const std::string& path, FileState& state) {
    size_t start = 0;
    for (size_t i = 0; i < state.pending.size(); ++i) {
        if (state.pending[i] != '\n') continue;

        size_t end = i;
        if (end > start && state.pending[end - 1] == '\r') --end;   // 兼容 CRLF
        m_pipeline.process_raw(path, state.pending.substr(start, end - start));
        start = i + 1;
    }
    state.pending.erase(0, start);

    if (state.pending.size() > kMaxLineBytes) {
        std::cerr << "FileWatcher: line exceeds " << kMaxLineBytes
                  << " bytes in " << path << ", flushing without newline" << std::endl;
        m_pipeline.process_raw(path, state.pending);
        state.pending.clear();
    }
}

void FileWatcher::drain_file(const std::string& path) {
    struct stat sb {};
    if (::stat(path.c_str(), &sb) != 0) return;
    if (!S_ISREG(sb.st_mode)) return;

    auto& state = m_files[path];
    const auto inode = static_cast<uint64_t>(sb.st_ino);
    const auto size  = static_cast<uintmax_t>(sb.st_size);

    if (state.inode != 0 && state.inode != inode) {
        // inode 变了：logrotate 之类把文件换掉了，从头读新文件
        state.offset = 0;
        state.pending.clear();
    } else if (size < state.offset) {
        std::cerr << "FileWatcher: file truncated, resetting offset: " << path << std::endl;
        state.offset = 0;
        state.pending.clear();
    }
    state.inode = inode;

    if (size <= state.offset) return;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    file.seekg(static_cast<std::streamoff>(state.offset));

    std::string buffer(kReadChunk, '\0');
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto got = static_cast<size_t>(file.gcount());
        if (got == 0) break;

        state.pending.append(buffer, 0, got);
        state.offset += got;

        // 只消费到最后一个换行符为止，剩下的半行留到下次
        flush_pending(path, state);
    }
}

void FileWatcher::start() {
    if (m_running.exchange(true)) return;

    std::vector<std::string> dirs;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        dirs.reserve(m_dir_patterns.size());
        for (const auto& [dir, patterns] : m_dir_patterns) {
            (void)patterns;
            dirs.push_back(dir);
        }
    }

    // 启动时把已有内容跳过，只处理之后的新增行
    for (const auto& dir : dirs) scan_directory(dir, /*prime=*/true);

#ifdef __linux__
    m_stop_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    m_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotify_fd >= 0 && m_stop_fd >= 0) {
        size_t registered = 0;
        for (const auto& dir : dirs) {
            const int wd = inotify_add_watch(
                m_inotify_fd, dir.c_str(),
                IN_MODIFY | IN_CREATE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE);
            if (wd >= 0) {
                m_watch_map[wd] = dir;
                ++registered;
            } else {
                std::cerr << "FileWatcher: cannot watch " << dir
                          << ": " << std::strerror(errno) << std::endl;
            }
        }
        if (registered > 0 || dirs.empty()) {
            m_thread = std::thread(&FileWatcher::inotify_loop, this);
            return;
        }
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }
#endif

    // fallback：轮询模式
    m_thread = std::thread(&FileWatcher::poll_loop, this);
}

void FileWatcher::stop() {
    if (!m_running.exchange(false)) return;

#ifdef __linux__
    if (m_stop_fd >= 0) {
        // 立刻叫醒阻塞在 poll() 上的线程，不用等超时
        const uint64_t token = 1;
        const ssize_t written = ::write(m_stop_fd, &token, sizeof(token));
        (void)written;
    }
#endif

    if (m_thread.joinable()) m_thread.join();

#ifdef __linux__
    if (m_inotify_fd >= 0) {
        for (const auto& [wd, dir] : m_watch_map) {
            (void)dir;
            inotify_rm_watch(m_inotify_fd, wd);
        }
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }
    m_watch_map.clear();

    if (m_stop_fd >= 0) {
        close(m_stop_fd);
        m_stop_fd = -1;
    }
#endif
}

void FileWatcher::inotify_loop() {
#ifdef __linux__
    // inotify_event 尾部是柔性数组，缓冲区必须按它对齐
    alignas(struct inotify_event) char buffer[8192];

    struct pollfd fds[2];
    fds[0] = {m_inotify_fd, POLLIN, 0};
    fds[1] = {m_stop_fd,    POLLIN, 0};

    while (m_running) {
        // 阻塞等待。旧实现在非阻塞 fd 上直接 read，拿到 EAGAIN 就 sleep(100ms)，
        // 等于把 inotify 退化成了 100ms 轮询。
        const int rc = ::poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::cerr << "FileWatcher: poll failed: " << std::strerror(errno) << std::endl;
            break;
        }
        if (fds[1].revents & POLLIN) break;          // stop() 唤醒
        if (!(fds[0].revents & POLLIN)) continue;

        const ssize_t len = ::read(m_inotify_fd, buffer, sizeof(buffer));
        if (len <= 0) continue;

        for (char* ptr = buffer; ptr < buffer + len; ) {
            const auto* event = reinterpret_cast<const struct inotify_event*>(ptr);
            ptr += sizeof(struct inotify_event) + event->len;

            if (event->mask & IN_Q_OVERFLOW) {
                // 事件队列溢出，可能漏了改动：全量重扫已跟踪的文件
                std::cerr << "FileWatcher: inotify queue overflow, rescanning" << std::endl;
                for (const auto& [wd, dir] : m_watch_map) {
                    (void)wd;
                    scan_directory(dir, /*prime=*/false);
                }
                for (const auto& [path, state] : m_files) {
                    (void)state;
                    drain_file(path);
                }
                continue;
            }

            auto it = m_watch_map.find(event->wd);
            if (it == m_watch_map.end() || event->len == 0) continue;

            const std::string& dir = it->second;
            const std::string name(event->name);   // 尾部有填充 NUL，构造时会自动截断
            if (!matches(dir, name)) continue;

            const std::string path = join_path(dir, name);

            if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                m_files.erase(path);
                continue;
            }
            if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                // 新建或轮转进来的文件：从头读
                auto& state  = m_files[path];
                state.offset = 0;
                state.inode  = 0;
                state.pending.clear();
            }
            drain_file(path);
        }
    }
#endif
}

void FileWatcher::poll_loop() {
    const auto interval_ms = static_cast<int>(m_poll_interval.count());

    while (m_running) {
        std::vector<std::string> dirs;
        {
            std::lock_guard<std::mutex> lock(m_config_mutex);
            dirs.reserve(m_dir_patterns.size());
            for (const auto& [dir, patterns] : m_dir_patterns) {
                (void)patterns;
                dirs.push_back(dir);
            }
        }
        // 轮询模式下也要发现运行期新出现的文件
        for (const auto& dir : dirs) scan_directory(dir, /*prime=*/false);

        for (const auto& [path, state] : m_files) {
            (void)state;
            drain_file(path);
        }

#ifdef __linux__
        if (m_stop_fd >= 0) {
            struct pollfd stop_fd = {m_stop_fd, POLLIN, 0};
            if (::poll(&stop_fd, 1, interval_ms) > 0) break;   // stop() 唤醒
            continue;
        }
#endif
        std::this_thread::sleep_for(m_poll_interval);
    }
}

} // namespace logmind
