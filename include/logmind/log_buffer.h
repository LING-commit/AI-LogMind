#pragma once
#include <logmind/log_entry.h>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <cstdint>

namespace logmind {

class LogBuffer {
public:
    explicit LogBuffer(size_t capacity = 50000);

    void push(LogEntry entry);

    std::vector<LogEntry> query_by_time(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end,
        size_t limit = 1000) const;

    std::vector<LogEntry> search(const std::string& query,
                                  size_t limit = 1000) const;

    size_t size()     const;
    size_t capacity() const { return m_capacity; }
    size_t total_pushed()    const { return m_total_pushed; }
    size_t total_discarded() const { return m_total_discarded; }

    using EntryCallback = std::function<void(const LogEntry&)>;
    void set_on_entry_callback(EntryCallback cb) { m_callback = std::move(cb); }

private:
    class SearchIndex {
    public:
        void add_entry(const std::string& message, uint64_t global_id);
        std::vector<uint64_t> lookup(const std::string& query) const;
    private:
        static std::vector<std::string> tokenize(const std::string& text);
        std::unordered_map<std::string, std::vector<uint64_t>> m_index;
        mutable std::shared_mutex m_rw_mutex;
    };

    mutable std::mutex       m_mutex;
    std::deque<LogEntry>     m_items;
    std::deque<uint64_t>     m_ids;           // parallel id deque for index lookup
    size_t                   m_capacity;
    uint64_t                 m_next_id = 1;   // monotonically increasing global id
    size_t                   m_total_pushed    = 0;
    size_t                   m_total_discarded = 0;
    size_t                   m_total_indexed   = 0;
    EntryCallback            m_callback;
    SearchIndex              m_search_index;
};

} // namespace logmind
