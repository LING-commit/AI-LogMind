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
    std::string plugins_dir() const;
    std::string db_path() const;
    int         log_buffer_capacity() const;
    int         worker_threads() const;
    int         ai_timeout_seconds() const;
    bool        persist_logs() const;

    // ── 获取嵌套配置 ──
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
