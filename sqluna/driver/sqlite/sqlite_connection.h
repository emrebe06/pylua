#pragma once

#include <string>
#include <vector>

#include "sqluna/core/config/config.h"
#include "sqluna/core/connection/connection.h"
#include "sqluna/driver/sqlite/sqlite_api.h"

namespace sqluna::driver::sqlite {

class SQLiteConnection final : public core::Connection {
  public:
    explicit SQLiteConnection(const core::config::SQLiteConfig& config);
    ~SQLiteConnection() override;

    void execute(const core::PreparedQuery& query) override;
    std::vector<core::ResultRow> query(const core::PreparedQuery& query) override;
    std::int64_t last_insert_rowid() const override;

  private:
    sqlite3* handle_ = nullptr;
    const SQLiteApi& api_;
};

}  // namespace sqluna::driver::sqlite
