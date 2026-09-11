#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace logmind {

class ConfigMgr {
public:
    bool load(const std::string& path);
    bool reload();
    void set_defaults();

    const std::string& path() const { return m_path; }
    const nlohmann::json& data() const { return m_data; }

    // ── 便捷访问 ──
    // 只保留真正有调用点的。原先还有 db_path()/persist_logs()/ai_timeout_seconds()，
    // 它们读的是 storage.* / ai_analyzer.* 这些完全没实现的配置段，全部零调用点，
    // 留着只会让人误以为对应功能存在。
    std::string plugins_dir() const;
    int         log_buffer_capacity() const;
    int         worker_threads() const;

    // ── 获取插件私有配置（config.json 里的 plugins.<插件名>）──
    nlohmann::json plugin_config(const std::string& plugin_name) const;

    std::string last_error() const { return m_last_error; }
    bool        is_loaded()  const { return m_loaded; }

private:
    std::string m_path;
    nlohmann::json m_data;
    std::string m_last_error;
    bool m_loaded = false;
};

} // namespace logmind
