#pragma once

#include <cstddef>
#include <string>

namespace sqluna::core::config {

struct SQLiteConfig {
    std::string database_path = "sqluna.db";
    std::size_t pool_size = 4;
    int busy_timeout_ms = 5000;
    bool enable_foreign_keys = true;
};

void validate(const SQLiteConfig& config);

}  // namespace sqluna::core::config
