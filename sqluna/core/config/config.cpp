#include "sqluna/core/config/config.h"

#include "sqluna/utils/error/error.h"

namespace sqluna::core::config {

void validate(const SQLiteConfig& config) {
    if (config.database_path.empty()) {
        throw utils::error::ConfigError("database path cannot be empty");
    }
    if (config.pool_size == 0) {
        throw utils::error::ConfigError("pool size must be greater than zero");
    }
    if (config.busy_timeout_ms < 0) {
        throw utils::error::ConfigError("busy timeout cannot be negative");
    }
}

}  // namespace sqluna::core::config
