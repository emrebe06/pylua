#include "sqluna/core/connection/connection.h"

#include <sstream>

#include "sqluna/utils/error/error.h"

namespace sqluna::core {

DbValue::DbValue() : storage_(std::monostate{}) {}
DbValue::DbValue(std::nullptr_t) : storage_(std::monostate{}) {}
DbValue::DbValue(int value) : storage_(static_cast<std::int64_t>(value)) {}
DbValue::DbValue(std::int64_t value) : storage_(value) {}
DbValue::DbValue(double value) : storage_(value) {}
DbValue::DbValue(bool value) : storage_(static_cast<std::int64_t>(value ? 1 : 0)) {}
DbValue::DbValue(std::string value) : storage_(std::move(value)) {}
DbValue::DbValue(const char* value) : storage_(std::string(value)) {}

bool DbValue::is_null() const { return std::holds_alternative<std::monostate>(storage_); }
bool DbValue::is_integer() const { return std::holds_alternative<std::int64_t>(storage_); }
bool DbValue::is_real() const { return std::holds_alternative<double>(storage_); }
bool DbValue::is_text() const { return std::holds_alternative<std::string>(storage_); }

std::int64_t DbValue::as_integer() const {
    if (!is_integer()) {
        throw utils::error::MappingError("expected integer database value");
    }
    return std::get<std::int64_t>(storage_);
}

double DbValue::as_real() const {
    if (is_real()) {
        return std::get<double>(storage_);
    }
    if (is_integer()) {
        return static_cast<double>(std::get<std::int64_t>(storage_));
    }
    throw utils::error::MappingError("expected numeric database value");
}

const std::string& DbValue::as_text() const {
    if (!is_text()) {
        throw utils::error::MappingError("expected text database value");
    }
    return std::get<std::string>(storage_);
}

std::string DbValue::to_string() const {
    if (is_null()) {
        return "null";
    }
    if (is_integer()) {
        return std::to_string(as_integer());
    }
    if (is_real()) {
        std::ostringstream stream;
        stream << as_real();
        return stream.str();
    }
    return as_text();
}

const DbValue::Storage& DbValue::storage() const { return storage_; }

void ResultRow::set(std::string column, DbValue value) { columns_[std::move(column)] = std::move(value); }

const DbValue& ResultRow::at(const std::string& column) const {
    const auto it = columns_.find(column);
    if (it == columns_.end()) {
        throw utils::error::MappingError("missing column in result row: " + column);
    }
    return it->second;
}

const std::map<std::string, DbValue>& ResultRow::columns() const { return columns_; }

}  // namespace sqluna::core
