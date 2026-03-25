#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "sqluna/core/connection/connection.h"
#include "sqluna/orm/model/model.h"

namespace sqluna::orm::mapper {

namespace detail {

template <typename Model, typename Member>
void assign_field(Model& model_instance,
                  const core::ResultRow& row,
                  const model::FieldDefinition<Model, Member>& field_definition) {
    model_instance.*(field_definition.member) = row.get<Member>(field_definition.name);
}

template <typename Model, typename Tuple, std::size_t... Indices>
void assign_fields(Model& model_instance, const core::ResultRow& row, const Tuple& tuple, std::index_sequence<Indices...>) {
    (assign_field(model_instance, row, std::get<Indices>(tuple)), ...);
}

}  // namespace detail

template <typename Model>
Model map_row(const core::ResultRow& row) {
    static_assert(model::has_model_traits<Model>::value, "Model must declare SQLUNA_MODEL");
    Model model_instance{};
    const auto fields = model::ModelTraits<Model>::fields();
    detail::assign_fields(model_instance, row, fields,
                          std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});
    return model_instance;
}

template <typename Model>
std::vector<Model> map_all(const std::vector<core::ResultRow>& rows) {
    std::vector<Model> models;
    models.reserve(rows.size());
    for (const auto& row : rows) {
        models.push_back(map_row<Model>(row));
    }
    return models;
}

}  // namespace sqluna::orm::mapper
