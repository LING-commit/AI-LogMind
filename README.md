# AI-LogMind

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)

基于 C++17 和 Qt6 的插件化日志采集与分析系统，支持实时文件监控与向量检索。

> **关于项目名里的 "AI"**：本仓库目前**不包含任何模型推理代码**——没有 LLM 调用，
> 也不生成 embedding。它提供的是 AI 分析所需的**基础设施**：
> `IAnalyzer::ai_analyze()` 扩展点、以及一个能存能查的向量库。
> 向量由 Analyzer 插件产出（写进 `entry.fields["embedding"]`），守护进程只负责
> 存储与检索。详见 [接入 AI 分析](#接入-ai-分析)。

## 功能特性

- **🔌 插件化架构**：动态加载 Collector、Analyzer、Alert 三类插件，带 ABI 版本校验
- **📊 实时监控**：基于 inotify 的目录级监控，正确处理日志轮转与写了一半的行
- **🗄️ 向量存储**：内置 HNSW 索引 + SQLite 持久化，支持相似日志检索
- **🔗 进程间通信**：通过 D-Bus 提供接口，支持多客户端访问
- **🎯 可配置流水线**：Collector → Analyzer → Alert，告警带级别阈值、指纹去重与限流
- **⚡ 可选异步**：`worker_threads > 1` 时按 source 分片并行处理，单文件内保持顺序

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                     Client (Qt6 GUI)                     │
└────────────────────┬────────────────────────────────────┘
                     │ D-Bus IPC
┌────────────────────▼────────────────────────────────────┐
│                   LogMind Daemon                         │
│  ┌─────────────┐  ┌──────────┐  ┌─────────────────┐   │
│  │ FileWatcher │─▶│ Pipeline │─▶│    LogBuffer    │   │
│  └─────────────┘  └─────┬────┘  └─────────────────┘   │
│                          │                               │
│                          ├──────▶┌─────────────────┐   │
│                          │       │  VectorStore    │   │
│                          │       │ (有 embedding   │   │
│                          │       │  的条目)         │   │
│  ┌──────────────────────▼──────────────────────────┐   │
│  │           PluginLoader (ABI v2)                  │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐     │   │
│  │  │Collector │  │ Analyzer │  │  Alert   │     │   │
│  │  │ Plugins  │  │ Plugins  │  │ Plugins  │     │   │
│  │  └──────────┘  └──────────┘  └──────────┘     │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 核心组件

- **FileWatcher**: 监控日志**目录**（inotify + 轮询 fallback）。跟随 logrotate，
  只在遇到换行符后才提交整行
- **Pipeline**: 协调插件执行；告警规则（级别阈值 / 去重 / 限流）在这里统一实施
- **PluginLoader**: 动态加载 `.so`。插件由 `shared_ptr` 持有，重载时不会把正在
  执行的代码段抽走
- **VectorStore**: HNSW 索引 + SQLite 持久化，重启后从库里重建索引
- **LogBuffer**: 环形缓冲 + 分段倒排索引（随缓冲区整段淘汰，内存有界）
- **ConfigMgr**: JSON 配置

### 插件系统

支持三类插件：

1. **Collector（采集器）**: 解析原始日志行，提取结构化数据
2. **Analyzer（分析器）**: 分析、标注或过滤条目（可产出 embedding）
3. **Alert（告警器）**: 投递已被流水线判定需要告警的条目

`plugins/` 下有三个可直接编译运行的示例，分别对应上述三类。

**ABI 兼容性**：`plugin_abi_version()` 必须返回 `logmind::LOGMIND_ABI_VERSION`
（当前为 **2**）。缺少该符号或版本不符的 `.so` 会被拒绝加载。

## 构建与安装

### 依赖项

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake pkg-config \
    qt6-base-dev libsqlite3-dev nlohmann-json3-dev

# Fedora/RHEL
sudo dnf install gcc-c++ cmake pkgconfig \
    qt6-qtbase-devel sqlite-devel json-devel
```

### 编译

```bash
git clone https://github.com/LING-commit/AI-LogMind.git
cd AI-LogMind

cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_CLIENT=ON \
      -DBUILD_TESTS=ON
cmake --build build -j$(nproc)

sudo cmake --install build     # 可选
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_CLIENT` | OFF | 是否构建 Qt6 客户端 |
| `BUILD_TESTS` | OFF | 是否构建测试代码 |
| `ENABLE_ASAN` | OFF | 启用 AddressSanitizer |
| `ENABLE_UBSAN` | OFF | 启用 UndefinedBehaviorSanitizer |
| `ENABLE_TSAN` | OFF | 启用 ThreadSanitizer |

> 守护进程以 `ENABLE_EXPORTS ON` 构建。插件 SDK 是 header-only，
> `ICollector` 等接口的 typeinfo 编在可执行文件里，不导出动态符号的话
> 所有插件都会 dlopen 失败。自建宿主程序时请照做。

## 快速上手

```bash
# 1. 编译（含示例插件）
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# 2. 拿示例配置改一份
mkdir -p ~/.config/logmind
sed 's|/usr/lib/logmind/plugins|'"$PWD"'/build/plugins|' \
    config/config.example.json > ~/.config/logmind/config.json

# 3. 起守护进程
./build/logmind-daemon -c ~/.config/logmind/config.json -V

# 4. 另开一个终端灌一条日志
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon /com/logmind/Daemon \
  com.logmind.Daemon1.InjectLog \
  string:"app.log" string:"2024-01-15T10:23:45 ERROR [db] connection refused"
```

stdout-alert 插件会打印出这条告警。

### 命令行参数

```bash
./build/logmind-daemon                      # 默认配置
./build/logmind-daemon -c /path/config.json # 指定配置文件
./build/logmind-daemon -p /path/plugins     # 指定插件目录
./build/logmind-daemon -V                   # 详细日志
kill -HUP $(pidof logmind-daemon)           # 重载插件
```

### 配置文件

完整示例见 [`config/config.example.json`](config/config.example.json)。要点：

```json
{
  "plugins_dir": "/usr/lib/logmind/plugins",
  "log_dirs": ["/var/log"],
  "watch_patterns": ["*.log"],
  "worker_threads": 4,
  "log_buffer":  { "capacity": 50000 },
  "alerts": {
    "min_level": "ERROR",
    "dedup_window_seconds": 60,
    "max_alerts_per_minute": 60
  },
  "plugins": {
    "threshold-analyzer": { "error_threshold": 5 }
  },
  "vector_store": { "db_path": "/var/lib/logmind/vectors.db", "hnsw_m": 16 }
}
```

- `plugins.<插件名>` 那一段会作为参数传给该插件的 `on_load()`，键名取
  `manifest().name`
- `worker_threads > 1` 时流水线转为异步：采集线程只投递，按 source 分片处理
- 插件通过 `watch_patterns()` 声明的监控目标优先于 `log_dirs`/`watch_patterns`

### D-Bus 接口

接口名是 **`com.logmind.Daemon1`**（服务名与对象路径不带 `1`）。

```bash
# 守护进程状态
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon /com/logmind/Daemon \
  org.freedesktop.DBus.Properties.Get \
  string:"com.logmind.Daemon1" string:"State"

# 注入一条日志
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon /com/logmind/Daemon \
  com.logmind.Daemon1.InjectLog \
  string:"test.log" string:"[ERROR] Test message"

# 运行统计
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon /com/logmind/Daemon \
  com.logmind.Daemon1.GetStats
```

主要方法：`QueryLogs`、`QueryLogsByTime`、`SearchSimilar`、`InjectLog`、
`ListPlugins`、`GetPluginInfo`、`EnablePlugin`、`DisablePlugin`、
`ReloadPlugins`、`GetStats`、`GetStatus`。

## 开发插件

参考 `plugins/` 下的三个完整示例。最小 Collector 骨架：

```cpp
#include <logmind/collector.h>
#include <logmind/version.h>

class MyCollector : public logmind::ICollector {
public:
    logmind::PluginManifest manifest() const override {
        logmind::PluginManifest m;
        m.name    = "my-collector";
        m.version = "1.0.0";
        m.type    = logmind::PluginType::Collector;
        return m;
    }

    bool on_load(const nlohmann::json& config) override {
        // config 来自 config.json 的 plugins["my-collector"]
        (void)config;
        return true;
    }

    int match(const std::string& source) const override {
        return source.find("myapp.log") != std::string::npos ? 100 : 0;
    }

    ParseResult parse(std::istream& stream) override {
        ParseResult result;
        std::string line;
        while (std::getline(stream, line)) {
            result.entries.push_back(parse_one(line));
        }
        return result;
    }

    // 可选但推荐：流水线是逐行调用的，覆写它可以省掉每行一个 istringstream
    ParseResult parse_line(const std::string& line) override {
        ParseResult result;
        result.entries.push_back(parse_one(line));
        return result;
    }

private:
    logmind::LogEntry parse_one(const std::string& line) {
        logmind::LogEntry entry;
        entry.message   = line;
        entry.timestamp = std::chrono::system_clock::now();
        return entry;
    }
};

extern "C" {
LOGMIND_PLUGIN_EXPORT int plugin_abi_version() {
    return logmind::LOGMIND_ABI_VERSION;
}
LOGMIND_PLUGIN_EXPORT logmind::IPlugin* create_plugin(logmind::CreateContext ctx) {
    (void)ctx;
    return new MyCollector();
}
LOGMIND_PLUGIN_EXPORT void destroy_plugin(logmind::IPlugin* plugin) {
    delete plugin;
}
}
```

三个入口点上的 `LOGMIND_PLUGIN_EXPORT` 不能省：插件通常用
`-fvisibility=hidden` 编译，不标注的话 `dlsym` 找不到它们。

### 编译插件

```cmake
add_library(my_collector MODULE my_collector.cpp)
target_link_libraries(my_collector PRIVATE logmind::plugin_sdk)
set_target_properties(my_collector PROPERTIES
  PREFIX "" CXX_VISIBILITY_PRESET hidden)
install(TARGETS my_collector LIBRARY DESTINATION lib/logmind/plugins)
```

## 接入 AI 分析

守护进程本身不做向量化。要让相似日志检索跑起来，写一个 Analyzer 插件，
在 `analyze()` 里把向量塞进 `entry.fields["embedding"]`：

```cpp
std::vector<logmind::LogEntry> analyze(std::vector<logmind::LogEntry> entries) override {
    for (auto& entry : entries) {
        std::vector<float> vec = my_embedder.encode(entry.message);
        entry.fields["embedding"] = vec;      // 守护进程会自动写进 VectorStore
    }
    return entries;
}
```

之后就能通过 D-Bus 的 `SearchSimilar(embedding, top_k)` 查询。

`IAnalyzer` 另外预留了 `supports_ai_analysis()` / `ai_analyze()` 两个扩展点，
供 LLM 归因类插件使用。**核心代码里没有它们的实现**，目前也没有调用点。

## 测试

```bash
cmake -B build -DBUILD_TESTS=ON && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

包含 `smoke-test`、`stress-test`、`riscv-test` 三个目标。

## 性能

测试环境：AMD64、16 核、GCC 13、`-O2`。以下数字由 `tests/` 下的压测复现，
**不是**理论峰值。

| 项目 | 实测 |
|------|------|
| Pipeline 单线程（采集器+分析器+告警器全加载） | ~78 万行/秒 |
| Pipeline 4 worker（8 个 source 分片） | ~100 万行/秒 |
| LogBuffer push（含倒排索引） | ~116 万行/秒 |
| LogBuffer 索引检索（limit=10） | ~2.4 µs/次 |
| VectorStore 建索引（384 维，`ef_construction=200`） | ~420 条/秒 |
| VectorStore 检索（2 万条 384 维向量，top-10） | ~1.8 ms/次 |

文件监控延迟没有单独测量。inotify 模式下监控线程阻塞在 `poll()` 上，
内核有事件即被唤醒，不存在固定的轮询间隔；`poll_interval` 只在
inotify 不可用而回退到轮询模式时生效。

说明：

- 行吞吐取决于日志格式与插件的复杂度。示例采集器是手写解析（不用正则）；
  换成正则实现会显著变慢。
- HNSW 建索引比检索慢两三个数量级。高频写入场景请用 `insert_batch()`
  （整批一个事务，而不是每条一次 fsync）。
- 检索召回率对照暴力检索验证过：`M=16, ef_search=96` 下 recall@10 = 100%
  （1500 条 48 维随机向量）。`hnsw_ef_search` 调小会更快但召回下降。

## 项目结构

```
AI-LogMind/
├── CMakeLists.txt          # 根 CMake 配置
├── config/
│   └── config.example.json # 配置示例
├── include/logmind/        # 公共头文件（Plugin SDK）
├── src/
│   ├── daemon/             # 守护进程核心
│   └── client/             # Qt6 GUI 客户端
├── plugins/                # 示例插件
│   ├── syslog_collector/
│   ├── threshold_analyzer/
│   └── stdout_alert/
└── tests/
    ├── smoke/
    ├── stress/
    └── plugins/            # 测试用 mock 插件
```

## 技术栈

- **语言**: C++17
- **GUI 框架**: Qt6 (Core, DBus, Widgets)
- **数据库**: SQLite3（向量持久化，WAL 模式）
- **JSON 解析**: nlohmann/json
- **向量检索**: 内置 HNSW 实现（`src/daemon/vector_store.cpp`，无外部依赖）
- **构建系统**: CMake 3.20+
- **平台支持**: Linux (GCC 11+ / Clang 14+)

## 已知问题

- **D-Bus 没有访问控制**。`dbus_service.cpp` 里的 `check_permission()` /
  `is_member_of_group()` 是**未接线的桩**——没有任何调用点，且后者恒返回 `true`。
  只要能连上会话总线的进程都可以调用 `InjectLog` / `ReloadPlugins`。
  不要在多用户机器上以特权身份运行。
- **日志不落盘**。`LogBuffer` 是纯内存环形缓冲，重启即丢。
- **`AnalyzeLogs` / `GetAnalysisReport` 返回占位内容**，等待 Analyzer 插件实现。
- **重载插件期间**（SIGHUP / `ReloadPlugins`）有一个短暂窗口，此时插件集为空，
  期间到达的日志会走 fallback 采集路径。

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

### 代码规范

- 遵循 C++ Core Guidelines
- 使用 `clang-format` 格式化代码
- 所有公共 API 必须有文档注释
- 新功能需附带单元测试
- 改动 `IPlugin`/`ICollector`/`IAnalyzer`/`IAlert` 的对象布局或虚表时，
  必须递增 `LOGMIND_ABI_VERSION`

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

## 致谢

- [Qt Project](https://www.qt.io/) - 跨平台 GUI 框架
- [nlohmann/json](https://github.com/nlohmann/json) - 现代 C++ JSON 库
- [hnswlib](https://github.com/nmslib/hnswlib) - 本项目的 HNSW 实现参考了
  Malkov & Yashunin 的论文与 hnswlib 的设计（未直接依赖该库）

## 联系方式

- 作者: Liu Lingyu
- Email: 258143494@qq.com
- 项目主页: https://github.com/LING-commit/AI-LogMind

---

**注**: 本项目仍在开发中，API 可能会变更。生产环境使用前请充分测试，
并留意上面的「已知问题」。
