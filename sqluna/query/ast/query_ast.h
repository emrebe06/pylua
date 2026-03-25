#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sqluna/core/connection/connection.h"
#include "sqluna/utils/error/error.h"

namespace sqluna::query::ast {

enum class ComparisonOperator {
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Like,
};

inline ComparisonOperator comparison_operator_from_string(std::string_view op) {
    if (op == "=" || op == "==") return ComparisonOperator::Equal;
    if (op == "!=") return ComparisonOperator::NotEqual;
    if (op == ">") return ComparisonOperator::Greater;
    if (op == ">=") return ComparisonOperator::GreaterEqual;
    if (op == "<") return ComparisonOperator::Less;
    if (op == "<=") return ComparisonOperator::LessEqual;
    if (op == "like" || op == "LIKE") return ComparisonOperator::Like;
    throw utils::error::QueryError("unsupported comparison operator: " + std::string(op));
}

struct Condition {
    std::string column;
    ComparisonOperator op = ComparisonOperator::Equal;
    core::DbValue value;
};

struct SelectQuery {
    std::string table;
    std::vector<std::string> fields;
    std::vector<Condition> conditions;
    std::optional<std::size_t> limit;
};

struct InsertQuery {
    std::string table;
    std::vector<std::pair<std::string, core::DbValue>> values;
};

}  // namespace sqluna::query::ast
