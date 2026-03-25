#include "sqluna/query/builder/query_builder.h"

#include "sqluna/core/config/config.h"
#include "sqluna/driver/sqlite/sqlite_connection.h"

namespace sqluna::query::builder {

TableBuilder::TableBuilder(Database& database, std::string table_name) : database_(&database) { query_.table = std::move(table_name); }

TableBuilder& TableBuilder::select(std::initializer_list<std::string> fields) {
    query_.fields.assign(fields.begin(), fields.end());
    return *this;
}

TableBuilder& TableBuilder::where(std::string column, std::string op, core::DbValue value) {
    query_.conditions.push_back(ast::Condition{std::move(column), ast::comparison_operator_from_string(op), std::move(value)});
    return *this;
}

TableBuilder& TableBuilder::limit(std::size_t value) {
    query_.limit = value;
    return *this;
}

std::vector<core::ResultRow> TableBuilder::get() const { return database_->query(query_); }

std::optional<core::ResultRow> TableBuilder::first() const {
    ast::SelectQuery query = query_;
    query.limit = 1;
    auto rows = database_->query(query);
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows.front();
}

std::int64_t TableBuilder::insert(std::initializer_list<std::pair<std::string, core::DbValue>> values) const {
    ast::InsertQuery query;
    query.table = query_.table;
    query.values.assign(values.begin(), values.end());
    return database_->execute_insert(query);
}

Database::Database(std::shared_ptr<core::ConnectionPool> pool) : pool_(std::move(pool)) {}

Database Database::sqlite(const core::config::SQLiteConfig& config) {
    core::config::validate(config);
    auto pool = std::make_shared<core::ConnectionPool>(config.pool_size, [config]() {
        return std::make_shared<driver::sqlite::SQLiteConnection>(config);
    });
    return Database(std::move(pool));
}

TableBuilder Database::table(std::string table_name) { return TableBuilder(*this, std::move(table_name)); }

std::vector<core::ResultRow> Database::query(const ast::SelectQuery& query) const {
    auto connection = pool_->acquire();
    return connection->query(compiler_.compile(query));
}

std::int64_t Database::execute_insert(const ast::InsertQuery& query) const {
    auto connection = pool_->acquire();
    connection->execute(compiler_.compile(query));
    return connection->last_insert_rowid();
}

void Database::execute(const core::PreparedQuery& query) const {
    auto connection = pool_->acquire();
    connection->execute(query);
}

}  // namespace sqluna::query::builder
