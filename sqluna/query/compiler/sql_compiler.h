#pragma once

#include <string>

#include "sqluna/core/connection/connection.h"
#include "sqluna/orm/schema/migration.h"
#include "sqluna/query/ast/query_ast.h"

namespace sqluna::query::compiler {

class SqlCompiler {
  public:
    core::PreparedQuery compile(const ast::SelectQuery& query) const;
    core::PreparedQuery compile(const ast::InsertQuery& query) const;
    core::PreparedQuery compile(const orm::schema::CreateTableMigration& migration) const;
    core::PreparedQuery compile(const orm::schema::AddColumnMigration& migration) const;

  private:
    static std::string comparison_operator_to_sql(ast::ComparisonOperator op);
    static std::string column_type_to_sql(orm::schema::ColumnType type);
};

}  // namespace sqluna::query::compiler
