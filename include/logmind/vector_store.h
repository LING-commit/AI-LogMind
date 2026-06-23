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
    size_t      max_elements            = 1000000;
    bool        persist                 = true;
    size_t      auto_trim_threshold     = 0;
};

struct SearchResult {
    std::string log_id;
    std::string metadata_json;
    float       distance    = 0.0f;
    float       similarity  = 0.0f;
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
