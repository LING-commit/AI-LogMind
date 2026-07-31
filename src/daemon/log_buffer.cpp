#include <logmind/log_buffer.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace logmind {

LogBuffer::LogBuffer(size_t capacity)
    : m_capacity(capacity == 0 ? 1 : capacity)
    // 分成 8 段左右：段太大则淘汰粒度粗（索引里留着已失效的 posting），
    // 段太小则查询要遍历很多段。
    , m_segment_span(std::max<size_t>(1024, (capacity == 0 ? 1 : capacity) / 8))
{
}

void LogBuffer::push(LogEntry entry) {
    std::shared_ptr<const EntryCallback> callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const uint64_t id = m_next_id++;
        if (m_items.empty()) m_oldest_id = id;

        callback = m_callback;
        // 有回调时留一份给锁外使用；没有回调就直接移动进去，省掉一次拷贝
        if (callback && *callback) m_items.push_back(entry);
        else                       m_items.push_back(std::move(entry));

        index_entry_locked(m_items.back().message, id);
        ++m_total_pushed;

        while (m_items.size() > m_capacity) {
            m_items.pop_front();
            ++m_oldest_id;
            ++m_total_discarded;
        }
        evict_segments_locked();
    }

    // 回调放在锁外。旧实现在持锁状态下调用它，任何在回调里回查 buffer 的
    // 代码都会立刻自锁死。
    if (callback && *callback) (*callback)(entry);
}

void LogBuffer::set_on_entry_callback(EntryCallback cb) {
    auto holder = std::make_shared<const EntryCallback>(std::move(cb));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(holder);
}

void LogBuffer::index_entry_locked(const std::string& message, uint64_t id) {
    if (m_segments.empty() ||
        (m_segments.back().last_id - m_segments.back().first_id + 1) >= m_segment_span) {
        Segment seg;
        seg.first_id = id;
        seg.last_id  = id;
        m_segments.push_back(std::move(seg));
    }

    auto& seg = m_segments.back();
    seg.last_id = id;

    for (auto& token : tokenize(message)) {
        auto& postings = seg.index[std::move(token)];
        // 同一条消息里重复出现的词只记一次
        if (postings.empty() || postings.back() != id) postings.push_back(id);
    }
}

void LogBuffer::evict_segments_locked() {
    // 整段丢弃：一段里所有 id 都已经不在缓冲区里了，这段就没用了
    while (m_segments.size() > 1 && m_segments.front().last_id < m_oldest_id) {
        m_segments.pop_front();
    }
}

const LogEntry* LogBuffer::entry_for_locked(uint64_t id) const {
    if (id < m_oldest_id) return nullptr;                     // 已被淘汰
    const size_t idx = id - m_oldest_id;
    return (idx < m_items.size()) ? &m_items[idx] : nullptr;
}

std::vector<LogEntry> LogBuffer::query_by_time(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end,
    size_t limit) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<LogEntry> result;
    if (limit == 0) return result;
    result.reserve(std::min(limit, m_items.size()));

    for (const auto& e : m_items) {
        if (e.timestamp < start || e.timestamp > end) continue;
        result.push_back(e);
        if (result.size() >= limit) break;
    }
    return result;
}

std::vector<LogEntry> LogBuffer::search(const std::string& query, size_t limit) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<LogEntry> result;
    if (limit == 0 || m_items.empty()) return result;
    result.reserve(std::min(limit, m_items.size()));

    if (query.empty()) {
        for (auto it = m_items.rbegin(); it != m_items.rend() && result.size() < limit; ++it) {
            result.push_back(*it);
        }
        return result;
    }

    // 1) 走倒排索引，从最新的段往回找
    const auto tokens = tokenize(query);
    if (!tokens.empty()) {
        for (auto seg = m_segments.rbegin();
             seg != m_segments.rend() && result.size() < limit; ++seg) {
            const auto ids = lookup_segment(*seg, tokens);
            // posting 是升序的，要「新的在前」就反着遍历
            for (auto it = ids.rbegin(); it != ids.rend() && result.size() < limit; ++it) {
                if (const LogEntry* e = entry_for_locked(*it)) result.push_back(*e);
            }
        }
        if (!result.empty()) return result;
    }

    // 2) 索引未命中时回退到线性扫描。
    //    例如查 "err" 时分词得到 token "err"，而索引里存的是 "error"，
    //    倒排索引做不了前缀/子串匹配。
    const std::string needle = to_lower(query);
    for (auto it = m_items.rbegin(); it != m_items.rend() && result.size() < limit; ++it) {
        if (contains_ci(it->message, needle)) result.push_back(*it);
    }
    return result;
}

std::vector<uint64_t> LogBuffer::lookup_segment(
    const Segment& seg, const std::vector<std::string>& tokens)
{
    // 先找最短的 posting list 当基准，再用其余 token 过滤（AND 语义）
    const std::vector<uint64_t>* shortest = nullptr;
    for (const auto& token : tokens) {
        auto it = seg.index.find(token);
        if (it == seg.index.end()) return {};      // 有 token 不在本段 → 本段无匹配
        if (!shortest || it->second.size() < shortest->size()) shortest = &it->second;
    }
    if (!shortest) return {};

    std::vector<uint64_t> result = *shortest;
    for (const auto& token : tokens) {
        auto it = seg.index.find(token);
        if (&it->second == shortest) continue;

        // unordered_set 而非 std::set：这里只要成员判定，不需要有序
        const std::unordered_set<uint64_t> filter(it->second.begin(), it->second.end());
        result.erase(std::remove_if(result.begin(), result.end(),
                        [&](uint64_t id) { return filter.count(id) == 0; }),
                     result.end());
        if (result.empty()) break;
    }
    return result;
}

size_t LogBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items.size();
}

size_t LogBuffer::total_pushed() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total_pushed;
}

size_t LogBuffer::total_discarded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_total_discarded;
}

std::vector<std::string> LogBuffer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (const char c : text) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || uc == '_' || uc == '-') {
            current.push_back(static_cast<char>(std::tolower(uc)));
        } else if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) tokens.push_back(std::move(current));
    return tokens;
}

std::string LogBuffer::to_lower(const std::string& s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return out;
}

// 大小写不敏感的子串匹配，不复制 haystack。
// 旧实现对每条 entry 都要复制一份 message 再整体 tolower。
bool LogBuffer::contains_ci(const std::string& haystack, const std::string& needle_lower) {
    if (needle_lower.empty()) return true;
    if (needle_lower.size() > haystack.size()) return false;

    const auto it = std::search(
        haystack.begin(), haystack.end(),
        needle_lower.begin(), needle_lower.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    return it != haystack.end();
}

} // namespace logmind
