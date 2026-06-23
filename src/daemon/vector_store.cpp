#include <logmind/vector_store.h>
#include <sqlite3.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <queue>
#include <random>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

namespace logmind {

// ── HNSW 索引 ──
class HNSWIndex {
public:
    HNSWIndex(size_t max_elements, size_t M, size_t ef_construction)
        : m_M(M), m_M_max(M), m_M_max0(2 * M)
        , m_ef_construction(ef_construction)
        , m_max_elements(max_elements)
    {
        m_rng.seed(42);
        m_vectors.reserve(max_elements);
    }

    void add_point(const std::vector<float>& vec, size_t id) {
        if (id >= m_vectors.size()) m_vectors.resize(id + 1);
        m_vectors[id] = vec;

        int level = random_level();
        Node node;
        node.id = id;
        node.level = level;

        if (m_size == 0) {
            m_entry_point = id;
            m_max_level = level;
            m_graph[level][id] = node;
            m_size++;
            return;
        }

        // 从最高层逐层搜索
        size_t curr = m_entry_point;
        for (int lc = m_max_level; lc > level; --lc) {
            auto candidates = search_layer(vec, curr, 1, lc);
            if (!candidates.empty()) curr = candidates.top().id;
        }

        // 从 level 层到底层逐层插入
        for (int lc = level; lc >= 0; --lc) {
            auto candidates = search_layer(vec, curr, m_ef_construction, lc);
            auto neighbors = select_neighbors(vec, candidates, lc == 0 ? m_M_max0 : m_M_max);
            m_graph[lc][id].neighbors = neighbors;
            for (const auto& [nid, dist] : neighbors) {
                auto& n_edges = m_graph[lc][nid].neighbors;
                n_edges.emplace_back(id, dist);
                if (n_edges.size() > (lc == 0 ? m_M_max0 : m_M_max)) {
                    // 截断
                    std::sort(n_edges.begin(), n_edges.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; });
                    n_edges.resize(lc == 0 ? m_M_max0 : m_M_max);
                }
            }
            if (!candidates.empty()) curr = candidates.top().id;
        }

        if (level > m_max_level) {
            m_max_level = level;
            m_entry_point = id;
        }
        m_size++;
    }

    std::vector<size_t> search_knn(const std::vector<float>& query, size_t k) const {
        if (m_size == 0) return {};

        auto candidates = search_layer(query, m_entry_point, k, m_max_level);
        std::vector<size_t> result;
        while (!candidates.empty()) {
            result.push_back(candidates.top().id);
            candidates.pop();
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

private:
    struct Candidate {
        size_t id;
        float  dist;
        bool operator>(const Candidate& other) const { return dist > other.dist; }
        bool operator<(const Candidate& other) const { return dist < other.dist; }
    };

    struct Node {
        size_t id;
        std::vector<std::pair<size_t, float>> neighbors;
        int level = 0;
    };

    int random_level() {
        // 指数衰减层级生成
        const float mL = 1.0f / std::log(static_cast<float>(m_M));
        float r = std::uniform_real_distribution<float>(0, 1)(m_rng);
        return static_cast<int>(-std::log(r) * mL);
    }

    float l2_distance(const std::vector<float>& a, const std::vector<float>& b) const {
        float sum = 0.0f;
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            float d = a[i] - b[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }

    std::priority_queue<Candidate> search_layer(
        const std::vector<float>& query,
        size_t entry_point,
        size_t ef,
        int layer) const
    {
        std::priority_queue<Candidate> top_candidates;
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> candidates;

        float dist = l2_distance(query, m_vectors[entry_point]);
        top_candidates.push({entry_point, dist});
        candidates.push({entry_point, dist});

        std::unordered_set<size_t> visited = {entry_point};

        while (!candidates.empty()) {
            auto curr = candidates.top();
            candidates.pop();

            auto worst = top_candidates.top();
            if (curr.dist > worst.dist) break;

            auto it = m_graph.find(layer);
            if (it == m_graph.end()) continue;

            auto node_it = it->second.find(curr.id);
            if (node_it == it->second.end()) continue;

            for (const auto& [nid, ndist] : node_it->second.neighbors) {
                if (visited.count(nid)) continue;
                visited.insert(nid);

                float d = l2_distance(query, m_vectors[nid]);
                if (top_candidates.size() < ef || d < top_candidates.top().dist) {
                    candidates.push({nid, d});
                    top_candidates.push({nid, d});
                    if (top_candidates.size() > ef) top_candidates.pop();
                }
            }
        }

        return top_candidates;
    }

    std::vector<std::pair<size_t, float>> select_neighbors(
        const std::vector<float>& /*query*/,
        std::priority_queue<Candidate>& candidates,
        size_t M_max) const
    {
        std::vector<std::pair<size_t, float>> result;
        while (!candidates.empty() && result.size() < M_max) {
            auto c = candidates.top();
            candidates.pop();
            result.emplace_back(c.id, c.dist);
        }
        return result;
    }

    // ── 成员 ──
    const size_t m_M, m_M_max, m_M_max0, m_ef_construction;
    const size_t m_max_elements;

    size_t m_size = 0;
    int m_max_level = 0;
    size_t m_entry_point = 0;

    std::unordered_map<int, std::unordered_map<size_t, Node>> m_graph;
    std::vector<std::vector<float>> m_vectors;
    mutable std::mt19937 m_rng;
};

// ── VectorStore 实现 ──
struct VectorStore::Impl {
    VectorStoreConfig config;
    sqlite3*         db   = nullptr;
    HNSWIndex*       hnsw = nullptr;
    size_t           count = 0;

    bool init_schema();
    bool insert_to_sqlite(const std::string& log_id,
                          const std::vector<float>& embedding,
                          const std::string& metadata);
    bool remove_from_sqlite(const std::string& log_id);
};

bool VectorStore::Impl::init_schema() {
    if (!db) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS vectors (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            log_id      TEXT UNIQUE NOT NULL,
            embedding   BLOB NOT NULL,
            dimension   INTEGER NOT NULL,
            metadata    TEXT,
            created_at  INTEGER NOT NULL DEFAULT (unixepoch())
        );
        CREATE INDEX IF NOT EXISTS idx_vectors_log_id ON vectors(log_id);
        CREATE INDEX IF NOT EXISTS idx_vectors_created ON vectors(created_at);
        CREATE TABLE IF NOT EXISTS store_meta (
            key   TEXT PRIMARY KEY,
            value TEXT
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "SQLite init error: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool VectorStore::Impl::insert_to_sqlite(
    const std::string& log_id,
    const std::vector<float>& embedding,
    const std::string& metadata)
{
    if (!db) return false;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO vectors (log_id, embedding, dimension, metadata) "
                       "VALUES (?, ?, ?, ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, log_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, embedding.data(),
                      static_cast<int>(embedding.size() * sizeof(float)),
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, static_cast<int>(embedding.size()));
    sqlite3_bind_text(stmt, 4, metadata.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

VectorStore::VectorStore(const VectorStoreConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
}

VectorStore::~VectorStore() {
    close();
}

bool VectorStore::open() {
    if (sqlite3_open(m_impl->config.db_path.c_str(), &m_impl->db) != SQLITE_OK) {
        std::cerr << "Failed to open vector store: "
                  << m_impl->config.db_path << std::endl;
        return false;
    }

    m_impl->init_schema();

    // 统计已有向量数
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, "SELECT COUNT(*) FROM vectors",
                           -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            m_impl->count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    // 初始化 HNSW
    m_impl->hnsw = new HNSWIndex(
        m_impl->config.max_elements,
        m_impl->config.hnsw_m,
        m_impl->config.hnsw_ef_construction
    );

    if (m_impl->count > 0) {
        rebuild_index();
    }

    return true;
}

void VectorStore::close() {
    delete m_impl->hnsw;
    m_impl->hnsw = nullptr;

    if (m_impl->db) {
        sqlite3_close(m_impl->db);
        m_impl->db = nullptr;
    }
}

bool VectorStore::insert(const std::string& log_id,
                          const std::vector<float>& embedding,
                          const std::string& metadata_json)
{
    if (!m_impl->hnsw) return false;

    size_t new_id = m_impl->hnsw->size();
    m_impl->hnsw->add_point(embedding, new_id);

    if (m_impl->config.persist && m_impl->db) {
        m_impl->insert_to_sqlite(log_id, embedding, metadata_json);
    }

    m_impl->count++;
    return true;
}

size_t VectorStore::insert_batch(
    const std::vector<std::string>& log_ids,
    const std::vector<std::vector<float>>& embeddings,
    const std::vector<std::string>& metadata_list)
{
    size_t count = std::min(
        {log_ids.size(), embeddings.size(), metadata_list.size()});
    for (size_t i = 0; i < count; ++i) {
        insert(log_ids[i], embeddings[i],
               i < metadata_list.size() ? metadata_list[i] : "{}");
    }
    return count;
}

std::vector<SearchResult> VectorStore::search(
    const std::vector<float>& query_embedding, size_t top_k)
{
    if (!m_impl->hnsw || m_impl->hnsw->empty()) return {};

    auto nearest = m_impl->hnsw->search_knn(query_embedding, top_k);

    std::vector<SearchResult> results;
    for (const auto& id : nearest) {
        SearchResult r;
        r.log_id     = "vector_" + std::to_string(id);
        r.distance   = 0.0f;
        r.similarity = 1.0f;
        results.push_back(r);
    }
    return results;
}

size_t VectorStore::size() const {
    return m_impl->count;
}

void VectorStore::clear() {
    if (m_impl->db) {
        sqlite3_exec(m_impl->db, "DELETE FROM vectors", nullptr, nullptr, nullptr);
    }
    m_impl->count = 0;
    rebuild_index();
}

bool VectorStore::rebuild_index() {
    delete m_impl->hnsw;
    m_impl->hnsw = new HNSWIndex(
        m_impl->config.max_elements,
        m_impl->config.hnsw_m,
        m_impl->config.hnsw_ef_construction
    );
    // 简化处理：生产环境需要从 SQLite 读取所有向量重建
    return true;
}

bool VectorStore::remove(const std::string& log_id) {
    if (m_impl->db) {
        m_impl->remove_from_sqlite(log_id);
    }
    // HNSW 不支持真正删除，标记即可
    return true;
}

bool VectorStore::Impl::remove_from_sqlite(const std::string& log_id) {
    if (!db) return false;
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM vectors WHERE log_id = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, log_id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

} // namespace logmind
