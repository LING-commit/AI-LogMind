# AI-LogMind

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)

基于 C++17 和 Qt6 的智能日志采集与分析系统，支持插件化架构和实时监控。

## 功能特性

- **🔌 插件化架构**：支持动态加载 Collector、Analyzer、Alert 三类插件
- **📊 实时监控**：基于 inotify（Linux）的高效文件监控，支持轮询 fallback
- **🗄️ 向量存储**：集成 HNSW 向量数据库，支持日志语义检索
- **🔗 进程间通信**：通过 D-Bus 提供标准接口，支持多客户端访问
- **🎯 灵活的日志处理流水线**：Collector → Analyzer → Alert 可配置流程
- **⚡ 高性能**：支持多线程处理、零拷贝优化、LTO 链接时优化

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                     Client (Qt6 GUI)                     │
└────────────────────┬────────────────────────────────────┘
                     │ D-Bus IPC
┌────────────────────▼────────────────────────────────────┐
│                   LogMind Daemon                         │
│  ┌─────────────┐  ┌──────────┐  ┌─────────────────┐   │
│  │ FileWatcher │─▶│ Pipeline │─▶│  VectorStore    │   │
│  └─────────────┘  └─────┬────┘  └─────────────────┘   │
│                          │                               │
│  ┌──────────────────────▼──────────────────────────┐   │
│  │           PluginLoader (ABI v1)                  │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐     │   │
│  │  │Collector │  │ Analyzer │  │  Alert   │     │   │
│  │  │ Plugins  │  │ Plugins  │  │ Plugins  │     │   │
│  │  └──────────┘  └──────────┘  └──────────┘     │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 核心组件

- **FileWatcher**: 监控日志文件变化（inotify + 轮询 fallback）
- **Pipeline**: 日志处理流水线，协调插件执行
- **PluginLoader**: 动态加载 `.so` 插件，管理插件生命周期
- **VectorStore**: 向量数据库，支持语义相似度检索
- **LogBuffer**: 环形缓冲区，高效存储日志条目
- **ConfigMgr**: 配置管理，支持 JSON 格式

### 插件系统

支持三类插件：

1. **Collector（采集器）**: 解析原始日志行，提取结构化数据
2. **Analyzer（分析器）**: 对日志进行智能分析（异常检测、关联分析等）
3. **Alert（告警器）**: 根据规则发送告警（邮件、Webhook 等）

插件接口遵循 ABI 版本控制，保证二进制兼容性。

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
# 克隆项目
git clone https://github.com/LING-commit/AI-LogMind.git
cd AI-LogMind

# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_CLIENT=ON \
      -DBUILD_TESTS=ON

# 编译
cmake --build build -j$(nproc)

# 安装（可选）
sudo cmake --install build
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_CLIENT` | OFF | 是否构建 Qt6 客户端 |
| `BUILD_TESTS` | OFF | 是否构建测试代码 |
| `ENABLE_ASAN` | OFF | 启用 AddressSanitizer |
| `ENABLE_UBSAN` | OFF | 启用 UndefinedBehaviorSanitizer |
| `ENABLE_TSAN` | OFF | 启用 ThreadSanitizer |

## 使用方法

### 启动守护进程

```bash
# 使用默认配置
./build/logmind-daemon

# 指定配置文件
./build/logmind-daemon -c /path/to/config.json

# 指定插件目录
./build/logmind-daemon -p /usr/lib/logmind/plugins

# 启用详细日志
./build/logmind-daemon -V
```

### 配置文件示例

```json
{
  "plugins_dir": "/usr/lib/logmind/plugins",
  "log_dirs": ["/var/log", "/home/user/logs"],
  "watch_patterns": ["*.log", "*.txt"],
  "vector_store": {
    "db_path": "/var/lib/logmind/vectors.db",
    "hnsw_m": 16,
    "hnsw_ef_construction": 200,
    "max_elements": 100000,
    "persist": true
  }
}
```

### D-Bus 接口

```bash
# 查询守护进程状态
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon \
  /com/logmind/Daemon \
  org.freedesktop.DBus.Properties.Get \
  string:"com.logmind.Daemon" string:"State"

# 注入测试日志
dbus-send --session --print-reply \
  --dest=com.logmind.Daemon \
  /com/logmind/Daemon \
  com.logmind.Daemon.InjectLog \
  string:"test.log" string:"[ERROR] Test message"

# 重载插件（发送 SIGHUP）
kill -HUP $(pidof logmind-daemon)
```

## 开发插件

### 创建 Collector 插件

```cpp
#include <logmind/collector.h>

class MyCollector : public logmind::ICollector {
public:
    std::string name() const override { return "my-collector"; }
    std::string version() const override { return "1.0.0"; }
    
    int match(const std::string& source) override {
        return source.find("myapp.log") != std::string::npos ? 100 : 0;
    }
    
    ParseResult parse(std::istream& stream) override {
        ParseResult result;
        std::string line;
        while (std::getline(stream, line)) {
            LogEntry entry;
            // 解析 line 填充 entry
            entry.message = line;
            entry.timestamp = std::chrono::system_clock::now();
            result.entries.push_back(std::move(entry));
        }
        return result;
    }
};

extern "C" {
    int plugin_abi_version() { return LOGMIND_ABI_VERSION; }
    
    IPlugin* create_plugin(CreateContext ctx) {
        return new MyCollector();
    }
    
    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}
```

### 编译插件

```cmake
add_library(my_collector SHARED my_collector.cpp)
target_link_libraries(my_collector PRIVATE logmind::plugin_sdk)
install(TARGETS my_collector LIBRARY DESTINATION lib/logmind/plugins)
```

## 测试

```bash
# 运行所有测试
cd build && ctest --output-on-failure

# 运行烟雾测试
./build/tests/smoke/smoke_test

# 运行压力测试
./build/tests/stress/stress_test

# 运行 RISC-V 架构测试
./build/tests/stress/riscv_test
```

## 性能指标

在配置为 Intel i7-10700K, 32GB RAM, NVMe SSD 的测试环境：

- **日志处理吞吐量**: ~50,000 行/秒（单线程）
- **文件监控延迟**: < 100ms（inotify 模式）
- **内存占用**: ~30MB（基础守护进程，无插件）
- **向量检索性能**: ~1ms（HNSW，10万条目）

## 项目结构

```
AI-LogMind/
├── CMakeLists.txt          # 根 CMake 配置
├── include/logmind/        # 公共头文件（Plugin SDK）
├── src/
│   ├── daemon/             # 守护进程核心
│   └── client/             # Qt6 GUI 客户端
├── tests/
│   ├── smoke/              # 烟雾测试
│   ├── stress/             # 压力测试
│   └── plugins/            # 插件测试
└── plugins/                # 示例插件（如果存在）
```

## 技术栈

- **语言**: C++17
- **GUI 框架**: Qt6 (Core, DBus, Widgets)
- **数据库**: SQLite3
- **JSON 解析**: nlohmann/json
- **向量检索**: HNSW (hnswlib)
- **构建系统**: CMake 3.20+
- **平台支持**: Linux (GCC 11+ / Clang 14+)

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

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

## 致谢

- [Qt Project](https://www.qt.io/) - 跨平台 GUI 框架
- [nlohmann/json](https://github.com/nlohmann/json) - 现代 C++ JSON 库
- [hnswlib](https://github.com/nmslib/hnswlib) - 快速近似最近邻检索

## 联系方式

- 作者: Liu Lingyu
- Email: lingyu.liu@spacemit.com
- 项目主页: https://github.com/LING-commit/AI-LogMind

---

**注**: 本项目仍在开发中，API 可能会变更。生产环境使用前请充分测试。
