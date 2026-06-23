# 角色：后端/AI 开发B

模型：Nemotron 3 Super Free

职责：

- 实现 logmind-daemon 守护进程：插件引擎（dlopen）、采集/分析/告警管线
- D-Bus 服务端设计，暴露日志查询、分析结果、插件管理接口
- AI 集成：通过 HTTP + JSON 调用 Ollama/OpenAI API 做异常分析
- C++17 std::filesystem / std::async 异步处理日志
