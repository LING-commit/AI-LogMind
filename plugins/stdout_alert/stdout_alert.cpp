// 示例告警器：把告警写到 stdout 或指定文件。
//
// 故意保持最简：告警是否触发、去重和限流都由流水线的 AlertRule 负责，
// 插件只管把已经决定要发的告警送出去。
#include <logmind/alert.h>
#include <logmind/version.h>

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

class StdoutAlert : public logmind::IAlert {
public:
    logmind::PluginManifest manifest() const override {
        logmind::PluginManifest m;
        m.name        = "stdout-alert";
        m.version     = "1.0.0";
        m.description = "Writes alerts to stdout or a file, one line each";
        m.author      = "LogMind";
        m.type        = logmind::PluginType::Alert;
        m.license     = "MIT";
        // 会产生外部可见的副作用（写文件），标注出来供守护进程参考
        m.has_side_effects = true;
        return m;
    }

    bool on_load(const nlohmann::json& config) override {
        m_path        = config.value("path", std::string("-"));
        m_max_entries = config.value("max_entries_per_alert", size_t{3});

        if (m_path != "-") {
            m_file.open(m_path, std::ios::app);
            if (!m_file.is_open()) {
                std::cerr << "stdout-alert: cannot open " << m_path << std::endl;
                return false;   // on_load 返回 false 会让守护进程拒绝加载本插件
            }
        }
        return true;
    }

    void on_unload() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) m_file.close();
    }

    void send(const logmind::AlertContext& ctx) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostream& out = m_file.is_open() ? m_file : std::cout;

        out << "[ALERT " << ctx.severity << "] rule=" << ctx.rule_name
            << " entries=" << ctx.entries.size()
            << " :: " << ctx.summary << "\n";

        size_t shown = 0;
        for (const auto& entry : ctx.entries) {
            if (shown++ >= m_max_entries) {
                out << "    ... and " << (ctx.entries.size() - m_max_entries) << " more\n";
                break;
            }
            out << "    " << to_string(entry.level) << " [" << entry.module << "] "
                << entry.message << "\n";
        }
        out.flush();
    }

private:
    std::mutex    m_mutex;
    std::string   m_path = "-";
    std::ofstream m_file;
    size_t        m_max_entries = 3;
};

extern "C" {

LOGMIND_PLUGIN_EXPORT int plugin_abi_version() {
    return logmind::LOGMIND_ABI_VERSION;
}

LOGMIND_PLUGIN_EXPORT logmind::IPlugin* create_plugin(logmind::CreateContext ctx) {
    (void)ctx;
    return new StdoutAlert();
}

LOGMIND_PLUGIN_EXPORT void destroy_plugin(logmind::IPlugin* plugin) {
    delete plugin;
}

} // extern "C"
