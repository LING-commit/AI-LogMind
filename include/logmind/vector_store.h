#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace logmind {

struct VectorStoreConfig {
    std::string db_path                 = "/var/lib/logmind/vectors.db";
    size_t      hnsw_ef_construction    = 200;
    size_t      hnsw_m                  = 16;
    // 检索时的候选池大小。越大召回越高、越慢；至少要 >= top_k。
    size_t      hnsw_ef_search          = 64;
    // 向量维度。0 表示由第一条插入的向量决定，之后所有向量必须一致。
    size_t      dim                     = 0;
    size_t      max_elements            = 1000000;
    bool        persist                 = true;
    // 非 0 时，存活条目超过该阈值就按插入顺序淘汰最旧的。
    size_t      auto_trim_threshold     = 0;
};

struct SearchResult {
    std::string log_id;
    std::string metadata_json;
    float       distance    = 0.0f;   // L2 距离
    float       similarity  = 0.0f;   // 1 / (1 + distance)，越接近 1 越相似
};

class VectorStore {
public:
    explicit VectorStore(const VectorStoreConfig& config);
    ~VectorStore();

    VectorStore(const VectorStore&) = delete;
    VectorStore& operator=(const VectorStore&) = delete;

    bool open();
    void close();

    bool insert(const std::string&   log_id,
                const std::vector<float>& embedding,
                const std::string&   metadata_json);

    size_t insert_batch(const std::vector<std::string>& log_ids,
                        const std::vector<std::vector<float>>& embeddings,
                        const std::vector<std::string>& metadata_list);

    std::vector<SearchResult> search(const std::vector<float>& query_embedding,
                                     size_t top_k = 10);

    size_t size() const;
    void clear();
    bool rebuild_index();
    bool remove(const std::string& log_id);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace logmind
