#pragma once
#include <cstdint>

namespace logmind {

constexpr uint32_t LOGMIND_ABI_VERSION = 1;

enum class DaemonState : uint8_t {
    INIT,
    LOADING,
    STARTING,
    RUNNING,
    RELOADING,
    STOPPING,
    STOPPED,
    FAILED
};

inline const char* to_string(DaemonState s) {
    switch (s) {
        case DaemonState::INIT:      return "INIT";
        case DaemonState::LOADING:   return "LOADING";
        case DaemonState::STARTING:  return "STARTING";
        case DaemonState::RUNNING:   return "RUNNING";
        case DaemonState::RELOADING: return "RELOADING";
        case DaemonState::STOPPING:  return "STOPPING";
        case DaemonState::STOPPED:   return "STOPPED";
        case DaemonState::FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

} // namespace logmind
