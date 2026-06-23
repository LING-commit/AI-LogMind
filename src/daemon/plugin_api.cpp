#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>

logmind::ICollector::~ICollector() = default;
logmind::ICollector::ParseResult logmind::ICollector::parse_file(const std::string&) {
    return {};
}
logmind::IAnalyzer::~IAnalyzer() = default;
logmind::IAlert::~IAlert() = default;
