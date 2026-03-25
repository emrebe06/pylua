#pragma once

#include <string>
#include <vector>

namespace sqluna::core {
struct PreparedQuery;
}

namespace sqluna::query::builder {
class Database;
}

namespace sqluna::orm::schema {

enum class ColumnType {
    Integer,
    Real,
    Text,
};

struct ColumnDefinition {
    std::string name;
    ColumnType type = ColumnType::Text;
    bool primary_key = false;
    bool not_null = false;
    bool auto_increment = false;
};

struct CreateTableMigration {
    std::string table;
    std::vector<ColumnDefinition> columns;
    bool if_not_exists = true;
};

struct AddColumnMigration {
    std::string table;
    ColumnDefinition column;
};

class Migrator {
  public:
    explicit Migrator(query::builder::Database& database);

    void apply(const CreateTableMigration& migration);
    void apply(const AddColumnMigration& migration);

  private:
    query::builder::Database& database_;
};

}  // namespace sqluna::orm::schema
