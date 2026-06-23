#include <logmind/log_buffer.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <iterator>

namespace logmind {

LogBuffer::LogBuffer(size_t capacity)
    : m_capacity(capacity)
{
}

void LogBuffer::push(LogEntry entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = m_next_id++;
    m_items.push_back(std::move(entry));
    m_ids.push_back(id);
    m_total_pushed++;

    // Index the new entry
    m_search_index.add_entry(m_items.back().message, id);
    m_total_indexed++;

    if (m_items.size() > m_capacity) {
        m_items.pop_front();
        m_ids.pop_front();
        m_total_discarded++;
    }

    if (m_callback && !m_items.empty()) {
        m_callback(m_items.back());
    }
}

std::vector<LogEntry> LogBuffer::query_by_time(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end,
    size_t limit) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogEntry> result;
    result.reserve(std::min(limit, m_items.size()));

    for (const auto& e : m_items) {
        if (e.timestamp >= start && e.timestamp <= end) {
            result.push_back(e);
            if (result.size() >= limit) break;
        }
    }

    return result;
}

std::vector<LogEntry> LogBuffer::search(const std::string& query,
                                         size_t limit) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogEntry> result;

    if (query.empty()) {
        // Return most recent entries (up to limit)
        for (auto it = m_items.rbegin(); it != m_items.rend() && result.size() < limit; ++it) {
            result.push_back(*it);
        }
        return result;
    }

    // Step 1: look up index
    auto candidate_ids = m_search_index.lookup(query);

    // Step 2: if the index has candidates, map IDs back to entries
    if (!candidate_ids.empty()) {
        // Build fast ID→index map (recent entries are at the end of m_ids)
        // Since m_ids is sorted by age, do a reverse scan for most recent matches
        auto id_set = std::set<uint64_t>(candidate_ids.begin(), candidate_ids.end());

        // Reverse scan (newest first), collect up to limit
        for (auto it = m_ids.rbegin(); it != m_ids.rend() && result.size() < limit; ++it) {
            if (id_set.count(*it)) {
                auto dist = std::distance(it, m_ids.rend()) - 1;
                auto idx = static_cast<size_t>(dist);
                if (idx < m_items.size()) {
                    result.push_back(m_items[idx]);
                }
            }
        }
        return result;
    }

    // Step 3: fallback to linear scan (for single-use or rare queries)
    auto q = query;
    std::transform(q.begin(), q.end(), q.begin(),
                   [](unsigned char c) -> unsigned char {
                       return static_cast<unsigned char>(std::tolower(static_cast<int>(c)));
                   });

    for (auto it = m_items.rbegin(); it != m_items.rend() && result.size() < limit; ++it) {
        auto msg = it->message;
        std::transform(msg.begin(), msg.end(), msg.begin(),
                       [](unsigned char c) -> unsigned char {
                           return static_cast<unsigned char>(std::tolower(static_cast<int>(c)));
                       });
        if (msg.find(q) != std::string::npos) {
            result.push_back(*it);
        }
    }

    return result;
}

size_t LogBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items.size();
}

// ── SearchIndex ──
void LogBuffer::SearchIndex::add_entry(
    const std::string& message, uint64_t global_id)
{
    auto tokens = tokenize(message);
    std::unique_lock<std::shared_mutex> lock(m_rw_mutex);
    for (const auto& token : tokens) {
        if (!token.empty()) {
            m_index[token].push_back(global_id);
        }
    }
}

std::vector<uint64_t>
LogBuffer::SearchIndex::lookup(const std::string& query) const {
    auto tokens = tokenize(query);
    if (tokens.empty()) return {};

    std::shared_lock<std::shared_mutex> lock(m_rw_mutex);

    // Find the token with the shortest posting list
    auto best_it = m_index.end();
    size_t best_len = SIZE_MAX;
    for (const auto& t : tokens) {
        auto it = m_index.find(t);
        if (it == m_index.end()) return {};  // missing token → no match
        if (it->second.size() < best_len) {
            best_len = it->second.size();
            best_it = it;
        }
    }

    if (best_it == m_index.end()) return {};

    // Use the shortest posting list, then filter by other tokens
    std::vector<uint64_t> result = best_it->second;
    for (const auto& t : tokens) {
        if (m_index.find(t)->first == best_it->first) continue;
        auto& postings = m_index.find(t)->second;
        auto post_set = std::set<uint64_t>(postings.begin(), postings.end());
        result.erase(
            std::remove_if(result.begin(), result.end(),
                [&](uint64_t id) { return post_set.count(id) == 0; }),
            result.end());
    }

    return result;
}

std::vector<std::string>
LogBuffer::SearchIndex::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (const auto c : text) {
        auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || uc == '_' || uc == '-') {
            current.push_back(static_cast<char>(std::tolower(static_cast<int>(uc))));
        } else {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
        }
    }
    if (!current.empty()) tokens.push_back(std::move(current));
    return tokens;
}

} // namespace logmind
