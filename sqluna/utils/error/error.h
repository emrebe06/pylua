#pragma once

#include <stdexcept>
#include <string>

namespace sqluna::utils::error {

class SqlunaError : public std::runtime_error {
  public:
    explicit SqlunaError(const std::string& message) : std::runtime_error(message) {}
};

class ConfigError : public SqlunaError {
  public:
    explicit ConfigError(const std::string& message) : SqlunaError(message) {}
};

class DriverError : public SqlunaError {
  public:
    explicit DriverError(const std::string& message) : SqlunaError(message) {}
};

class QueryError : public SqlunaError {
  public:
    explicit QueryError(const std::string& message) : SqlunaError(message) {}
};

class MappingError : public SqlunaError {
  public:
    explicit MappingError(const std::string& message) : SqlunaError(message) {}
};

}  // namespace sqluna::utils::error
