
---
## 11. 补充需求（由需求分析师 req-b 补充）

### 11.1 边缘场景与异常处理需求

| 需求 ID | 描述 | 验收标准 |
|---------|------|----------|
| NRE-01 | **超大日志单行处理**<br>当单行日志超过 1MB 时（如堆栈追踪、JSON 数据泄露），不应导致内存爆炸或解析卡顿。 | 单行日志处理内存峰值 ≤ 10MB，处理延迟 ≤ 500ms/行 |
| NRE-02 | **损坏日志容错**<br>日志文件出现乱码、不完整UTF-8序列或二进制数据时，应能跳过错误字符继续解析后续内容。 | 错误字符替换为 � 或省略，不影响后续日志解析，错误率统计单独记录 |
| NRE-03 | **插件依赖缺失降级**<br>插件依赖的系统库（如 libpcre、libssl）缺失时，应记明确错误并禁用该插件，不影响其他插件加载。 | daemon 启动时记录 ERROR 日志，插件列表中显示为 “依赖缺失”，其他插件正常工作 |
| NRE-04 | **磁盘空间耗尽保护**<br>向量存储或日志缓存目录磁盘已满时，应停止写入旧数据并报警，而非崩溃。 | 检测到磁盘空间 < 1GB 时通过 D-Bus 发送低磁盘警告，新写入自动失败但读取服务仍可用 |
| NRE-05 | **时间戳异常处理**<br>日志中的时间戳格式错误（如未来时间、1970年前）或缺失时，应使用文件修改时间或采集时间作为后备。 | 时间戳无效时自动回退到文件系统时间，UI 中标记为 “时间疑似异常” |
| NRE-06 | **并发插件加载冲突**<br>多个进程同时尝试安装/更新同一插件时，应使用文件锁防止竞态条件。 | 仅有一个进程成功完成安装，其他进程获得 “插件正在被其他进程更新” 错误 |
| NRE-07 | **D-Bus 服务不可用降级**<br>当 D-Bus 守护进程崩溃或被杀死时，daemon 应继续处理日志但暂时无法提供查询服务。 | daemon 日志中记录 WARN，客户端显示连接断开，重新连接后自动恢复功能 |
| NRE-08 | **恶意插件隔离**<br>即使插件包含故意无限循环或耗尽资源的代码，也不应拖垮整个 daemon。 | 插件执行超时（可配置，默认 30s）后自动终止该插件实例并标记为故障 |
| NRE-09 | **日志洪流保护**<br>突发流量（如 DDoS 攻击日志）导致解析速度跟不上写入速度时，应采用背压机制而非 OOM。 | 当内存占用超过阈值时，暂停新文件读取直至处理速度赶上，不丢失已在管线中的数据 |
| NRE-10 | **配置文件损坏恢复**<br>/etc/logmind/config.json 损坏或格式错误时，应使用内置默认配置启动并报错。 | 启动时记录 ERROR 并使用默认配置，不阻塞 daemon 启动过程 |

### 11.2 不同用户角色的权限边界

#### 11.2.1 权限矩阵详细说明

| 权限项 | SRE/运维 | 后端开发 | 测试工程师 | 技术负责人 | DevOps | 说明 |
|--------|----------|----------|------------|------------|--------|------|
| **日志查看** | ✅ | ✅ | ✅ | ✅ | ✅ | 所有角色均可实时查看和历史检索日志 |
| **日志过滤/搜索** | ✅ | ✅ | ✅ | ✅ | ✅ | 支持关键字、时间范围、来源过滤 |
| **AI 分析触发** | ✅ | ✅ | ✅ | ✅ | ✅ | 可对任意日志触发 AI 分析获取根因建议 |
| **插件启用/禁用** | ❌ | ❌ | ❌ | ✅ | ✅ | 仅技术负责人和 DevOps 可修改插件启用状态 |
| **插件重载** | ❌ | ❌ | ❌ | ✅ | ✅ | 触发插件目录重新扫描和加载 |
| **插件安装/卸载** | ❌ | ❌ | ❌ | ✅ | ✅ | 需要对插件目录的写入权限 |
| **告警规则配置** | ❌ | ❌ | ❌ | ✅ | ✅ | 包括阈值设置、通知渠道选择、去重窗口 |
| **监听路径修改** | ❌ | ❌ | ❌ | ✅ | ✅ | 新增/删除日志文件或目录的监听 |
| **系统状态查看** | ✅ | ❌ | ❌ | ✅ | ✅ | 仅 SRE 和技术负责人可查看内存、CPU、吞吐等指标 |
| **AI API Key 配置** | ❌ | ❌ | ❌ | ✅ | ✅ | 高敏感操作，仅限最高权限角色 |
| **告警历史查看** | ✅ | ✅ | ✅ | ✅ | ✅ | 所有角色可查看已发送的告警记录 |
| **插件开发/调试** | ❌ | ❌ | ❌ | ❌ | ✅ | DevOps 专有权限，需要访问插件 SDK 和编译环境 |

#### 11.2.2 权限实现机制

1. **基于 Linux 用户组**：
   - `logmind-adm`：技术负责人组，拥有 `/etc/logmind/` 写权限和 D-Bus 管理权限
   - `logmind-dev`：DevOps 组，拥有插件目录 `/usr/lib/logmind/plugins/` 写权限
   - `logmind-user`：普通用户组（SRE/开发/测试），仅具备读取和查询权限

2. **D-Bus 访问控制**：
   - 通过 `/etc/dbus-1/system.d/logmind.conf` 配置策略
   - 不同方法对应不同用户组的访问权限
   - 例如：`EnablePlugin` 方法仅允许 `logmind-adm` 和 `logmind-dev` 调用

3. **Qt 客户端界面权限控制**：
   - 根据当前登录用户的 Linux 动态检查所属用户组
   - 禁用无权限的 UI 元素（按钮、菜单项）
   - 即使通过手动 D-Bus 调用也会被后端策略拒绝

### 11.3 插件扩展的非功能约束

| 约束类别 | 具体要求 | 理由 |
|----------|----------|------|
| **接口稳定性** | 插件接口（ICollector/IAnalyzer/IAlert）在同一 major 版本内保持向后兼容 | 防止插件因 daemon 升级而频繁重新编译 |
| **资源限制** | 单个插件实例峰值内存 ≤ 100MB，CPU 占用 ≤ 1核心（平均） | 防止单个插件耗尽系统资源影响其他插件 |
| **执行时限** | 插件单次处理回调（如 parse/analyze/send）必须在 5秒内完成 | 防止插件卡死阻塞整个处理管线 |
| **线程安全** | 插件方法必须是线程安全的，因为可能被多个调度线程并发调用 | daemon 内部使用线程池提升吞吐 |
| **无阻塞 I/O** | 插件内部不得进行同步磁盘/网络阻塞操作，应使用异步API或委托给 daemon 的线程池 | 保持管线的响应性和吞吐量 |
| **错误隔离** | 插件内部异常不应传播到 daemon 主线程，应捕获并转换为错误日志 | 单个插件崩溃不应影响整个系统 |
| **日志脱敏责任** | 插件自行负责在输出到外部系统（如告警）前脱敏敏感信息（密码、token） | daemon 无法知晓插件内部业务语义 |
| **版本声明** | 插件必须通过 `version()` 方法返回符合 SemVer 的版本号 | 便于运维追踪插件更新和兼容性问题 |
| **最小依赖** | 插件应尽量减少对第三方库的依赖，优先使用 daemon 提供的基础能力 | 减少部署复杂度和潜在冲突 |
| **跨平台兼容性** | 插件源代码应避免平台特定假设，以便在 x86_64、ARM64、RISC-V 上编译 | 支持 daemon 的跨平台目标 |

#### 11.3.1 插件 SDK 与约束检查

daemon 将提供一个简单的验证工具 `logmind-plugin-validator`：

```bash
# 编译时验证
logmind-plugin-validator --check-interface ./my_plugin.so

# 运行时沙箱测试
logmind-plugin-validator --sandbox-test ./my_plugin.so --test-data sample.log
```

该工具将检查：
- 接口符号是否完整导出
- 基本功能调用是否在资源限制内
- 是否存在明显的阻塞调用或内存泄漏倾向
- 是否符合线程安全基本要求（静态分析）

### 11.4 部署与运维需求

#### 11.4.1 部署方式

| 部署场景 | 推荐方式 | 说明 |
|----------|----------|------|
| **生产环境** | Debian 包 (`apt install logmind`) | 官方维护的 .deb 包，包含 systemd 服务和默认插件 |
| **开发测试** | 本地编译 + `make install` | 方便快速迭代和调试 |
| **容器化部署** | Docker 镜像 (`logmind/logmind:latest`) | 基于 Debian slim，预装常用插件 |
| **边缘设备** | 压缩二进制发行包 | 静态链接关键依赖，减少根文件系统需求 |
| **定制发行版** | Yocto / Buildroot 集成 | 支持嵌入式系统和定制 Linux 发行版 |

#### 11.4.2 系统服务配置

**systemd 服务文件** (`/etc/systemd/system/logmind-daemon.service`)：

```ini
[Unit]
Description=LogMind Intelligent Log Analysis Daemon
After=network.target local-fs.target
Wants=network-online.target

[Service]
Type=notify
ExecStart=/usr/bin/logmind-daemon --config /etc/logmind/config.json
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5s
StartLimitInterval=60s
StartLimitBurst=3

# 资源限制
MemoryMax=1G
CPUQuota=80%
LimitNOFILE=65535
LimitNPROC=64

# 安全加固
PrivateTmp=yes
ProtectSystem=full
ProtectHome=yes
NoNewPrivileges=yes
CapabilityBoundingSet=CAP_SYS_PTRACE CAP_SYS_ADMIN
SystemCallFilter=@system-service

[Install]
WantedBy=multi-user.target
```

#### 11.4.3 日志与监控

| 监控项 | 采集方式 | 告警阈值 |
|--------|----------|----------|
| daemon 内存使用 | systemd + D-Bus `GetStats` | > 800MB 持续 5分钟 |
| 处理吞吐量 (QPS) | D-Bus `GetStats` | < 1000 行/秒 持续 10分钟 |
| 插件错误率 | 插件加载日志 | 单个插件连续 3次加载失败 |
| D-Bus 连接数 | 客户端监控 | 单客户端 > 50个并发连接 |
| 日志延迟 | 采集时间 vs 系统时间 | > 30秒 持续 1分钟 |
| 磁盘使用率 | 挂载点监控 | > 85% 警告，> 95% 只读模式 |

#### 11.4.4 运维操作手册

**日常运维**：
- 查看状态：`systemctl status logmind-daemon`
- 查看日志：`journalctl -u logmind-daemon -f`
- 重载插件：`gdbus call --session --dest com.logmind.Daemon --object-path /com/logmind/Daemon --method com.logmind.Daemon1.ReloadPlugins`
- 查看插件列表：`gdbus call --session --dest com.logmind.Daemon --object-path /com/logmind/Daemon --method com.logmind.Daemon1.ListPlugins`

**故障排查**：
1. 检查 daemon 是否运行：`systemctl is-active logmind-daemon`
2. 查看最近错误：`journalctl -u logmind-daemon --since "10 minutes ago" | grep -i error`
3. 验证 D-Bus 服务：`busctl --session list | grep logmind`
4. 测试基本查询：`gdbus call --session --dest com.logmind.Daemon --object-path /com/logmind/Daemon --method com.logmind.Daemon1.QueryLogs "ERROR" 5`
5. 检查插件状态：上述 ListPlugins 方法的返回值

**性能调优**：
- 调整插件工作线程数：`/etc/logmind/config.json` 中的 `worker_threads`
- 配置向量存储持久化：开启 `vector_store.persist=true` 并设置 `sqlite_db_path`
- 调整文件监听轮询间隔：`file_watcher.poll_interval_ms`（仅在 fallback 模式下生效）
- 配置 AI 超时时间：`ai_analyzer.timeout_seconds`

#### 11.4.5 升级与回滚

**升级流程**：
1. 备份配置：`cp /etc/logmind/config.json /etc/logmind/config.json.bak.$(date +%Y%m%d%H%M%S)`
2. 下载新版本 .deb 包
3. 安装：`dpkg -i logmind_*.deb`（自动处理依赖和服务重启）
4. 验证服务：`systemctl status logmind-daemon`
5. 检查插件兼容性：`logmind-plugin-validator --scan-plugins`

**回滚流程**：
1. 如果有以前版本的 .deb 包：`dpkg -i logmind_<previous_version>.deb`
2. 否则从备份恢复配置并重新安装旧版本
3. 重启服务：`systemctl restart logmind-daemon`

#### 11.4.6 监控面板部署注意事项

- Qt 客户端为可选组件，服务器端可仅运行 daemon
- 客户端通过环境变量 `LOGMIND_DAEMON_ADDRESS` 指定 D-Bus 地址（默认 session bus）
- 在头less服务器上运行客户端时，需要配置虚拟显示或使用 VNC
- 建议在专用监控工作站或通过远程桌面访问客户端
