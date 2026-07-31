#pragma once
#include <logmind/log_entry.h>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace logmind {

class LogBuffer {
public:
    explicit LogBuffer(size_t capacity = 50000);

    void push(LogEntry entry);

    std::vector<LogEntry> query_by_time(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end,
        size_t limit = 1000) const;

    // 命中的条目按「新的在前」返回。
    std::vector<LogEntry> search(const std::string& query,
                                  size_t limit = 1000) const;

    size_t size()            const;
    size_t capacity()        const { return m_capacity; }
    size_t total_pushed()    const;
    size_t total_discarded() const;

    using EntryCallback = std::function<void(const LogEntry&)>;
    void set_on_entry_callback(EntryCallback cb);

private:
    // 分段倒排索引。
    //
    // 旧实现只有 add_entry 没有任何删除路径：环形缓冲淘汰条目时对应的 posting
    // 永远留在索引里，长跑必然把内存吃光。分段之后，最老的一段随缓冲区整段丢弃，
    // 不需要逐 token 去删 posting。
    struct Segment {
        uint64_t first_id = 0;    // 本段覆盖的 id 区间 [first_id, last_id]
        uint64_t last_id  = 0;
        std::unordered_map<std::string, std::vector<uint64_t>> index;
    };

    static std::vector<std::string> tokenize(const std::string& text);
    static std::string to_lower(const std::string& s);
    static bool contains_ci(const std::string& haystack, const std::string& needle_lower);
    static std::vector<uint64_t> lookup_segment(const Segment& seg,
                                                const std::vector<std::string>& tokens);

    void index_entry_locked(const std::string& message, uint64_t id);
    void evict_segments_locked();
    const LogEntry* entry_for_locked(uint64_t id) const;

    mutable std::mutex   m_mutex;
    std::deque<LogEntry> m_items;
    std::deque<Segment>  m_segments;

    size_t   m_capacity;
    size_t   m_segment_span;
    // 不变式：m_items[i] 的全局 id 恒等于 m_oldest_id + i。
    // id 连续递增分配、淘汰只从头部发生，所以 id → 下标是 O(1)，
    // 不必再维护一条与 m_items 平行的 id 队列。
    uint64_t m_oldest_id       = 1;
    uint64_t m_next_id         = 1;
    size_t   m_total_pushed    = 0;
    size_t   m_total_discarded = 0;

    // 用 shared_ptr 包一层，push() 里复制它只是一次原子加，
    // 比复制 std::function 便宜，也让回调能安全地在锁外调用。
    std::shared_ptr<const EntryCallback> m_callback;
};

} // namespace logmind
