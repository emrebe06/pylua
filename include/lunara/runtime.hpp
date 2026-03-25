#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lunara {
class Interpreter;
}

namespace lunara::runtime {

class RuntimeError : public std::runtime_error {
  public:
    explicit RuntimeError(const std::string& message) : std::runtime_error(message) {}
};

class Callable;
struct ListData;
struct ObjectData;

class Value {
  public:
    using Storage =
        std::variant<std::monostate, double, bool, std::string, std::shared_ptr<ListData>, std::shared_ptr<ObjectData>,
                     std::shared_ptr<Callable>>;

    Value();
    explicit Value(double number);
    explicit Value(bool boolean);
    explicit Value(std::string string_value);
    explicit Value(const char* string_value);
    explicit Value(std::shared_ptr<ListData> list_value);
    explicit Value(std::shared_ptr<ObjectData> object_value);
    explicit Value(std::shared_ptr<Callable> callable);

    bool is_nil() const;
    bool is_truthy() const;
    bool is_number() const;
    bool is_string() const;
    bool is_list() const;
    bool is_object() const;
    bool is_callable() const;

    double as_number() const;
    const std::string& as_string() const;
    std::shared_ptr<ListData> as_list() const;
    std::shared_ptr<ObjectData> as_object() const;
    std::shared_ptr<Callable> as_callable() const;

    std::string type_name() const;
    std::string to_string() const;

    const Storage& storage() const;

  private:
    Storage data_;
};

struct ListData {
    std::vector<Value> items;
};

struct ObjectData {
    std::map<std::string, Value> fields;
};

bool operator==(const Value& lhs, const Value& rhs);
bool operator!=(const Value& lhs, const Value& rhs);

class Callable {
  public:
    virtual ~Callable() = default;
    virtual std::size_t arity() const = 0;
    virtual Value call(Interpreter& interpreter, const std::vector<Value>& arguments) = 0;
    virtual std::string debug_name() const = 0;
};

struct Binding {
    Value value;
    bool is_const = false;
    std::optional<std::string> type_hint;
};

class Environment : public std::enable_shared_from_this<Environment> {
  public:
    explicit Environment(std::shared_ptr<Environment> enclosing = nullptr);

    void define(const std::string& name, const Value& value, bool is_const, std::optional<std::string> type_hint = std::nullopt);
    void assign(const std::string& name, const Value& value);
    Value get(const std::string& name) const;
    std::optional<std::string> type_hint_for(const std::string& name) const;
    std::map<std::string, Value> exported_values() const;

  private:
    std::unordered_map<std::string, Binding> values_;
    std::shared_ptr<Environment> enclosing_;
};

}  // namespace lunara::runtime

