#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sqluna/core/config/config.h"
#include "sqluna/core/pool/connection_pool.h"
#include "sqluna/query/ast/query_ast.h"
#include "sqluna/query/compiler/sql_compiler.h"

namespace sqluna::query::builder {

inline std::pair<std::string, core::DbValue> field(std::string name, core::DbValue value) {
    return {std::move(name), std::move(value)};
}

class Database;

class TableBuilder {
  public:
    TableBuilder(Database& database, std::string table_name);

    TableBuilder& select(std::initializer_list<std::string> fields);

    template <typename... Fields>
    TableBuilder& select(const std::string& first, Fields... rest) {
        query_.fields = {first, std::string(rest)...};
        return *this;
    }

    TableBuilder& where(std::string column, std::string op, core::DbValue value);
    TableBuilder& limit(std::size_t value);

    std::vector<core::ResultRow> get() const;
    std::optional<core::ResultRow> first() const;
    std::int64_t insert(std::initializer_list<std::pair<std::string, core::DbValue>> values) const;

  private:
    Database* database_;
    ast::SelectQuery query_;
};

class Database {
  public:
    explicit Database(std::shared_ptr<core::ConnectionPool> pool);

    static Database sqlite(const core::config::SQLiteConfig& config);

    TableBuilder table(std::string table_name);
    std::vector<core::ResultRow> query(const ast::SelectQuery& query) const;
    std::int64_t execute_insert(const ast::InsertQuery& query) const;
    void execute(const core::PreparedQuery& query) const;

  private:
    std::shared_ptr<core::ConnectionPool> pool_;
    query::compiler::SqlCompiler compiler_;

    friend class TableBuilder;
};

}  // namespace sqluna::query::builder
