#include "sqluna/query/compiler/sql_compiler.h"

#include <sstream>

#include "sqluna/security/sanitizer/input_sanitizer.h"

namespace sqluna::query::compiler {

namespace {

std::string comma_join(const std::vector<std::string>& items) {
    std::ostringstream stream;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        stream << items[i];
    }
    return stream.str();
}

}  // namespace

core::PreparedQuery SqlCompiler::compile(const ast::SelectQuery& query) const {
    core::PreparedQuery prepared;
    std::ostringstream sql;

    const std::string table = security::sanitizer::sanitize_identifier(query.table);
    sql << "SELECT ";
    if (query.fields.empty()) {
        sql << "*";
    } else {
        std::vector<std::string> fields;
        fields.reserve(query.fields.size());
        for (const auto& field : query.fields) {
            fields.push_back(security::sanitizer::sanitize_identifier(field));
        }
        sql << comma_join(fields);
    }
    sql << " FROM " << table;

    if (!query.conditions.empty()) {
        sql << " WHERE ";
        for (std::size_t i = 0; i < query.conditions.size(); ++i) {
            if (i != 0) {
                sql << " AND ";
            }
            sql << security::sanitizer::sanitize_identifier(query.conditions[i].column) << ' '
                << comparison_operator_to_sql(query.conditions[i].op) << " ?";
            prepared.bindings.push_back(query.conditions[i].value);
        }
    }

    if (query.limit.has_value()) {
        sql << " LIMIT " << *query.limit;
    }

    sql << ';';
    prepared.sql = sql.str();
    return prepared;
}

core::PreparedQuery SqlCompiler::compile(const ast::InsertQuery& query) const {
    core::PreparedQuery prepared;
    if (query.values.empty()) {
        throw utils::error::QueryError("insert query requires at least one value");
    }

    std::ostringstream sql;
    sql << "INSERT INTO " << security::sanitizer::sanitize_identifier(query.table) << " (";
    std::vector<std::string> columns;
    columns.reserve(query.values.size());
    for (const auto& [name, value] : query.values) {
        columns.push_back(security::sanitizer::sanitize_identifier(name));
        prepared.bindings.push_back(value);
    }
    sql << comma_join(columns) << ") VALUES (";
    for (std::size_t i = 0; i < query.values.size(); ++i) {
        if (i != 0) {
            sql << ", ";
        }
        sql << '?';
    }
    sql << ");";

    prepared.sql = sql.str();
    return prepared;
}

core::PreparedQuery SqlCompiler::compile(const orm::schema::CreateTableMigration& migration) const {
    if (migration.columns.empty()) {
        throw utils::error::QueryError("create table migration requires columns");
    }

    std::ostringstream sql;
    sql << "CREATE TABLE ";
    if (migration.if_not_exists) {
        sql << "IF NOT EXISTS ";
    }
    sql << security::sanitizer::sanitize_identifier(migration.table) << " (";
    for (std::size_t i = 0; i < migration.columns.size(); ++i) {
        if (i != 0) {
            sql << ", ";
        }
        const auto& column = migration.columns[i];
        sql << security::sanitizer::sanitize_identifier(column.name) << ' ' << column_type_to_sql(column.type);
        if (column.primary_key) {
            sql << " PRIMARY KEY";
        }
        if (column.auto_increment) {
            sql << " AUTOINCREMENT";
        }
        if (column.not_null) {
            sql << " NOT NULL";
        }
    }
    sql << ");";
    return core::PreparedQuery{sql.str(), {}};
}

core::PreparedQuery SqlCompiler::compile(const orm::schema::AddColumnMigration& migration) const {
    const auto& column = migration.column;
    std::ostringstream sql;
    sql << "ALTER TABLE " << security::sanitizer::sanitize_identifier(migration.table) << " ADD COLUMN "
        << security::sanitizer::sanitize_identifier(column.name) << ' ' << column_type_to_sql(column.type);
    if (column.not_null) {
        sql << " NOT NULL";
    }
    sql << ';';
    return core::PreparedQuery{sql.str(), {}};
}

std::string SqlCompiler::comparison_operator_to_sql(ast::ComparisonOperator op) {
    switch (op) {
        case ast::ComparisonOperator::Equal: return "=";
        case ast::ComparisonOperator::NotEqual: return "!=";
        case ast::ComparisonOperator::Greater: return ">";
        case ast::ComparisonOperator::GreaterEqual: return ">=";
        case ast::ComparisonOperator::Less: return "<";
        case ast::ComparisonOperator::LessEqual: return "<=";
        case ast::ComparisonOperator::Like: return "LIKE";
    }
    return "=";
}

std::string SqlCompiler::column_type_to_sql(orm::schema::ColumnType type) {
    switch (type) {
        case orm::schema::ColumnType::Integer: return "INTEGER";
        case orm::schema::ColumnType::Real: return "REAL";
        case orm::schema::ColumnType::Text: return "TEXT";
    }
    return "TEXT";
}

}  // namespace sqluna::query::compiler
