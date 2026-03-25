#pragma once

#include <iosfwd>
#include <mutex>
#include <string_view>

namespace sqluna::utils::logger {

enum class Level {
    Trace = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
};

class Logger {
  public:
    static Logger& instance();

    void set_level(Level level);
    void set_output(std::ostream& stream);
    void log(Level level, std::string_view message);

  private:
    Logger() = default;

    std::mutex mutex_;
    std::ostream* output_ = nullptr;
    Level level_ = Level::Info;
};

}  // namespace sqluna::utils::logger
