#include <filesystem>
#include <iostream>
#include <vector>

#include "sqluna/sqluna.h"

struct User {
    std::int64_t id = 0;
    std::string name;
    std::int64_t age = 0;
};

SQLUNA_MODEL(User, "users", SQLUNA_FIELD(User, id), SQLUNA_FIELD(User, name), SQLUNA_FIELD(User, age))

int main() {
    namespace qb = sqluna::query::builder;
    namespace schema = sqluna::orm::schema;

    const std::filesystem::path database_path = std::filesystem::current_path() / "sqluna_demo.db";

    sqluna::core::config::SQLiteConfig config;
    config.database_path = database_path.string();
    config.pool_size = 2;

    auto db = qb::Database::sqlite(config);
    schema::Migrator migrator(db);

    migrator.apply(schema::CreateTableMigration{
        "users",
        {
            {"id", schema::ColumnType::Integer, true, true, true},
            {"name", schema::ColumnType::Text, false, true, false},
            {"age", schema::ColumnType::Integer, false, true, false},
        },
        true,
    });

    db.table("users").insert({
        qb::field("name", "Ada"),
        qb::field("age", 36),
    });
    db.table("users").insert({
        qb::field("name", "Linus"),
        qb::field("age", 22),
    });

    const auto rows = db.table("users")
                          .select("id", "name", "age")
                          .where("age", ">", 18)
                          .limit(10)
                          .get();

    const auto users = sqluna::orm::mapper::map_all<User>(rows);
    for (const auto& user : users) {
        std::cout << user.id << " | " << user.name << " | " << user.age << '\n';
    }

    return 0;
}
