#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace sqluna::core {

class DbValue {
  public:
    using Storage = std::variant<std::monostate, std::int64_t, double, std::string>;

    DbValue();
    DbValue(std::nullptr_t);
    DbValue(int value);
    DbValue(std::int64_t value);
    DbValue(double value);
    DbValue(bool value);
    DbValue(std::string value);
    DbValue(const char* value);

    bool is_null() const;
    bool is_integer() const;
    bool is_real() const;
    bool is_text() const;

    std::int64_t as_integer() const;
    double as_real() const;
    const std::string& as_text() const;
    std::string to_string() const;

    const Storage& storage() const;

  private:
    Storage storage_;
};

struct PreparedQuery {
    std::string sql;
    std::vector<DbValue> bindings;
};

class ResultRow {
  public:
    void set(std::string column, DbValue value);
    const DbValue& at(const std::string& column) const;
    const std::map<std::string, DbValue>& columns() const;

    template <typename T>
    T get(const std::string& column) const {
        const DbValue& value = at(column);
        if constexpr (std::is_same_v<T, std::string>) {
            return value.as_text();
        } else if constexpr (std::is_same_v<T, double>) {
            return value.as_real();
        } else if constexpr (std::is_same_v<T, bool>) {
            return value.as_integer() != 0;
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(value.as_integer());
        } else {
            static_assert(sizeof(T) == 0, "Unsupported ResultRow::get<T>() type");
        }
    }

  private:
    std::map<std::string, DbValue> columns_;
};

class Connection {
  public:
    virtual ~Connection() = default;

    virtual void execute(const PreparedQuery& query) = 0;
    virtual std::vector<ResultRow> query(const PreparedQuery& query) = 0;
    virtual std::int64_t last_insert_rowid() const = 0;
};

}  // namespace sqluna::core
