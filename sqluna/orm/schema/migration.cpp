#include "sqluna/orm/schema/migration.h"

#include "sqluna/query/builder/query_builder.h"
#include "sqluna/query/compiler/sql_compiler.h"

namespace sqluna::orm::schema {

Migrator::Migrator(query::builder::Database& database) : database_(database) {}

void Migrator::apply(const CreateTableMigration& migration) {
    query::compiler::SqlCompiler compiler;
    database_.execute(compiler.compile(migration));
}

void Migrator::apply(const AddColumnMigration& migration) {
    query::compiler::SqlCompiler compiler;
    database_.execute(compiler.compile(migration));
}

}  // namespace sqluna::orm::schema
