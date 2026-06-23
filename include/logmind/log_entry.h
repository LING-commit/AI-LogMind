#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace logmind {

enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

inline const char* to_string(LogLevel l) {
    switch (l) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

inline LogLevel log_level_from_string(const std::string& s) {
    if (s == "DEBUG") return LogLevel::DEBUG;
    if (s == "INFO")  return LogLevel::INFO;
    if (s == "WARN")  return LogLevel::WARN;
    if (s == "ERROR") return LogLevel::ERROR;
    if (s == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO;
}

struct LogEntry {
    std::string                     source;
    std::string                     raw;
    std::chrono::system_clock::time_point timestamp;
    LogLevel                        level = LogLevel::INFO;
    std::string                     message;
    std::string                     module;
    nlohmann::json                  fields;
    nlohmann::json                  tags;
    nlohmann::json                  meta;
};

enum class PluginType : uint8_t {
    Collector = 0,
    Analyzer,
    Alert
};

inline const char* to_string(PluginType t) {
    switch (t) {
        case PluginType::Collector: return "Collector";
        case PluginType::Analyzer:  return "Analyzer";
        case PluginType::Alert:     return "Alert";
    }
    return "Unknown";
}

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    PluginType  type;
    bool        loaded   = false;
    bool        enabled  = true;
    std::string so_path;
    std::string error;
};

struct PluginManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    PluginType  type;
    std::string license;
    bool        has_side_effects = false;
};

struct CreateContext {
    nlohmann::json config;
};

} // namespace logmind
