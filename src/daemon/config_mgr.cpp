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
        {"log_buffer", {
            {"capacity",    50000},
            {"enable_search_index", true}
        }},
        {"storage", {
            {"persist_logs",            false},
            {"db_path",                 "/var/lib/logmind/logs.db"},
            {"retention_days",          30},
            {"max_db_size_mb",          1024},
            {"auto_cleanup",            true},
            {"cleanup_interval_hours",  6}
        }},
        {"vector_store", {
            {"db_path",                 "/var/lib/logmind/vectors.db"},
            {"hnsw_m",                  16},
            {"hnsw_ef_construction",    200},
            {"max_elements",            1000000},
            {"persist",                 true}
        }},
        {"worker_threads",  4},
        {"ai_analyzer", {
            {"timeout_seconds", 30},
            {"max_retries",     2},
            {"temperature",     0.1}
        }},
        {"disk_monitor", {
            {"enabled",                 true},
            {"watch_path",              "/var/lib/logmind"},
            {"check_interval_seconds",  60},
            {"warning_threshold_pct",    85},
            {"critical_threshold_pct",   95}
        }}
    };
    m_loaded = true;
}

std::string ConfigMgr::plugins_dir() const {
    return m_data.value("plugins_dir", "/usr/lib/logmind/plugins");
}

std::string ConfigMgr::db_path() const {
    auto storage = m_data.value("storage", nlohmann::json::object());
    return storage.value("db_path", "/var/lib/logmind/logs.db");
}

int ConfigMgr::log_buffer_capacity() const {
    auto buf = m_data.value("log_buffer", nlohmann::json::object());
    return buf.value("capacity", 50000);
}

int ConfigMgr::worker_threads() const {
    return m_data.value("worker_threads", 4);
}

int ConfigMgr::ai_timeout_seconds() const {
    auto ai = m_data.value("ai_analyzer", nlohmann::json::object());
    return ai.value("timeout_seconds", 30);
}

bool ConfigMgr::persist_logs() const {
    auto storage = m_data.value("storage", nlohmann::json::object());
    return storage.value("persist_logs", false);
}

nlohmann::json ConfigMgr::plugin_config(const std::string& plugin_name) const {
    auto plugins = m_data.value("plugins", nlohmann::json::object());
    return plugins.value(plugin_name, nlohmann::json::object());
}

} // namespace logmind
