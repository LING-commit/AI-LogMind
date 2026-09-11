#include <logmind/collector.h>
#include <logmind/analyzer.h>
#include <logmind/alert.h>
#include <fstream>
#include <sstream>

logmind::ICollector::~ICollector() = default;

logmind::ICollector::ParseResult logmind::ICollector::parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ParseResult result;
        result.error = "cannot open " + path;
        return result;
    }
    return parse(file);
}

logmind::ICollector::ParseResult logmind::ICollector::parse_line(const std::string& line) {
    std::istringstream stream(line);
    return parse(stream);
}

logmind::IAnalyzer::~IAnalyzer() = default;
logmind::IAlert::~IAlert() = default;
