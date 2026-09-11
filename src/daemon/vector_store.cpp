#include <logmind/vector_store.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <unordered_map>
#include <vector>

namespace logmind {

// ── HNSW 索引 ──
//
// 按 Malkov & Yashunin 的原始算法实现：
//   插入 —— 从顶层贪心下降到目标层，再自目标层向下逐层做 ef_construction 搜索并双向连边；
//   检索 —— 先逐层贪心下降到第 1 层，再在第 0 层做 ef 扩展搜索。
//
// 旧实现只在最高层调了一次 search_layer 就返回。最高层通常只有个位数节点，
// 所以召回率接近随机——"1ms 检索"快是因为几乎什么都没搜。
class HNSWIndex {
public:
    HNSWIndex(size_t dim, size_t max_elements, size_t M, size_t ef_construction)
        : m_dim(dim)
        , m_max_elements(max_elements)
        , m_M(std::max<size_t>(M, 2))
        , m_M_max0(2 * std::max<size_t>(M, 2))
        , m_ef_construction(std::max(ef_construction, std::max<size_t>(M, 2)))
        , m_level_mult(1.0 / std::log(static_cast<double>(std::max<size_t>(M, 2))))
    {
        m_rng.seed(42);   // 固定种子：同样的插入序列得到可复现的索引
        m_data.reserve(std::min<size_t>(max_elements, 1024) * dim);
    }

    void add_point(uint32_t id, const float* vec) {
        ensure_capacity(id);
        std::copy(vec, vec + m_dim,
                  m_data.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(id) * m_dim));
        m_deleted[id] = 0;

        const int level = random_level();
        m_levels[id] = level;
        m_links[id].assign(static_cast<size_t>(level) + 1, {});

        if (m_count == 0) {
            m_entry     = id;
            m_max_level = level;
            m_count     = 1;
            return;
        }

        const float* q = &m_data[static_cast<size_t>(id) * m_dim];

        // 1) 顶层 → level+1：每层只做贪心下降，保留最近的一个点
        uint32_t ep = m_entry;
        for (int lc = m_max_level; lc > level; --lc) {
            ep = greedy_descend(q, ep, lc);
        }

        // 2) min(level, max_level) → 0：每层搜索 + 双向连边
        for (int lc = std::min(level, m_max_level); lc >= 0; --lc) {
            auto candidates = search_layer(q, ep, m_ef_construction, lc, id);
            auto neighbors  = take_nearest(candidates, m_M);
            const size_t cap = (lc == 0) ? m_M_max0 : m_M;

            m_links[id][static_cast<size_t>(lc)] = neighbors;
            for (const uint32_t nb : neighbors) {
                auto& back = m_links[nb][static_cast<size_t>(lc)];
                back.push_back(id);
                if (back.size() > cap) shrink_links(nb, back, cap);
            }
            if (!candidates.empty()) ep = candidates.front().second;
        }

        if (level > m_max_level) {
            m_max_level = level;
            m_entry     = id;
        }
        ++m_count;
    }

    // 返回 (L2 距离, id)，按距离升序，已剔除墓碑节点
    std::vector<std::pair<float, uint32_t>>
    search_knn(const float* query, size_t k, size_t ef) const {
        if (m_count == 0 || k == 0) return {};

        uint32_t ep = m_entry;
        for (int lc = m_max_level; lc > 0; --lc) {
            ep = greedy_descend(query, ep, lc);
        }

        // 墓碑节点仍要参与图遍历（它们是连通性的一部分），只是不进结果，
        // 所以候选池要比 k 开得大一些。
        const size_t pool = std::max(k, ef);
        auto candidates = search_layer(query, ep, pool, 0, kNoSkip);

        std::vector<std::pair<float, uint32_t>> result;
        result.reserve(std::min(k, candidates.size()));
        for (const auto& [dist_sq, id] : candidates) {
            if (m_deleted[id]) continue;
            result.emplace_back(std::sqrt(dist_sq), id);   // 内部按平方距离排序，出口才开方
            if (result.size() >= k) break;
        }
        return result;
    }

    void mark_deleted(uint32_t id) {
        if (id < m_deleted.size() && !m_deleted[id]) {
            m_deleted[id] = 1;
            ++m_deleted_count;
        }
    }

    size_t size()      const { return m_count - m_deleted_count; }
    size_t capacity()  const { return m_max_elements; }
    bool   empty()     const { return size() == 0; }

private:
    using Cand = std::pair<float, uint32_t>;   // (平方距离, id)
    static constexpr uint32_t kNoSkip = 0xFFFFFFFFu;

    int random_level() {
        double r = std::uniform_real_distribution<double>(0.0, 1.0)(m_rng);
        if (r <= 0.0) r = std::numeric_limits<double>::min();
        return static_cast<int>(-std::log(r) * m_level_mult);
    }

    float distance_sq(const float* a, uint32_t id) const {
        const float* b = &m_data[static_cast<size_t>(id) * m_dim];
        float sum = 0.0f;
        // 连续内存 + 定长循环，GCC 在 -O2 下可自动向量化
        for (size_t i = 0; i < m_dim; ++i) {
            const float d = a[i] - b[i];
            sum += d * d;
        }
        return sum;
    }

    uint32_t greedy_descend(const float* q, uint32_t ep, int layer) const {
        float best = distance_sq(q, ep);
        bool improved = true;
        while (improved) {
            improved = false;
            for (const uint32_t nb : m_links[ep][static_cast<size_t>(layer)]) {
                const float d = distance_sq(q, nb);
                if (d < best) {
                    best     = d;
                    ep       = nb;
                    improved = true;
                }
            }
        }
        return ep;
    }

    // 返回按平方距离升序排好的候选，最多 ef 个。skip_id 用于插入时排除自身。
    std::vector<Cand> search_layer(const float* q, uint32_t ep, size_t ef,
                                   int layer, uint32_t skip_id) const {
        const uint32_t stamp = next_visit_stamp();

        std::priority_queue<Cand, std::vector<Cand>, std::greater<Cand>> to_visit;  // 最近优先
        std::priority_queue<Cand> found;                                            // 最远在堆顶

        const float d0 = distance_sq(q, ep);
        to_visit.emplace(d0, ep);
        found.emplace(d0, ep);
        m_visited[ep] = stamp;

        while (!to_visit.empty()) {
            const Cand cur = to_visit.top();
            if (cur.first > found.top().first) break;   // 最近的未访问点也比已知最远点还远
            to_visit.pop();

            for (const uint32_t nb : m_links[cur.second][static_cast<size_t>(layer)]) {
                if (nb == skip_id || m_visited[nb] == stamp) continue;
                m_visited[nb] = stamp;

                const float d = distance_sq(q, nb);
                if (found.size() < ef || d < found.top().first) {
                    to_visit.emplace(d, nb);
                    found.emplace(d, nb);
                    if (found.size() > ef) found.pop();
                }
            }
        }

        std::vector<Cand> result;
        result.reserve(found.size());
        while (!found.empty()) {
            result.push_back(found.top());
            found.pop();
        }
        std::reverse(result.begin(), result.end());   // 出堆是降序，翻成升序
        return result;
    }

    static std::vector<uint32_t> take_nearest(const std::vector<Cand>& candidates, size_t m) {
        std::vector<uint32_t> out;
        out.reserve(std::min(m, candidates.size()));
        for (const auto& c : candidates) {
            out.push_back(c.second);
            if (out.size() >= m) break;
        }
        return out;
    }

    void shrink_links(uint32_t owner, std::vector<uint32_t>& links, size_t cap) const {
        const float* base = &m_data[static_cast<size_t>(owner) * m_dim];

        // 距离先算一遍存起来再排序。放在比较器里现算的话，partial_sort 的
        // 每次比较都要跑两遍 m_dim 维循环，是建索引阶段最大的一块开销。
        m_shrink_scratch.clear();
        m_shrink_scratch.reserve(links.size());
        for (const uint32_t id : links) {
            m_shrink_scratch.emplace_back(distance_sq(base, id), id);
        }
        std::partial_sort(m_shrink_scratch.begin(),
                          m_shrink_scratch.begin() + static_cast<std::ptrdiff_t>(cap),
                          m_shrink_scratch.end());

        links.resize(cap);
        for (size_t i = 0; i < cap; ++i) links[i] = m_shrink_scratch[i].second;
    }

    void ensure_capacity(uint32_t id) {
        const size_t need = static_cast<size_t>(id) + 1;
        if (need <= m_levels.size()) return;
        m_data.resize(need * m_dim, 0.0f);
        m_links.resize(need);
        m_levels.resize(need, 0);
        m_deleted.resize(need, 0);
        m_visited.resize(need, 0);
    }

    // 用「访问代号」代替每次查询新建一个 unordered_set
    uint32_t next_visit_stamp() const {
        if (++m_visit_counter == 0) {
            std::fill(m_visited.begin(), m_visited.end(), 0u);
            m_visit_counter = 1;
        }
        return m_visit_counter;
    }

    // ── 成员（声明顺序即初始化顺序）──
    const size_t m_dim;
    const size_t m_max_elements;
    const size_t m_M;
    const size_t m_M_max0;
    const size_t m_ef_construction;
    const double m_level_mult;

    size_t   m_count         = 0;
    size_t   m_deleted_count = 0;
    int      m_max_level     = 0;
    uint32_t m_entry         = 0;

    std::vector<float>                     m_data;     // 连续存储，stride = m_dim
    std::vector<std::vector<std::vector<uint32_t>>> m_links;   // [id][layer] → 邻居
    std::vector<int>                       m_levels;
    std::vector<uint8_t>                   m_deleted;

    mutable std::vector<uint32_t> m_visited;
    mutable uint32_t              m_visit_counter = 0;
    mutable std::vector<Cand>     m_shrink_scratch;   // shrink_links 复用的缓冲
    std::mt19937                  m_rng;
};

// ── VectorStore 实现 ──
struct VectorStore::Impl {
    VectorStoreConfig config;

    mutable std::mutex mutex;
    sqlite3*      db          = nullptr;
    sqlite3_stmt* insert_stmt = nullptr;

    std::unique_ptr<HNSWIndex> index;
    size_t dim = 0;

    // HNSW 内部下标 ↔ log_id 的映射。旧实现完全没有这层映射，
    // 所以 search() 只能编出 "vector_<下标>" 这样的假结果。
    std::vector<std::string>                  id_to_log;    // 空串 = 墓碑
    std::vector<std::string>                  id_to_meta;
    std::unordered_map<std::string, uint32_t> log_to_id;
    size_t trim_cursor = 0;

    bool exec(const char* sql);
    bool init_schema();
    bool prepare_statements();
    bool write_row(const std::string& log_id,
                   const std::vector<float>& embedding,
                   const std::string& metadata);
    bool delete_row(const std::string& log_id);
    bool load_all();
    void reset_index();
    void tombstone(uint32_t id);
    void trim();
    bool insert_locked(const std::string& log_id,
                       const std::vector<float>& embedding,
                       const std::string& metadata,
                       bool write_db);
};

bool VectorStore::Impl::exec(const char* sql) {
    if (!db) return false;
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "VectorStore: SQL error (" << sql << "): "
                  << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool VectorStore::Impl::init_schema() {
    return exec(R"(
        CREATE TABLE IF NOT EXISTS vectors (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            log_id      TEXT UNIQUE NOT NULL,
            embedding   BLOB NOT NULL,
            dimension   INTEGER NOT NULL,
            metadata    TEXT,
            created_at  INTEGER NOT NULL DEFAULT (unixepoch())
        );
        CREATE INDEX IF NOT EXISTS idx_vectors_created ON vectors(created_at);
        CREATE TABLE IF NOT EXISTS store_meta (
            key   TEXT PRIMARY KEY,
            value TEXT
        );
    )");
    // log_id 上的 UNIQUE 约束已经隐含唯一索引，原先额外建的 idx_vectors_log_id 是冗余的
}

bool VectorStore::Impl::prepare_statements() {
    // 预编译一次反复复用，避免每行一次 sqlite3_prepare_v2
    const char* sql =
        "INSERT OR REPLACE INTO vectors (log_id, embedding, dimension, metadata) "
        "VALUES (?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &insert_stmt, nullptr) != SQLITE_OK) {
        std::cerr << "VectorStore: prepare failed: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool VectorStore::Impl::write_row(const std::string& log_id,
                                  const std::vector<float>& embedding,
                                  const std::string& metadata) {
    if (!insert_stmt) return false;

    sqlite3_reset(insert_stmt);
    sqlite3_clear_bindings(insert_stmt);

    // SQLITE_STATIC 而非 TRANSIENT：调用方的缓冲区在 step 返回前都有效，省掉三次拷贝
    sqlite3_bind_text(insert_stmt, 1, log_id.c_str(),
                      static_cast<int>(log_id.size()), SQLITE_STATIC);
    sqlite3_bind_blob(insert_stmt, 2, embedding.data(),
                      static_cast<int>(embedding.size() * sizeof(float)), SQLITE_STATIC);
    sqlite3_bind_int(insert_stmt, 3, static_cast<int>(embedding.size()));
    sqlite3_bind_text(insert_stmt, 4, metadata.c_str(),
                      static_cast<int>(metadata.size()), SQLITE_STATIC);

    const bool ok = (sqlite3_step(insert_stmt) == SQLITE_DONE);
    if (!ok) {
        std::cerr << "VectorStore: insert failed for '" << log_id
                  << "': " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_reset(insert_stmt);
    return ok;
}

bool VectorStore::Impl::delete_row(const std::string& log_id) {
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM vectors WHERE log_id = ?",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, log_id.c_str(), static_cast<int>(log_id.size()), SQLITE_STATIC);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

void VectorStore::Impl::reset_index() {
    id_to_log.clear();
    id_to_meta.clear();
    log_to_id.clear();
    trim_cursor = 0;
    index = (dim > 0)
        ? std::make_unique<HNSWIndex>(dim, config.max_elements,
                                      config.hnsw_m, config.hnsw_ef_construction)
        : nullptr;
}

// 真正从 SQLite 把向量读回内存重建索引。
// 旧实现只是 delete 掉旧索引再 new 一个空的（注释写着「简化处理」），
// 于是 persist=true 的向量重启后永远加载不回来。
bool VectorStore::Impl::load_all() {
    reset_index();
    if (!db) return true;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT log_id, embedding, dimension, metadata FROM vectors ORDER BY id";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "VectorStore: cannot read vectors: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    size_t loaded = 0;
    size_t skipped = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* log_id_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const void* blob     = sqlite3_column_blob(stmt, 1);
        const int   nbytes   = sqlite3_column_bytes(stmt, 1);
        const int   row_dim  = sqlite3_column_int(stmt, 2);
        const auto* meta_c   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        if (!log_id_c || !blob || row_dim <= 0 ||
            nbytes != row_dim * static_cast<int>(sizeof(float))) {
            ++skipped;
            continue;
        }

        std::vector<float> embedding(static_cast<size_t>(row_dim));
        std::memcpy(embedding.data(), blob, static_cast<size_t>(nbytes));

        if (insert_locked(log_id_c, embedding, meta_c ? meta_c : "{}", /*write_db=*/false)) {
            ++loaded;
        } else {
            ++skipped;
        }
    }
    sqlite3_finalize(stmt);

    if (loaded > 0 || skipped > 0) {
        std::cout << "VectorStore: loaded " << loaded << " vectors";
        if (skipped > 0) std::cout << " (" << skipped << " skipped)";
        std::cout << std::endl;
    }
    return true;
}

void VectorStore::Impl::tombstone(uint32_t id) {
    if (!index || id >= id_to_log.size()) return;
    index->mark_deleted(id);
    id_to_log[id].clear();
    id_to_meta[id].clear();
    id_to_meta[id].shrink_to_fit();
}

void VectorStore::Impl::trim() {
    if (config.auto_trim_threshold == 0) return;
    if (log_to_id.size() <= config.auto_trim_threshold) return;

    size_t excess = log_to_id.size() - config.auto_trim_threshold;
    // id 是递增分配的，所以下标越小越旧。cursor 只前进，避免每次都从头扫墓碑。
    while (excess > 0 && trim_cursor < id_to_log.size()) {
        const uint32_t id = static_cast<uint32_t>(trim_cursor++);
        if (id_to_log[id].empty()) continue;

        const std::string victim = id_to_log[id];
        tombstone(id);
        log_to_id.erase(victim);
        if (db) delete_row(victim);
        --excess;
    }
}

bool VectorStore::Impl::insert_locked(const std::string& log_id,
                                      const std::vector<float>& embedding,
                                      const std::string& metadata,
                                      bool write_db) {
    if (log_id.empty()) {
        std::cerr << "VectorStore: empty log_id rejected" << std::endl;
        return false;
    }
    if (embedding.empty()) return false;

    if (dim == 0) {
        dim = embedding.size();
        index = std::make_unique<HNSWIndex>(dim, config.max_elements,
                                            config.hnsw_m, config.hnsw_ef_construction);
    }
    if (embedding.size() != dim) {
        std::cerr << "VectorStore: dimension mismatch for '" << log_id << "': got "
                  << embedding.size() << ", index is " << dim << std::endl;
        return false;
    }
    if (id_to_log.size() >= config.max_elements) {
        std::cerr << "VectorStore: max_elements (" << config.max_elements
                  << ") reached, rejecting '" << log_id << "'" << std::endl;
        return false;
    }

    // 同一个 log_id 重复插入：HNSW 无法原地更新，给旧点打墓碑再插新点
    auto existing = log_to_id.find(log_id);
    if (existing != log_to_id.end()) {
        tombstone(existing->second);
        log_to_id.erase(existing);
    }

    const auto new_id = static_cast<uint32_t>(id_to_log.size());
    index->add_point(new_id, embedding.data());
    id_to_log.push_back(log_id);
    id_to_meta.push_back(metadata);
    log_to_id.emplace(log_id, new_id);

    if (write_db && config.persist && db && !write_row(log_id, embedding, metadata)) {
        return false;
    }
    trim();
    return true;
}

// ── 公开接口 ──
VectorStore::VectorStore(const VectorStoreConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
}

VectorStore::~VectorStore() {
    close();
}

bool VectorStore::open() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& im = *m_impl;

    if (im.db) return true;

    if (sqlite3_open(im.config.db_path.c_str(), &im.db) != SQLITE_OK) {
        std::cerr << "VectorStore: failed to open " << im.config.db_path << ": "
                  << (im.db ? sqlite3_errmsg(im.db) : "out of memory") << std::endl;
        sqlite3_close(im.db);   // open 失败也会返回句柄，必须关掉
        im.db = nullptr;
        return false;
    }

    // WAL + NORMAL：批量写入时不用每行一次 fsync
    im.exec("PRAGMA journal_mode=WAL");
    im.exec("PRAGMA synchronous=NORMAL");

    if (!im.init_schema() || !im.prepare_statements()) {
        if (im.insert_stmt) { sqlite3_finalize(im.insert_stmt); im.insert_stmt = nullptr; }
        sqlite3_close(im.db);
        im.db = nullptr;
        return false;
    }

    im.dim = im.config.dim;
    return im.load_all();
}

void VectorStore::close() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& im = *m_impl;

    if (im.insert_stmt) {
        sqlite3_finalize(im.insert_stmt);
        im.insert_stmt = nullptr;
    }
    if (im.db) {
        sqlite3_close(im.db);
        im.db = nullptr;
    }
    im.index.reset();
    im.id_to_log.clear();
    im.id_to_meta.clear();
    im.log_to_id.clear();
    im.trim_cursor = 0;
}

bool VectorStore::insert(const std::string& log_id,
                         const std::vector<float>& embedding,
                         const std::string& metadata_json) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->insert_locked(log_id, embedding, metadata_json, /*write_db=*/true);
}

size_t VectorStore::insert_batch(const std::vector<std::string>& log_ids,
                                 const std::vector<std::vector<float>>& embeddings,
                                 const std::vector<std::string>& metadata_list) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& im = *m_impl;

    const size_t n = std::min({log_ids.size(), embeddings.size(), metadata_list.size()});
    if (n == 0) return 0;

    // 整批包一个事务。旧实现逐条 insert，每条都是一次隐式事务 + 一次 fsync。
    const bool in_tx = im.config.persist && im.db && im.exec("BEGIN IMMEDIATE");

    size_t ok = 0;
    for (size_t i = 0; i < n; ++i) {
        if (im.insert_locked(log_ids[i], embeddings[i], metadata_list[i], true)) ++ok;
    }

    if (in_tx && !im.exec("COMMIT")) im.exec("ROLLBACK");
    return ok;
}

std::vector<SearchResult> VectorStore::search(const std::vector<float>& query_embedding,
                                              size_t top_k) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& im = *m_impl;

    if (!im.index || im.index->empty() || query_embedding.empty() || top_k == 0) return {};
    if (query_embedding.size() != im.dim) {
        std::cerr << "VectorStore: query dimension " << query_embedding.size()
                  << " != index dimension " << im.dim << std::endl;
        return {};
    }

    const auto hits = im.index->search_knn(query_embedding.data(), top_k,
                                           im.config.hnsw_ef_search);

    std::vector<SearchResult> results;
    results.reserve(hits.size());
    for (const auto& [distance, id] : hits) {
        SearchResult r;
        r.log_id        = im.id_to_log[id];
        r.metadata_json = im.id_to_meta[id];
        r.distance      = distance;
        r.similarity    = 1.0f / (1.0f + distance);
        results.push_back(std::move(r));
    }
    return results;
}

size_t VectorStore::size() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->log_to_id.size();
}

void VectorStore::clear() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->db) m_impl->exec("DELETE FROM vectors");
    m_impl->reset_index();
}

bool VectorStore::rebuild_index() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    // 重建也是回收墓碑占用空间的唯一途径
    return m_impl->load_all();
}

bool VectorStore::remove(const std::string& log_id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& im = *m_impl;

    bool found = false;
    auto it = im.log_to_id.find(log_id);
    if (it != im.log_to_id.end()) {
        // HNSW 无法从图里真正摘除节点：打墓碑，仍参与遍历但不进结果。
        // 空间要靠 rebuild_index() 回收。
        im.tombstone(it->second);
        im.log_to_id.erase(it);
        found = true;
    }
    if (im.db && im.delete_row(log_id)) found = true;
    return found;
}

} // namespace logmind
