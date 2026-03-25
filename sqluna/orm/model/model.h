#pragma once

#include <string>
#include <tuple>
#include <type_traits>

namespace sqluna::orm::model {

template <typename Model, typename Member>
struct FieldDefinition {
    const char* name;
    Member Model::*member;
};

template <typename Model, typename Member>
constexpr FieldDefinition<Model, Member> make_field(const char* name, Member Model::*member) {
    return {name, member};
}

template <typename Model>
struct ModelTraits;

template <typename Model, typename = void>
struct has_model_traits : std::false_type {};

template <typename Model>
struct has_model_traits<Model, std::void_t<decltype(ModelTraits<Model>::table_name()), decltype(ModelTraits<Model>::fields())>>
    : std::true_type {};

}  // namespace sqluna::orm::model

#define SQLUNA_FIELD(TYPE, MEMBER) ::sqluna::orm::model::make_field<TYPE>(#MEMBER, &TYPE::MEMBER)

#define SQLUNA_MODEL(TYPE, TABLE_NAME, ...)              \
    namespace sqluna::orm::model {                       \
    template <>                                          \
    struct ModelTraits<TYPE> {                           \
        static std::string table_name() { return TABLE_NAME; } \
        static auto fields() { return std::make_tuple(__VA_ARGS__); } \
    };                                                   \
    }
