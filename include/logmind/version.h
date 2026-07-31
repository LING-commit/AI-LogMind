#pragma once
#include <cstdint>

namespace logmind {

// ABI 版本。凡是改动 IPlugin/ICollector/IAnalyzer/IAlert 的对象布局或虚表，
// 都必须递增，否则旧 .so 会以错误的布局被加载。
//   v2: IPlugin 增加了 manifest 缓存成员（原先是共享的函数内 static）；
//       ICollector 增加了 parse_line()
// 类型用 int 而非 uint32_t，好让插件里直接写 `return LOGMIND_ABI_VERSION;`
constexpr int LOGMIND_ABI_VERSION = 2;

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
