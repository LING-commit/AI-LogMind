#include <logmind/config_mgr.h>
#include <fstream>
#include <iostream>

namespace logmind {

bool ConfigMgr::load(const std::string& path) {
    m_path = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        m_last_error = "Cannot open config file: " + path;
        m_loaded = false;
        return false;
    }

    try {
        file >> m_data;
        m_loaded = true;
        return true;
    } catch (const std::exception& e) {
        m_last_error = "JSON parse error: ";
        m_last_error += e.what();
        m_loaded = false;
        return false;
    }
}

bool ConfigMgr::reload() {
    return load(m_path);
}

void ConfigMgr::set_defaults() {
    m_data = nlohmann::json{
        {"plugins_dir",     "/usr/lib/logmind/plugins"},
        // 采集目标。插件通过 watch_patterns() 声明的优先，都没声明时用这里。
        {"log_dirs",        nlohmann::json::array({"/var/log"})},
        {"watch_patterns",  nlohmann::json::array({"*.log"})},
        {"log_buffer", {
            {"capacity",    50000}
        }},
        // 每个插件的私有配置，键名就是插件的 manifest().name
        {"plugins", {
            {"threshold-analyzer", {
                {"window_seconds",  60},
                {"error_threshold", 5},
                {"drop_debug",      false}
            }},
            {"stdout-alert", {
                {"path",                  "-"},
                {"max_entries_per_alert", 3}
            }}
        }},
        // 告警触发规则，由流水线统一执行（插件只负责投递）
        {"alerts", {
            {"min_level",             "ERROR"},
            {"dedup_window_seconds",  60},
            {"max_alerts_per_minute", 60},
            {"dedup_cache_size",      4096}
        }},
        {"vector_store", {
            {"db_path",                 "/var/lib/logmind/vectors.db"},
            {"dim",                     0},
            {"hnsw_m",                  16},
            {"hnsw_ef_construction",    200},
            {"hnsw_ef_search",          64},
            {"max_elements",            1000000},
            {"auto_trim_threshold",     0},
            {"persist",                 true}
        }},
        // >1 时流水线转为异步：采集线程只投递，处理交给 worker
        {"worker_threads",  4}
    };
    m_loaded = true;
}

std::string ConfigMgr::plugins_dir() const {
    return m_data.value("plugins_dir", "/usr/lib/logmind/plugins");
}

int ConfigMgr::log_buffer_capacity() const {
    auto buf = m_data.value("log_buffer", nlohmann::json::object());
    return buf.value("capacity", 50000);
}

int ConfigMgr::worker_threads() const {
    return m_data.value("worker_threads", 4);
}

nlohmann::json ConfigMgr::plugin_config(const std::string& plugin_name) const {
    auto plugins = m_data.value("plugins", nlohmann::json::object());
    return plugins.value(plugin_name, nlohmann::json::object());
}

} // namespace logmind
