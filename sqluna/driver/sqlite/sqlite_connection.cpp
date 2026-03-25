#include "sqluna/driver/sqlite/sqlite_connection.h"

#include <memory>
#include <utility>

#include "sqluna/utils/error/error.h"

namespace sqluna::driver::sqlite {

namespace {

class Statement final {
  public:
    Statement(sqlite3* handle, const SQLiteApi& api, const core::PreparedQuery& query) : api_(api) {
        if (api_.prepare_v2(handle, query.sql.c_str(), static_cast<int>(query.sql.size()), &statement_, nullptr) != kSqliteOk) {
            throw utils::error::DriverError(std::string("sqlite prepare failed: ") + api_.errmsg(handle));
        }

        for (std::size_t i = 0; i < query.bindings.size(); ++i) {
            const int index = static_cast<int>(i + 1);
            const core::DbValue& value = query.bindings[i];
            if (value.is_null()) {
                if (api_.bind_null(statement_, index) != kSqliteOk) {
                    throw utils::error::DriverError("sqlite bind null failed");
                }
            } else if (value.is_integer()) {
                if (api_.bind_int64(statement_, index, value.as_integer()) != kSqliteOk) {
                    throw utils::error::DriverError("sqlite bind int64 failed");
                }
            } else if (value.is_real()) {
                if (api_.bind_double(statement_, index, value.as_real()) != kSqliteOk) {
                    throw utils::error::DriverError("sqlite bind double failed");
                }
            } else {
                if (api_.bind_text(statement_, index, value.as_text().c_str(), static_cast<int>(value.as_text().size()),
                                   nullptr) != kSqliteOk) {
                    throw utils::error::DriverError("sqlite bind text failed");
                }
            }
        }
    }

    ~Statement() {
        if (statement_ != nullptr) {
            api_.finalize(statement_);
        }
    }

    sqlite3_stmt* get() const { return statement_; }

  private:
    const SQLiteApi& api_;
    sqlite3_stmt* statement_ = nullptr;
};

core::DbValue column_value(sqlite3_stmt* statement, const SQLiteApi& api, int column_index) {
    switch (api.column_type(statement, column_index)) {
        case kSqliteInteger: return core::DbValue(api.column_int64(statement, column_index));
        case kSqliteFloat: return core::DbValue(api.column_double(statement, column_index));
        case kSqliteText: {
            const auto* text = reinterpret_cast<const char*>(api.column_text(statement, column_index));
            return core::DbValue(text == nullptr ? "" : text);
        }
        case kSqliteNull: return core::DbValue();
        default: throw utils::error::DriverError("unsupported SQLite column type");
    }
}

}  // namespace

SQLiteConnection::SQLiteConnection(const core::config::SQLiteConfig& config) : api_(SQLiteApi::instance()) {
    if (api_.open_v2(config.database_path.c_str(), &handle_, kSqliteOpenReadWrite | kSqliteOpenCreate | kSqliteOpenFullMutex,
                     nullptr) != kSqliteOk) {
        throw utils::error::DriverError("sqlite open failed for " + config.database_path);
    }

    if (api_.busy_timeout(handle_, config.busy_timeout_ms) != kSqliteOk) {
        throw utils::error::DriverError("sqlite busy timeout setup failed");
    }

    if (config.enable_foreign_keys) {
        execute(core::PreparedQuery{"PRAGMA foreign_keys = ON;", {}});
    }
}

SQLiteConnection::~SQLiteConnection() {
    if (handle_ != nullptr) {
        api_.close_v2(handle_);
    }
}

void SQLiteConnection::execute(const core::PreparedQuery& query) {
    Statement statement(handle_, api_, query);
    int rc = api_.step(statement.get());
    if (rc != kSqliteDone && rc != kSqliteRow) {
        throw utils::error::DriverError(std::string("sqlite execute failed: ") + api_.errmsg(handle_));
    }
    while (rc == kSqliteRow) {
        rc = api_.step(statement.get());
    }
    if (rc != kSqliteDone) {
        throw utils::error::DriverError(std::string("sqlite execute failed: ") + api_.errmsg(handle_));
    }
}

std::vector<core::ResultRow> SQLiteConnection::query(const core::PreparedQuery& query) {
    Statement statement(handle_, api_, query);
    std::vector<core::ResultRow> rows;

    while (true) {
        const int rc = api_.step(statement.get());
        if (rc == kSqliteDone) {
            break;
        }
        if (rc != kSqliteRow) {
            throw utils::error::DriverError(std::string("sqlite query failed: ") + api_.errmsg(handle_));
        }

        core::ResultRow row;
        const int column_count = api_.column_count(statement.get());
        for (int column_index = 0; column_index < column_count; ++column_index) {
            row.set(api_.column_name(statement.get(), column_index), column_value(statement.get(), api_, column_index));
        }
        rows.push_back(std::move(row));
    }

    return rows;
}

std::int64_t SQLiteConnection::last_insert_rowid() const { return api_.last_insert_rowid(handle_); }

}  // namespace sqluna::driver::sqlite
