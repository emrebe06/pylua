#include "sqluna/utils/logger/logger.h"

#include <iostream>
#include <ostream>

namespace sqluna::utils::logger {

namespace {

const char* level_name(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(Level level) { level_ = level; }

void Logger::set_output(std::ostream& stream) { output_ = &stream; }

void Logger::log(Level level, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& out = output_ ? *output_ : std::clog;
    out << "[SQLUna][" << level_name(level) << "] " << message << '\n';
}

}  // namespace sqluna::utils::logger
