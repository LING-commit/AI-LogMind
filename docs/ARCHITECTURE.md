# LogMind 智能日志分析平台 - 详细技术方案

## 1. 目录结构

```
logmind/
├── CMakeLists.txt                    # 根 CMakeLists
├── cmake/
│   ├── PluginScan.cmake             # 自动扫描 & 注册插件子目录
│   └── CompilerSettings.cmake       # 统一编译器选项 (C++17, warnings)
├── include/
│   └── logmind/
│       ├── plugin.h                 # IPlugin 基础接口
│       ├── collector.h              # ICollector 接口
│       ├── analyzer.h               # IAnalyzer 接口
│       ├── alert.h                  # IAlert 接口
│       ├── log_entry.h              # LogEntry / PluginInfo 数据结构
│       ├── plugin_loader.h          # PluginLoader 声明
│       ├── pipeline.h               # Pipeline (采集→分析→告警管线)
│       └── dbus_service.h           # D-Bus 服务适配器
├── src/
│   ├── daemon/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp                 # daemon 入口
│   │   ├── plugin_loader.cpp        # 插件加载器实现
│   │   ├── pipeline.cpp             # 管线编排
│   │   └── dbus_service.cpp         # D-Bus 服务端实现
│   └── client/
│       ├── CMakeLists.txt
│       ├── main.cpp                 # Qt 面板入口
│       ├── models/
│       │   ├── log_model.h / .cpp   # QAbstractListModel 日志模型
│       │   └── plugin_model.h / .cpp# QAbstractListModel 插件模型
│       └── widgets/
│           ├── main_window.h / .cpp # 主窗口
│           ├── log_view.h / .cpp    # 日志流视图
│           └── style.qss            # QSS 样式
├── plugins/
│   ├── collectors/                  # 采集器插件 (.so)
│   │   ├── CMakeLists.txt
│   │   ├── parser_nginx/           # Nginx 日志解析器
│   │   ├── parser_syslog/          # Syslog 解析器
│   │   ├── parser_mysql/           # MySQL 慢查询解析器
│   │   └── parser_docker/          # Docker 日志解析器
│   ├── analyzers/                   # 分析器插件 (.so)
│   │   ├── CMakeLists.txt
│   │   ├── anomaly_det/            # 基于规则的异常检测
│   │   ├── pattern_match/          # 正则/模式匹配
│   │   ├── ai_analyzer/            # LLM 语义分析
│   │   └── stat_analyzer/          # 统计聚合 (QPS, P99)
│   └── alerts/                      # 告警器插件 (.so)
│       ├── CMakeLists.txt
│       ├── email_alert/            # 邮件告警
│       ├── feishu_alert/           # 飞书机器人
│       ├── dbus_notify/            # 桌面通知
│       └── webhook_alert/          # 通用 Webhook
├── tests/
│   ├── CMakeLists.txt
│   ├── test_plugin_loader.cpp
│   ├── test_pipeline.cpp
│   └── test_parsers.cpp
├── packaging/
│   └── debian/
│       ├── control
│       ├── rules
│       ├── logmind-daemon.service  # systemd 服务
│       └── logmind.install         # 打包文件清单
└── docs/
    └── ARCHITECTURE.md
```

---

## 2. 插件 API 接口定义

### 2.1 数据结构 — `log_entry.h`

```cpp
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace logmind {

enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

struct LogEntry {
    std::string                     source;      // 来源插件名, e.g. "parser_nginx"
    std::string                     raw;         // 原始日志行
    std::chrono::system_clock::time_point timestamp;
    LogLevel                        level;
    std::string                     message;
    std::string                     module;      // 源模块/组件
    nlohmann::json                  fields;      // 解析后的结构化字段
    nlohmann::json                  tags;        // 分析标签 (由 Analyzer 写入)
    nlohmann::json                  meta;        // 元数据 (文件路径, 行号等)
};

enum class PluginType : uint8_t {
    Collector = 0,
    Analyzer,
    Alert
};

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    PluginType  type;
    bool        loaded;
    bool        enabled;
};

} // namespace logmind
```

### 2.2 基础插件接口 — `plugin.h`

```cpp
#pragma once
#include "log_entry.h"

namespace logmind {

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual const char* name()        const = 0;
    virtual const char* version()     const = 0;
    virtual const char* description() const = 0;
    virtual PluginType  type()        const = 0;
};

} // namespace logmind
```

### 2.3 采集器接口 — `collector.h`

```cpp
#pragma once
#include "plugin.h"
#include <istream>
#include <string>

namespace logmind {

class ICollector : public IPlugin {
public:
    PluginType type() const final { return PluginType::Collector; }

    // 判断是否能处理该文件 (按文件名/扩展名)
    virtual bool can_parse(const std::string& filename) = 0;

    // 从输入流解析日志，返回 LogEntry 列表
    virtual std::vector<LogEntry> parse(std::istream& stream) = 0;

    // 可选：需要监听的文件 glob 模式
    virtual std::vector<std::string> watch_patterns() const { return {}; }
};

} // namespace logmind
```

### 2.4 分析器接口 — `analyzer.h`

```cpp
#pragma once
#include "plugin.h"

namespace logmind {

class IAnalyzer : public IPlugin {
public:
    PluginType type() const final { return PluginType::Analyzer; }

    // 对一批日志条目做分析，返回标记后的条目（可追加 tags / 过滤）
    virtual std::vector<LogEntry> analyze(std::vector<LogEntry> entries) = 0;
};

} // namespace logmind
```

### 2.5 告警器接口 — `alert.h`

```cpp
#pragma once
#include "plugin.h"

namespace logmind {

struct AlertContext {
    std::string             rule_name;      // 触发规则的名称
    std::string             severity;       // "critical" / "warning" / "info"
    std::vector<LogEntry>   entries;        // 触发的日志条目
    std::string             summary;        // 告警摘要
};

class IAlert : public IPlugin {
public:
    PluginType type() const final { return PluginType::Alert; }

    // 发送告警 (异步非阻塞)
    virtual void send(const AlertContext& ctx) = 0;
};

} // namespace logmind
```

### 2.6 插件导出符号约定

每个 `.so` 必须导出以下两个 C 符号，供 `dlopen` / `dlsym` 调用：

```cpp
extern "C" {

logmind::IPlugin* create_plugin() {
    return new MyPlugin();
}

void destroy_plugin(logmind::IPlugin* plugin) {
    delete plugin;
}

} // extern "C"
```

---

## 3. D-Bus IPC 接口设计

### 3.1 服务定义

```
Service:    com.logmind.Daemon
Object:     /com/logmind/Daemon
Interface:  com.logmind.Daemon1
```

### 3.2 方法 (Methods)

| 方法 | 输入 | 返回 | 说明 |
|------|------|------|------|
| `QueryLogs` | `String query, UInt32 limit` | `Array<Dict>` | 按关键词/表达式查询日志 |
| `QueryLogsByTime` | `UInt64 start, UInt64 end, UInt32 limit` | `Array<Dict>` | 按时间范围查询 |
| `AnalyzeLogs` | `Array<String> logIds` | `Array<Dict>` | 触发分析器处理指定日志 |
| `GetAnalysisReport` | `String logId` | `String` | 获取单条日志的分析报告 (JSON) |
| `ListPlugins` | — | `Array<Dict>` | 列出所有已加载插件 |
| `GetPluginInfo` | `String name` | `Dict` | 获取插件详细信息 |
| `EnablePlugin` | `String name` | `Boolean` | 启用插件 |
| `DisablePlugin` | `String name` | `Boolean` | 禁用插件 |
| `ReloadPlugins` | — | `Boolean` | 重新扫描 & 加载插件目录 |
| `GetStats` | — | `Dict` | 获取 daemon 运行统计 (QPS, 内存等) |
| `GetStatus` | — | `Dict` | 获取 daemon 健康状态 |

### 3.3 信号 (Signals)

| 信号 | 参数 | 说明 |
|------|------|------|
| `LogEntryReceived` | `Dict entry` | 新日志条目到达 |
| `AlertTriggered` | `Dict alert` | 告警被触发 |
| `PluginLoaded` | `String name` | 插件加载成功 |
| `PluginUnloaded` | `String name` | 插件卸载 |
| `DaemonStatusChanged` | `Dict status` | Daemon 状态变化 |

### 3.4 Qt 客户端 D-Bus 适配

```cpp
// 客户端侧使用 QDBusInterface 调用
auto bus = QDBusConnection::sessionBus();
auto iface = new QDBusInterface(
    "com.logmind.Daemon",
    "/com/logmind/Daemon",
    "com.logmind.Daemon1",
    bus, this
);

// 调用方法
QDBusReply<QDBusVariant> reply = iface->call("QueryLogs", query, 100);

// 连接信号
bus.connect("com.logmind.Daemon",
            "/com/logmind/Daemon",
            "com.logmind.Daemon1",
            "LogEntryReceived",
            this, SLOT(onLogEntryReceived(QDBusVariant)));
```

### 3.5 服务端 D-Bus 适配

```cpp
// daemon 侧使用 QDBusConnection::registerObject + QDBusContext
class DaemonAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.logmind.Daemon1")
public:
    explicit DaemonAdaptor(Pipeline* pipeline, QObject* parent);

public slots:
    QVariantList QueryLogs(const QString& query, quint32 limit);
    QVariantList QueryLogsByTime(quint64 start, quint64 end, quint32 limit);
    QVariant     GetStatus();

signals:
    void LogEntryReceived(QVariantMap entry);
    void AlertTriggered(QVariantMap alert);
};
```

---

## 4. 插件加载器引擎架构

### 4.1 PluginLoader 类设计

```cpp
#pragma once
#include "logmind/plugin.h"
#include "logmind/collector.h"
#include "logmind/analyzer.h"
#include "logmind/alert.h"
#include <string>
#include <vector>
#include <memory>
#include <dlfcn.h>

namespace logmind {

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    // 禁止拷贝
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    // 扫描指定目录，加载所有 .so 插件
    bool load_directory(const std::string& path);

    // 加载单个 .so 文件
    bool load_plugin(const std::string& so_path);

    // 卸载单个插件
    void unload_plugin(const std::string& name);

    // 卸载所有插件
    void unload_all();

    // 按类型获取插件列表
    std::vector<ICollector*> collectors() const;
    std::vector<IAnalyzer*>  analyzers() const;
    std::vector<IAlert*>     alerts() const;

    // 按名称查找插件
    IPlugin* find(const std::string& name) const;

    // 获取所有插件信息
    std::vector<PluginInfo> list_plugins() const;

    // 启用/禁用插件 (禁用后跳过管线处理)
    bool set_enabled(const std::string& name, bool enabled);

private:
    struct PluginHandle {
        void*       handle;     // dlopen handle
        IPlugin*    instance;   // 插件实例
        std::string so_path;    // .so 文件路径
        bool        enabled;    // 启用状态
    };

    std::vector<PluginHandle> m_plugins;

    // 通过 dlsym 获取插件实例
    IPlugin* instantiate_plugin(void* handle);
};

} // namespace logmind
```

### 4.2 加载流程

```
load_directory("/usr/lib/logmind/plugins/")
  │
  ├─ scan *.so files
  │
  ├─ for each .so:
  │    ├─ dlopen(RTLD_NOW | RTLD_LOCAL)
  │    ├─ dlsym("create_plugin") → 工厂函数
  │    ├─ create_plugin() → IPlugin*
  │    ├─ 读取 name/version/description
  │    ├─ 按 type() 注册到对应分类容器
  │    └─ 如果失败: dlclose, 记录错误, 继续下一个
  │
  └─ 返回加载结果 (成功数 / 失败数)
```

### 4.3 安全 & 错误处理

- `RTLD_NOW`: 加载时立即解析所有符号，避免运行时崩溃
- `RTLD_LOCAL`: 隔离各插件符号，防止冲突
- 每个插件实例独立管理，卸载时 `instance->~IPlugin()` + `dlclose()`
- 失败的插件不会影响其他插件的加载
- 提供 `PluginLoader::list_plugins()` 获取加载状态和错误信息

---

## 5. Pipeline 管线编排

### 5.1 处理流程

```
[日志文件/流]
     │
     ▼
┌─────────────────────┐
│   Collector 插件     │  can_parse → parse → LogEntry[]
│   (parser_nginx.so)  │
└─────────┬───────────┘
          │ LogEntry[]
          ▼
┌─────────────────────┐
│   Analyzer 插件链    │  analyze(entries) → 标记/过滤/增强
│   (anomaly_det.so)   │
│   (ai_analyzer.so)   │
└─────────┬───────────┘
          │ LogEntry[] (已标记)
          ▼
┌─────────────────────┐
│   Alert 插件链       │  检查告警规则 → send(AlertContext)
│   (email_alert.so)   │
│   (feishu_alert.so)  │
└─────────┬───────────┘
          │
          ▼
    [存储 / D-Bus 通知]
```

```cpp
class Pipeline {
public:
    Pipeline(PluginLoader& loader);

    // 处理一条原始日志 (由文件监听器或 stdin 调用)
    void process_raw(const std::string& source_name,
                     const std::string& raw_line);

    // 处理一批日志条目 (直接传入已解析的条目)
    void process_entries(std::vector<LogEntry> entries);

private:
    PluginLoader&   m_loader;
    VectorStore     m_store;       // 向量存储
    std::mutex      m_mutex;

    std::vector<LogEntry> run_collectors(const std::string& source,
                                          const std::string& raw);
    std::vector<LogEntry> run_analyzers(std::vector<LogEntry> entries);
    void                  run_alerts(const std::vector<LogEntry>& entries);
};
```

---

## 6. CMake 构建系统

### 6.1 根 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(logmind VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 选项
option(BUILD_CLIENT "Build Qt monitor panel" ON)
option(BUILD_TESTS  "Build unit tests" ON)

# 依赖
find_package(Qt6 REQUIRED COMPONENTS Core DBus)
if(BUILD_CLIENT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# 公共头文件
add_library(logmind_core INTERFACE)
target_include_directories(logmind_core INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(logmind_core INTERFACE
    Qt6::Core
    Qt6::DBus
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

add_subdirectory(src/daemon)

if(BUILD_CLIENT)
    add_subdirectory(src/client)
endif()

add_subdirectory(plugins/collectors)
add_subdirectory(plugins/analyzers)
add_subdirectory(plugins/alerts)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 6.2 插件的 CMake 构建模式

每个插件使用 `MODULE` 类型编译为 `.so`，非 `SHARED`：

```cmake
# plugins/collectors/parser_nginx/CMakeLists.txt
add_library(parser_nginx MODULE
    parser_nginx.cpp
)
target_link_libraries(parser_nginx PRIVATE
    logmind_core
)
set_target_properties(parser_nginx PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/collectors"
    PREFIX ""                        # 去掉 lib 前缀 → parser_nginx.so
)
```

### 6.3 插件自动注册 (PluginScan.cmake)

```cmake
# cmake/PluginScan.cmake
# 自动扫描插件目录并添加子目录

function(add_plugin_subdirs base_dir)
    file(GLOB plugin_dirs LIST_DIRECTORIES true
        RELATIVE "${base_dir}" "${base_dir}/*"
    )
    foreach(dir ${plugin_dirs})
        if(IS_DIRECTORY "${base_dir}/${dir}")
            if(EXISTS "${base_dir}/${dir}/CMakeLists.txt")
                add_subdirectory("${base_dir}/${dir}")
            endif()
        endif()
    endforeach()
endfunction()
```

使用方式：

```cmake
# plugins/collectors/CMakeLists.txt
include(PluginScan)
add_plugin_subdirs("${CMAKE_CURRENT_SOURCE_DIR}")
```

### 6.4 安装规则

```cmake
# 插件安装到标准路径
install(TARGETS parser_nginx
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/logmind/plugins/collectors
)
```

---

## 7. Daemon 启动流程

```
main()
  │
  ├─ 解析命令行参数 (--plugins-dir, --config, --verbose)
  │
  ├─ 初始化 PluginLoader
  │    └─ load_directory(plugins_dir)
  │
  ├─ 初始化 Pipeline (传入 PluginLoader)
  │
  ├─ 初始化 VectorStore (SQLite)
  │
  ├─ 注册 D-Bus 服务
  │    ├─ QDBusConnection::sessionBus().registerService("com.logmind.Daemon")
  │    └─ QDBusConnection::sessionBus().registerObject("/com/logmind/Daemon", ...)
  │
  ├─ 初始化文件监听器 (inotify / std::filesystem::directory_iterator)
  │
  ├─ 进入 Qt 事件循环 (QCoreApplication::exec())
  │
  └─ 退出时:
       ├─ 停止文件监听
       ├─ PluginLoader::unload_all()
       └─ 释放 D-Bus 服务
```

---

## 7.1 文件监听策略 (inotify + fallback)

生产环境使用 Linux inotify 实现实时文件追加监听，避免轮询开销：

```cpp
class FileWatcher {
public:
    FileWatcher(Pipeline& pipeline, std::chrono::milliseconds poll_interval = 1s);

    // 添加监听文件/目录
    void watch(const std::string& path);

    // 启动监听 (Linux: inotify, 否则 fallback 轮询)
    void start();

    void stop();

private:
    // inotify 模式：监听 IN_MODIFY / IN_CREATE
    void inotify_loop();
    int m_inotify_fd = -1;
    std::map<int, std::string> m_watch_map;  // wd → path

    // fallback 轮询模式：定期 stat + 比较文件大小
    void poll_loop();
    std::unordered_map<std::string, uintmax_t> m_file_sizes;

    Pipeline&       m_pipeline;
    std::thread     m_thread;
    std::atomic<bool> m_running{false};
};
```

**inotify 工作流程：**

```
FileWatcher::watch("/var/log/nginx/")
  │
  ├─ inotify_add_watch(fd, "/var/log/nginx/access.log", IN_MODIFY)
  │
  ├─ 事件循环:
  │    ├─ read(fd) → 获取 inotify_event
  │    ├─ 匹配文件名
  │    ├─ seek 到上次尾部位置
  │    ├─ read 新追加内容
  │    ├─ pipeline.process_raw(source, line)
  │    └─ 更新尾部偏移
  │
  └─ 退出时: inotify_rm_watch + close(fd)
```

**fallback 轮询：**

```
对所有被监听文件:
  1. stat() 获取当前大小
  2. 如果 > 上次记录大小:
     - 打开文件, seek 到上次位置
     - 读取增量内容, 送入 pipeline
     - 更新记录大小
  3. 间隔 1s 重复
```

---


## 8. 向量存储引擎 (VectorStore)

为 AI 分析器提供简单的本地向量检索能力，无需外部依赖：

```cpp
class VectorStore {
public:
    bool open(const std::string& db_path);   // SQLite 数据库
    void close();

    void insert(const std::string& log_id,
                const std::vector<float>& embedding,
                const std::string& metadata_json);

    // HNSW 近似最近邻搜索
    std::vector<std::string> search(
        const std::vector<float>& query_embedding,
        size_t top_k);

private:
    sqlite3* m_db = nullptr;
    // HNSW 图索引 (内存)
    struct HNSWGraph { /* ... */ };
    std::unique_ptr<HNSWGraph> m_hnsw;
};
```

---

## 9. 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 插件类型 | dlopen + 虚接口 | 零运行时耦合，静态类型安全 |
| 插件隔离 | 每个 .so 独立进程内加载 | 避免跨进程开销，RTLD_LOCAL 隔离符号 |
| IPC | D-Bus session bus | 符合 Linux 桌面生态，Qt 原生支持 |
| 依赖管理 | apt install + CMake find_package | 减少构建系统复杂度，deb 打包时声明依赖控制 |
| JSON 库 | nlohmann-json3-dev (apt) | Ubuntu 官方源维护，无需 vcpkg |
| HTTP 客户端 | libcurl (仅 ai_analyzer 插件链接) | 主程序不直接依赖 CURL，插件自链接 |
| 日志监听 | inotify (Linux) + fallback 轮询 | 详见 §7.1 文件监听策略 |
| 数据交换 | JSON (nlohmann) | 跨语言/跨进程通用，D-Bus 原生支持变体 |
| 构建 | CMake MODULE 库 | 自动生成 .so，无 lib 前缀，安装路径规范 |
| 向量存储 | SQLite + 自研 HNSW | 零外部依赖，嵌入式中等规模可用 |
| 异步处理 | std::async + 线程池 | C++17 标准，无需引入第三方线程库 |
| 配置 | JSON 配置文件 | 与 D-Bus 数据格式一致，易于扩展 |

---

## 10. 下一步实施顺序

1. **Phase 0** — 搭建项目骨架 (目录结构、根 CMake、公共头文件)
2. **Phase 1** — PluginLoader 实现 + 示例 Collector 插件 (parser_nginx)
3. **Phase 2** — D-Bus 服务端 + Qt 客户端基础通信
4. **Phase 3** — Pipeline 管线编排 + Analyzer 插件
5. **Phase 4** — Alert 插件 + 告警机制
6. **Phase 5** — Qt 监控面板 (MVC 日志流、图表、插件管理)
7. **Phase 6** — AI 分析器 (Ollama/OpenAI 集成)
8. **Phase 7** — VectorStore 向量检索
9. **Phase 8** — Debian 打包 + systemd 服务
10. **Phase 9** — RISC-V 交叉编译验证
