#pragma once
#include <logmind/plugin.h>
#include <chrono>

namespace logmind {

struct AlertContext {
    std::string             rule_name;
    std::string             severity;
    std::string             summary;
    std::vector<LogEntry>   entries;
    nlohmann::json          extra;
    std::chrono::system_clock::time_point triggered_at;
    std::string source_plugin;
};

class IAlert : public IPlugin {
public:
    ~IAlert() override;
    PluginManifest manifest() const override = 0;

    virtual void send(const AlertContext& ctx) = 0;

    virtual bool supports_ack() const { return false; }
    virtual void ack(const std::string& alert_id) { (void)alert_id; }
};

} // namespace logmind
