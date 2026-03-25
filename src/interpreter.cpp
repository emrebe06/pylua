#include "lunara/interpreter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "lunara/lexer.hpp"
#include "lunara/parser.hpp"
#include "sqluna/orm/schema/migration.h"
#include "sqluna/query/builder/query_builder.h"
#include "sqluna/security/sanitizer/input_sanitizer.h"

namespace lunara::runtime {

namespace {

struct ParsedTypeHint {
    std::string name;
    std::vector<ParsedTypeHint> args;
};

void skip_type_spaces(std::string_view text, std::size_t& cursor) {
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
        ++cursor;
    }
}

ParsedTypeHint parse_type_hint_spec(std::string_view text, std::size_t& cursor) {
    skip_type_spaces(text, cursor);
    ParsedTypeHint parsed;
    while (cursor < text.size()) {
        const char ch = text[cursor];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            parsed.name.push_back(ch);
            ++cursor;
            continue;
        }
        break;
    }

    skip_type_spaces(text, cursor);
    if (cursor < text.size() && text[cursor] == '<') {
        ++cursor;
        do {
            parsed.args.push_back(parse_type_hint_spec(text, cursor));
            skip_type_spaces(text, cursor);
            if (cursor < text.size() && text[cursor] == ',') {
                ++cursor;
                continue;
            }
            break;
        } while (cursor < text.size());
        skip_type_spaces(text, cursor);
        if (cursor < text.size() && text[cursor] == '>') {
            ++cursor;
        }
    }

    return parsed;
}

ParsedTypeHint parse_type_hint_spec(const std::string& text) {
    std::size_t cursor = 0;
    return parse_type_hint_spec(std::string_view(text), cursor);
}

bool value_matches_parsed_type(const Value& value, const ParsedTypeHint& parsed) {
    if (parsed.name.empty() || parsed.name == "any") {
        return true;
    }
    if (parsed.name == "nil") return value.is_nil();
    if (parsed.name == "number") return value.is_number();
    if (parsed.name == "bool") return std::holds_alternative<bool>(value.storage());
    if (parsed.name == "string") return value.is_string();
    if (parsed.name == "function") return value.is_callable();
    if (parsed.name == "list") {
        if (!value.is_list()) return false;
        if (parsed.args.empty()) return true;
        const ParsedTypeHint& item_type = parsed.args.front();
        for (const auto& item : value.as_list()->items) {
            if (!value_matches_parsed_type(item, item_type)) {
                return false;
            }
        }
        return true;
    }
    if (parsed.name == "object") {
        if (!value.is_object()) return false;
        if (parsed.args.empty()) return true;
        const ParsedTypeHint& value_type = parsed.args.size() == 1 ? parsed.args.front() : parsed.args.back();
        for (const auto& [key, field_value] : value.as_object()->fields) {
            static_cast<void>(key);
            if (!value_matches_parsed_type(field_value, value_type)) {
                return false;
            }
        }
        return true;
    }
    return true;
}

bool value_matches_type_hint(const Value& value, const std::optional<std::string>& type_hint) {
    if (!type_hint.has_value() || *type_hint == "any") {
        return true;
    }
    return value_matches_parsed_type(value, parse_type_hint_spec(*type_hint));
}

std::string expected_type_name(const std::optional<std::string>& type_hint) {
    return type_hint.has_value() ? *type_hint : "any";
}

}  // namespace

Value::Value() : data_(std::monostate{}) {}
Value::Value(double number) : data_(number) {}
Value::Value(bool boolean) : data_(boolean) {}
Value::Value(std::string string_value) : data_(std::move(string_value)) {}
Value::Value(const char* string_value) : data_(std::string(string_value)) {}
Value::Value(std::shared_ptr<ListData> list_value) : data_(std::move(list_value)) {}
Value::Value(std::shared_ptr<ObjectData> object_value) : data_(std::move(object_value)) {}
Value::Value(std::shared_ptr<Callable> callable) : data_(std::move(callable)) {}

bool Value::is_nil() const { return std::holds_alternative<std::monostate>(data_); }
bool Value::is_truthy() const {
    if (is_nil()) {
        return false;
    }
    if (const auto* boolean = std::get_if<bool>(&data_)) {
        return *boolean;
    }
    return true;
}
bool Value::is_number() const { return std::holds_alternative<double>(data_); }
bool Value::is_string() const { return std::holds_alternative<std::string>(data_); }
bool Value::is_list() const { return std::holds_alternative<std::shared_ptr<ListData>>(data_); }
bool Value::is_object() const { return std::holds_alternative<std::shared_ptr<ObjectData>>(data_); }
bool Value::is_callable() const { return std::holds_alternative<std::shared_ptr<Callable>>(data_); }

double Value::as_number() const {
    if (const auto* number = std::get_if<double>(&data_)) {
        return *number;
    }
    throw RuntimeError("expected a number, got " + type_name());
}

const std::string& Value::as_string() const {
    if (const auto* string_value = std::get_if<std::string>(&data_)) {
        return *string_value;
    }
    throw RuntimeError("expected a string, got " + type_name());
}

std::shared_ptr<ListData> Value::as_list() const {
    if (const auto* list_value = std::get_if<std::shared_ptr<ListData>>(&data_)) {
        return *list_value;
    }
    throw RuntimeError("expected a list, got " + type_name());
}

std::shared_ptr<ObjectData> Value::as_object() const {
    if (const auto* object_value = std::get_if<std::shared_ptr<ObjectData>>(&data_)) {
        return *object_value;
    }
    throw RuntimeError("expected an object, got " + type_name());
}

std::shared_ptr<Callable> Value::as_callable() const {
    if (const auto* callable = std::get_if<std::shared_ptr<Callable>>(&data_)) {
        return *callable;
    }
    throw RuntimeError("expected a callable, got " + type_name());
}

std::string Value::type_name() const {
    if (std::holds_alternative<std::monostate>(data_)) return "nil";
    if (std::holds_alternative<double>(data_)) return "number";
    if (std::holds_alternative<bool>(data_)) return "bool";
    if (std::holds_alternative<std::string>(data_)) return "string";
    if (std::holds_alternative<std::shared_ptr<ListData>>(data_)) return "list";
    if (std::holds_alternative<std::shared_ptr<ObjectData>>(data_)) return "object";
    if (std::holds_alternative<std::shared_ptr<Callable>>(data_)) return "function";
    return "unknown";
}

const Value::Storage& Value::storage() const { return data_; }

bool operator==(const Value& lhs, const Value& rhs) {
    if (lhs.storage().index() != rhs.storage().index()) return false;
    if (lhs.is_nil() && rhs.is_nil()) return true;
    if (lhs.is_number()) return lhs.as_number() == rhs.as_number();
    if (std::holds_alternative<bool>(lhs.storage())) return std::get<bool>(lhs.storage()) == std::get<bool>(rhs.storage());
    if (lhs.is_string()) return lhs.as_string() == rhs.as_string();
    if (lhs.is_list()) return lhs.as_list() == rhs.as_list();
    if (lhs.is_object()) return lhs.as_object() == rhs.as_object();
    if (lhs.is_callable()) return lhs.as_callable() == rhs.as_callable();
    return false;
}

bool operator!=(const Value& lhs, const Value& rhs) { return !(lhs == rhs); }

Environment::Environment(std::shared_ptr<Environment> enclosing) : enclosing_(std::move(enclosing)) {}

void Environment::define(const std::string& name, const Value& value, bool is_const, std::optional<std::string> type_hint) {
    if (!value_matches_type_hint(value, type_hint)) {
        throw RuntimeError("value assigned to '" + name + "' does not match declared type '" + expected_type_name(type_hint) + "'");
    }
    values_[name] = Binding{value, is_const, std::move(type_hint)};
}

void Environment::assign(const std::string& name, const Value& value) {
    const auto it = values_.find(name);
    if (it != values_.end()) {
        if (it->second.is_const) {
            throw RuntimeError("cannot assign to const variable '" + name + "'");
        }
        if (!value_matches_type_hint(value, it->second.type_hint)) {
            throw RuntimeError("value assigned to '" + name + "' does not match declared type '" +
                               expected_type_name(it->second.type_hint) + "'");
        }
        it->second.value = value;
        return;
    }
    if (enclosing_) {
        enclosing_->assign(name, value);
        return;
    }
    throw RuntimeError("undefined variable '" + name + "'");
}

Value Environment::get(const std::string& name) const {
    const auto it = values_.find(name);
    if (it != values_.end()) {
        return it->second.value;
    }
    if (enclosing_) {
        return enclosing_->get(name);
    }
    throw RuntimeError("undefined variable '" + name + "'");
}

std::optional<std::string> Environment::type_hint_for(const std::string& name) const {
    const auto it = values_.find(name);
    if (it != values_.end()) {
        return it->second.type_hint;
    }
    if (enclosing_) {
        return enclosing_->type_hint_for(name);
    }
    return std::nullopt;
}

std::map<std::string, Value> Environment::exported_values() const {
    std::map<std::string, Value> exported;
    for (const auto& [name, binding] : values_) {
        if (!name.empty() && name[0] == '_') {
            continue;
        }
        exported[name] = binding.value;
    }
    return exported;
}

}  // namespace lunara::runtime

namespace lunara {

namespace {

using runtime::Callable;
using runtime::Environment;
using runtime::ListData;
using runtime::ObjectData;
using runtime::RuntimeError;
using runtime::Value;

class ReturnSignal final : public std::exception {
  public:
    explicit ReturnSignal(Value return_value) : value(std::move(return_value)) {}
    Value value;
};

class ThrownSignal final : public std::exception {
  public:
    explicit ThrownSignal(Value thrown_value) : value(std::move(thrown_value)) {}
    Value value;
};

class BreakSignal final : public std::exception {};

class ContinueSignal final : public std::exception {};

class NativeFunction final : public Callable {
  public:
    using Callback = std::function<Value(Interpreter&, const std::vector<Value>&)>;
    NativeFunction(std::string function_name, std::size_t function_arity, Callback function_callback)
        : name_(std::move(function_name)), arity_(function_arity), callback_(std::move(function_callback)) {}
    std::size_t arity() const override { return arity_; }
    Value call(Interpreter& interpreter, const std::vector<Value>& arguments) override { return callback_(interpreter, arguments); }
    std::string debug_name() const override { return "<native " + name_ + ">"; }

  private:
    std::string name_;
    std::size_t arity_;
    Callback callback_;
};

class UserFunction final : public Callable {
  public:
    UserFunction(const ast::FunctionStmt* declaration, std::shared_ptr<Environment> closure)
        : declaration_(declaration), closure_(std::move(closure)) {}
    std::size_t arity() const override { return declaration_->params.size(); }
    Value call(Interpreter& interpreter, const std::vector<Value>& arguments) override;
    std::string debug_name() const override { return "<func " + declaration_->name + ">"; }

  private:
    const ast::FunctionStmt* declaration_;
    std::shared_ptr<Environment> closure_;
};

class LambdaFunction final : public Callable {
  public:
    LambdaFunction(const ast::LambdaExpr* declaration, std::shared_ptr<Environment> closure)
        : declaration_(declaration), closure_(std::move(closure)) {}

    std::size_t arity() const override { return declaration_->params.size(); }
    Value call(Interpreter& interpreter, const std::vector<Value>& arguments) override;
    std::string debug_name() const override { return "<lambda>"; }

  private:
    const ast::LambdaExpr* declaration_;
    std::shared_ptr<Environment> closure_;
};

struct ParsedTypeHint {
    std::string name;
    std::vector<ParsedTypeHint> args;
};

void skip_type_spaces(std::string_view text, std::size_t& cursor) {
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
        ++cursor;
    }
}

ParsedTypeHint parse_type_hint_spec(std::string_view text, std::size_t& cursor) {
    skip_type_spaces(text, cursor);
    ParsedTypeHint parsed;
    while (cursor < text.size()) {
        const char ch = text[cursor];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            parsed.name.push_back(ch);
            ++cursor;
            continue;
        }
        break;
    }

    skip_type_spaces(text, cursor);
    if (cursor < text.size() && text[cursor] == '<') {
        ++cursor;
        do {
            parsed.args.push_back(parse_type_hint_spec(text, cursor));
            skip_type_spaces(text, cursor);
            if (cursor < text.size() && text[cursor] == ',') {
                ++cursor;
                continue;
            }
            break;
        } while (cursor < text.size());
        skip_type_spaces(text, cursor);
        if (cursor < text.size() && text[cursor] == '>') {
            ++cursor;
        }
    }

    return parsed;
}

ParsedTypeHint parse_type_hint_spec(const std::string& text) {
    std::size_t cursor = 0;
    return parse_type_hint_spec(std::string_view(text), cursor);
}

bool value_matches_parsed_type(const Value& value, const ParsedTypeHint& parsed) {
    if (parsed.name.empty() || parsed.name == "any") return true;
    if (parsed.name == "nil") return value.is_nil();
    if (parsed.name == "number") return value.is_number();
    if (parsed.name == "bool") return std::holds_alternative<bool>(value.storage());
    if (parsed.name == "string") return value.is_string();
    if (parsed.name == "function") return value.is_callable();
    if (parsed.name == "list") {
        if (!value.is_list()) return false;
        if (parsed.args.empty()) return true;
        for (const auto& item : value.as_list()->items) {
            if (!value_matches_parsed_type(item, parsed.args.front())) return false;
        }
        return true;
    }
    if (parsed.name == "object") {
        if (!value.is_object()) return false;
        if (parsed.args.empty()) return true;
        const ParsedTypeHint& value_type = parsed.args.size() == 1 ? parsed.args.front() : parsed.args.back();
        for (const auto& [field_name, field_value] : value.as_object()->fields) {
            static_cast<void>(field_name);
            if (!value_matches_parsed_type(field_value, value_type)) return false;
        }
        return true;
    }
    return true;
}

bool matches_type_hint(const Value& value, const ast::TypeHint& type_hint) {
    if (!type_hint.has_value() || *type_hint == "any") {
        return true;
    }
    return value_matches_parsed_type(value, parse_type_hint_spec(*type_hint));
}

void ensure_type_hint(const Value& value, const ast::TypeHint& type_hint, const std::string& context) {
    if (!matches_type_hint(value, type_hint)) {
        throw RuntimeError(context + " expected type '" + *type_hint + "', got '" + value.type_name() + "'");
    }
}

std::string escape_string(const std::string& input) {
    std::ostringstream out;
    for (const char ch : input) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

std::string format_number(double number) {
    std::ostringstream out;
    out << std::setprecision(15) << number;
    std::string text = out.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    return text;
}

std::string render_value(const Value& value, bool json_mode);

std::string render_list(const std::shared_ptr<ListData>& list_value, bool json_mode) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < list_value->items.size(); ++i) {
        if (i > 0) out << ", ";
        out << render_value(list_value->items[i], json_mode);
    }
    out << "]";
    return out.str();
}

std::string render_object(const std::shared_ptr<ObjectData>& object_value, bool json_mode) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& [key, value] : object_value->fields) {
        if (!first) out << ", ";
        first = false;
        out << '"' << escape_string(key) << "\": " << render_value(value, json_mode);
    }
    out << "}";
    return out.str();
}

std::string render_value(const Value& value, bool json_mode) {
    if (value.is_nil()) return json_mode ? "null" : "nil";
    if (const auto* boolean = std::get_if<bool>(&value.storage())) return *boolean ? "true" : "false";
    if (value.is_number()) return format_number(value.as_number());
    if (value.is_string()) return json_mode ? "\"" + escape_string(value.as_string()) + "\"" : value.as_string();
    if (value.is_list()) return render_list(value.as_list(), json_mode);
    if (value.is_object()) return render_object(value.as_object(), json_mode);
    return value.as_callable()->debug_name();
}

std::vector<std::string> split_string(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : value) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    parts.push_back(current);
    return parts;
}

Value lookup_template_value(const Value& data, const std::string& path) {
    Value current = data;
    for (const auto& segment : split_string(path, '.')) {
        if (!current.is_object()) {
            return Value();
        }
        const auto object_value = current.as_object();
        const auto it = object_value->fields.find(segment);
        if (it == object_value->fields.end()) {
            return Value();
        }
        current = it->second;
    }
    return current;
}

std::string render_template_text(const std::string& template_text, const Value& data) {
    std::string output;
    output.reserve(template_text.size());

    std::size_t cursor = 0;
    while (cursor < template_text.size()) {
        const std::size_t open = template_text.find("{{", cursor);
        if (open == std::string::npos) {
            output += template_text.substr(cursor);
            break;
        }

        output += template_text.substr(cursor, open - cursor);
        const std::size_t close = template_text.find("}}", open + 2);
        if (close == std::string::npos) {
            output += template_text.substr(open);
            break;
        }

        std::string key = template_text.substr(open + 2, close - open - 2);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) {
            key.erase(key.begin());
        }
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }

        const Value resolved = lookup_template_value(data, key);
        if (!resolved.is_nil()) {
            output += resolved.is_string() ? resolved.as_string() : resolved.to_string();
        }
        cursor = close + 2;
    }

    return output;
}

Value make_object(std::map<std::string, Value> fields);

Value runtime_error_value(const std::string& message) {
    return make_object({
        {"kind", Value("runtime_error")},
        {"message", Value(message)},
    });
}

Value literal_to_value(const ast::LiteralValue& literal) {
    if (std::holds_alternative<std::monostate>(literal)) return Value();
    if (const auto* number = std::get_if<double>(&literal)) return Value(*number);
    if (const auto* boolean = std::get_if<bool>(&literal)) return Value(*boolean);
    return Value(std::get<std::string>(literal));
}

bool bind_match_pattern(const ast::MatchPattern& pattern, const Value& value, const std::shared_ptr<Environment>& environment) {
    switch (pattern.kind) {
        case ast::MatchPatternKind::Literal:
            return value == literal_to_value(pattern.literal);
        case ast::MatchPatternKind::Wildcard:
            return true;
        case ast::MatchPatternKind::Binding:
            environment->define(pattern.binding_name, value, false);
            return true;
        case ast::MatchPatternKind::List: {
            if (!value.is_list()) return false;
            const auto list_value = value.as_list();
            if (list_value->items.size() != pattern.elements.size()) return false;
            for (std::size_t i = 0; i < pattern.elements.size(); ++i) {
                if (!bind_match_pattern(pattern.elements[i], list_value->items[i], environment)) return false;
            }
            return true;
        }
        case ast::MatchPatternKind::Object: {
            if (!value.is_object()) return false;
            const auto object_value = value.as_object();
            for (const auto& field : pattern.fields) {
                const auto it = object_value->fields.find(field.key);
                if (it == object_value->fields.end()) return false;
                if (!bind_match_pattern(*field.pattern, it->second, environment)) return false;
            }
            return true;
        }
    }
    return false;
}

void require_number_operands(const Token& op, const Value& lhs, const Value& rhs) {
    if (!lhs.is_number() || !rhs.is_number()) throw RuntimeError("operator '" + op.lexeme + "' expects numbers");
}

std::size_t number_to_index(double raw_index) {
    if (raw_index < 0 || std::floor(raw_index) != raw_index) throw RuntimeError("list index must be a non-negative integer");
    return static_cast<std::size_t>(raw_index);
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw RuntimeError("could not open file: " + path.string());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) throw RuntimeError("could not write file: " + path.string());
    output << text;
}

std::optional<std::filesystem::path> find_workspace_root(const std::filesystem::path& start) {
    std::filesystem::path current = std::filesystem::absolute(start);
    if (std::filesystem::is_regular_file(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        if (std::filesystem::exists(current / "lunara.toml")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

std::optional<std::string> package_name_from_manifest(const std::filesystem::path& manifest_path) {
    if (!std::filesystem::exists(manifest_path)) {
        return std::nullopt;
    }
    std::istringstream lines(read_text_file(manifest_path));
    std::string line;
    while (std::getline(lines, line)) {
        const std::size_t name_pos = line.find("name");
        const std::size_t quote_start = line.find('"');
        const std::size_t quote_end = line.find('"', quote_start == std::string::npos ? quote_start : quote_start + 1);
        if (name_pos != std::string::npos && quote_start != std::string::npos && quote_end != std::string::npos && quote_end > quote_start) {
            return line.substr(quote_start + 1, quote_end - quote_start - 1);
        }
    }
    return std::nullopt;
}

struct PackageLockEntry {
    std::string name;
    std::string version;
    std::string path;
    std::string registry;
};

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string unquote_copy(std::string value) {
    value = trim_copy(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<PackageLockEntry> read_lockfile_entries(const std::filesystem::path& lock_path) {
    std::vector<PackageLockEntry> entries;
    if (!std::filesystem::exists(lock_path)) {
        return entries;
    }

    std::istringstream lines(read_text_file(lock_path));
    std::string line;
    PackageLockEntry current;
    bool in_package = false;

    auto flush_current = [&]() {
        if (in_package && !current.name.empty()) {
            entries.push_back(current);
        }
        current = PackageLockEntry{};
    };

    while (std::getline(lines, line)) {
        line = trim_copy(std::move(line));
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "[[package]]") {
            flush_current();
            in_package = true;
            continue;
        }
        const std::size_t equal = line.find('=');
        if (!in_package || equal == std::string::npos) {
            continue;
        }
        const std::string key = trim_copy(line.substr(0, equal));
        const std::string value = unquote_copy(line.substr(equal + 1));
        if (key == "name") current.name = value;
        if (key == "version") current.version = value;
        if (key == "path") current.path = value;
        if (key == "registry") current.registry = value;
    }

    flush_current();
    return entries;
}

std::optional<std::filesystem::path> resolve_module_from_package_base(const std::filesystem::path& package_base,
                                                                      const std::vector<std::string>& segments) {
    std::filesystem::path source_root = package_base / "src";
    if (segments.size() == 1) {
        const auto main_path = source_root / "main.lunara";
        if (std::filesystem::exists(main_path)) return main_path;
        const auto direct_main = package_base / "main.lunara";
        if (std::filesystem::exists(direct_main)) return direct_main;
        return std::nullopt;
    }

    for (std::size_t i = 1; i < segments.size(); ++i) {
        source_root /= segments[i];
    }
    source_root.replace_extension(".lunara");
    if (std::filesystem::exists(source_root)) {
        return source_root;
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> resolve_from_lockfile(const std::filesystem::path& root,
                                                           const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return std::nullopt;
    }

    for (const auto& entry : read_lockfile_entries(root / "lunara.lock")) {
        if (entry.name != segments.front()) {
            continue;
        }

        if (!entry.path.empty()) {
            if (const auto resolved = resolve_module_from_package_base(root / entry.path, segments)) {
                return resolved;
            }
        }

        if (!entry.registry.empty() && !entry.version.empty()) {
            const auto registry_base = root / entry.registry / entry.name / entry.version;
            if (const auto resolved = resolve_module_from_package_base(registry_base, segments)) {
                return resolved;
            }
        }

        if (!entry.version.empty()) {
            const auto default_registry = root / ".lunara" / "registry" / entry.name / entry.version;
            if (const auto resolved = resolve_module_from_package_base(default_registry, segments)) {
                return resolved;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> resolve_from_package_root(const std::filesystem::path& root,
                                                               const std::string& module_name,
                                                               const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return std::nullopt;
    }

    if (const auto locked = resolve_from_lockfile(root, segments)) {
        return locked;
    }

    const auto local_package_name = package_name_from_manifest(root / "lunara.toml");
    if (local_package_name.has_value() && *local_package_name == segments.front()) {
        std::filesystem::path local_path = root / "src";
        if (segments.size() == 1) {
            const auto main_path = local_path / "main.lunara";
            if (std::filesystem::exists(main_path)) return main_path;
        } else {
            for (std::size_t i = 1; i < segments.size(); ++i) local_path /= segments[i];
            local_path.replace_extension(".lunara");
            if (std::filesystem::exists(local_path)) return local_path;
        }
    }

    std::filesystem::path dependency_root = root / "packages" / segments.front();
    if (std::filesystem::exists(dependency_root)) {
        std::filesystem::path package_path = dependency_root / "src";
        if (segments.size() == 1) {
            const auto main_path = package_path / "main.lunara";
            if (std::filesystem::exists(main_path)) return main_path;
            const auto direct_main = dependency_root / "main.lunara";
            if (std::filesystem::exists(direct_main)) return direct_main;
        } else {
            for (std::size_t i = 1; i < segments.size(); ++i) package_path /= segments[i];
            package_path.replace_extension(".lunara");
            if (std::filesystem::exists(package_path)) return package_path;
        }
    }

    static_cast<void>(module_name);
    return std::nullopt;
}

std::string iso_timestamp(bool utc) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    if (utc) gmtime_s(&tm, &tt); else localtime_s(&tm, &tt);
#else
    if (utc) gmtime_r(&tt, &tm); else localtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (utc) out << "Z";
    return out.str();
}

Value make_native_function(const std::string& name, std::size_t arity, NativeFunction::Callback callback) {
    return Value(std::make_shared<NativeFunction>(name, arity, std::move(callback)));
}

Value make_object(std::map<std::string, Value> fields) {
    auto object_value = std::make_shared<ObjectData>();
    object_value->fields = std::move(fields);
    return Value(object_value);
}

Value make_list(std::vector<Value> items) {
    auto list_value = std::make_shared<ListData>();
    list_value->items = std::move(items);
    return Value(list_value);
}

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void close_socket(SocketHandle socket_handle) {
#if defined(_WIN32)
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

std::string socket_error_string() {
#if defined(_WIN32)
    return "winsock error " + std::to_string(WSAGetLastError());
#else
    return std::string(std::strerror(errno));
#endif
}

std::string mime_type_for_path(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".ico") return "image/x-icon";
    return "text/plain; charset=utf-8";
}

std::string status_text(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default: return "Internal Server Error";
    }
}

void send_all(SocketHandle client_socket, const std::string& payload) {
    std::size_t total_sent = 0;
    while (total_sent < payload.size()) {
#if defined(_WIN32)
        const int sent = send(client_socket, payload.data() + total_sent, static_cast<int>(payload.size() - total_sent), 0);
#else
        const int sent = static_cast<int>(send(client_socket, payload.data() + total_sent, payload.size() - total_sent, 0));
#endif
        if (sent <= 0) {
            throw RuntimeError("socket send failed: " + socket_error_string());
        }
        total_sent += static_cast<std::size_t>(sent);
    }
}

std::string build_http_response(int status_code,
                                const std::string& content_type,
                                const std::string& body,
                                const std::map<std::string, std::string>& extra_headers = {}) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text(status_code) << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    for (const auto& [name, value] : extra_headers) {
        response << name << ": " << value << "\r\n";
    }
    response << "Connection: close\r\n\r\n";
    response << body;
    return response.str();
}

bool constant_time_equals(const std::string& lhs, const std::string& rhs) {
    const std::size_t max_size = (std::max)(lhs.size(), rhs.size());
    unsigned char diff = static_cast<unsigned char>(lhs.size() ^ rhs.size());
    for (std::size_t i = 0; i < max_size; ++i) {
        const unsigned char left = i < lhs.size() ? static_cast<unsigned char>(lhs[i]) : 0;
        const unsigned char right = i < rhs.size() ? static_cast<unsigned char>(rhs[i]) : 0;
        diff |= static_cast<unsigned char>(left ^ right);
    }
    return diff == 0;
}

std::string random_token(std::size_t length) {
    static constexpr std::string_view kAlphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<std::size_t> distribution(0, kAlphabet.size() - 1);
    std::string token;
    token.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        token.push_back(kAlphabet[distribution(generator)]);
    }
    return token;
}

std::string hex_encode(const std::uint8_t* bytes, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        encoded.push_back(kHex[(bytes[i] >> 4) & 0x0f]);
        encoded.push_back(kHex[bytes[i] & 0x0f]);
    }
    return encoded;
}

std::string sha256_hex(std::string_view input) {
    static constexpr std::uint32_t kInitial[] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    static constexpr std::uint32_t kConstants[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };

    auto rotr = [](std::uint32_t value, int shift) -> std::uint32_t {
        return (value >> shift) | (value << (32 - shift));
    };

    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) {
        bytes.push_back(0x00);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xff));
    }

    std::uint32_t hash[8];
    std::copy(std::begin(kInitial), std::end(kInitial), std::begin(hash));

    for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        std::uint32_t words[64]{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t index = chunk + static_cast<std::size_t>(i) * 4;
            words[i] = (static_cast<std::uint32_t>(bytes[index]) << 24) |
                       (static_cast<std::uint32_t>(bytes[index + 1]) << 16) |
                       (static_cast<std::uint32_t>(bytes[index + 2]) << 8) |
                       static_cast<std::uint32_t>(bytes[index + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const std::uint32_t s1 = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = hash[0];
        std::uint32_t b = hash[1];
        std::uint32_t c = hash[2];
        std::uint32_t d = hash[3];
        std::uint32_t e = hash[4];
        std::uint32_t f = hash[5];
        std::uint32_t g = hash[6];
        std::uint32_t h = hash[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t choice = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + choice + kConstants[i] + words[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>((hash[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((hash[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((hash[i] >> 8) & 0xff);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(hash[i] & 0xff);
    }
    return hex_encode(digest.data(), digest.size());
}

std::string hash_password(const std::string& password, const std::string& salt = random_token(16)) {
    std::string digest = password + ":" + salt;
    for (int i = 0; i < 2048; ++i) {
        digest = sha256_hex(digest + ":" + password + ":" + salt);
    }
    return "sha256$" + salt + "$" + digest;
}

bool verify_password(const std::string& password, const std::string& stored_hash) {
    const std::size_t first_sep = stored_hash.find('$');
    const std::size_t second_sep = stored_hash.find('$', first_sep == std::string::npos ? first_sep : first_sep + 1);
    if (first_sep == std::string::npos || second_sep == std::string::npos) {
        return false;
    }
    const std::string algorithm = stored_hash.substr(0, first_sep);
    const std::string salt = stored_hash.substr(first_sep + 1, second_sep - first_sep - 1);
    if (algorithm != "sha256" || salt.empty()) {
        return false;
    }
    return constant_time_equals(hash_password(password, salt), stored_hash);
}

std::filesystem::path safe_join_path(const std::filesystem::path& root, const std::string& child) {
    const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root);
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(canonical_root / child);
    const std::string candidate_text = candidate.string();
    const std::string root_text = canonical_root.string();
    if (candidate_text.rfind(root_text, 0) != 0) {
        throw RuntimeError("safe join blocked path traversal");
    }
    return candidate;
}

bool detect_cuda_available() {
#if defined(_WIN32)
    if (const char* cuda_path = std::getenv("CUDA_PATH")) {
        return std::filesystem::exists(cuda_path);
    }
    return std::filesystem::exists("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA");
#else
    if (const char* cuda_path = std::getenv("CUDA_PATH")) {
        return std::filesystem::exists(cuda_path);
    }
    return std::filesystem::exists("/usr/local/cuda");
#endif
}

Value build_capabilities_object() {
    const bool cuda_available = detect_cuda_available();
    const double cpu_threads = static_cast<double>(std::thread::hardware_concurrency());
    return make_object({
        {"cpu_threads", Value(cpu_threads)},
        {"cpu_available", Value(true)},
        {"gpu_available", Value(cuda_available)},
        {"cuda_available", Value(cuda_available)},
        {"preferred_backend", Value(cuda_available ? "cuda" : "cpu")},
    });
}

std::vector<double> list_to_numbers(const Value& value, const std::string& name) {
    if (!value.is_list()) {
        throw RuntimeError(name + " must be a list");
    }
    std::vector<double> numbers;
    numbers.reserve(value.as_list()->items.size());
    for (const auto& item : value.as_list()->items) {
        numbers.push_back(item.as_number());
    }
    return numbers;
}

Value numbers_to_list(const std::vector<double>& numbers) {
    std::vector<Value> items;
    items.reserve(numbers.size());
    for (const double value : numbers) {
        items.emplace_back(value);
    }
    return make_list(std::move(items));
}

Value decode_json_text(const std::string& text);

struct HttpRequestData {
    std::string method;
    std::string target;
    std::string path;
    std::string version;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query;
};

struct HttpResponseData {
    int status = 200;
    std::string content_type = "text/plain; charset=utf-8";
    std::string body;
    std::map<std::string, std::string> headers;
};

struct RouteEntry {
    std::string pattern;
    Value handler;
};

struct StaticMount {
    std::string prefix;
    std::filesystem::path root;
};

struct WebSocketHandlers {
    Value on_open;
    Value on_message;
    Value on_close;
};

struct WebSocketRoute {
    std::string pattern;
    WebSocketHandlers handlers;
};

struct RouterState {
    std::map<std::string, std::vector<RouteEntry>> routes;
    std::vector<Value> middlewares;
    std::vector<StaticMount> static_mounts;
    std::vector<WebSocketRoute> websocket_routes;
    std::optional<std::string> cors_origin;
};

struct RouteMatch {
    Value handler;
    std::map<std::string, std::string> params;
};

struct WebSocketRouteMatch {
    WebSocketHandlers handlers;
    std::map<std::string, std::string> params;
};

bool is_websocket_upgrade(const HttpRequestData& request);
void send_websocket_frame(SocketHandle socket_handle, std::uint8_t opcode, const std::string& payload);
void run_websocket_connection(Interpreter& interpreter,
                              SocketHandle client_socket,
                              const HttpRequestData& request,
                              const WebSocketHandlers& handlers,
                              std::map<std::string, std::string> params = {});

std::map<std::string, std::string> parse_query_map(const std::string& target) {
    std::map<std::string, std::string> query;
    const std::size_t query_pos = target.find('?');
    if (query_pos == std::string::npos || query_pos + 1 >= target.size()) {
        return query;
    }

    std::istringstream stream(target.substr(query_pos + 1));
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        if (pair.empty()) {
            continue;
        }
        const std::size_t equal_pos = pair.find('=');
        if (equal_pos == std::string::npos) {
            query[pair] = "";
        } else {
            query[pair.substr(0, equal_pos)] = pair.substr(equal_pos + 1);
        }
    }
    return query;
}

std::string strip_query_string(const std::string& target) {
    const std::size_t query_pos = target.find('?');
    if (query_pos == std::string::npos) {
        return target;
    }
    return target.substr(0, query_pos);
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::map<std::string, std::string> parse_cookie_map(const HttpRequestData& request) {
    std::map<std::string, std::string> cookies;
    const auto cookie_it = request.headers.find("cookie");
    if (cookie_it == request.headers.end()) {
        return cookies;
    }

    std::istringstream stream(cookie_it->second);
    std::string entry;
    while (std::getline(stream, entry, ';')) {
        const auto equal_pos = entry.find('=');
        if (equal_pos == std::string::npos) {
            continue;
        }

        std::string key = entry.substr(0, equal_pos);
        std::string value = entry.substr(equal_pos + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) {
            key.erase(key.begin());
        }
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        cookies[key] = value;
    }

    return cookies;
}

std::vector<std::string> split_path_segments(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (const char ch : path) {
        if (ch == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

std::string normalize_route_path(const std::string& path) {
    if (path.empty() || path == "/") {
        return "/";
    }
    std::string normalized = path;
    if (normalized.front() != '/') {
        normalized.insert(normalized.begin(), '/');
    }
    while (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::optional<std::map<std::string, std::string>> match_route_pattern(const std::string& pattern, const std::string& path) {
    const std::vector<std::string> pattern_segments = split_path_segments(normalize_route_path(pattern));
    const std::vector<std::string> path_segments = split_path_segments(normalize_route_path(path));
    if (pattern_segments.size() != path_segments.size()) {
        return std::nullopt;
    }

    std::map<std::string, std::string> params;
    for (std::size_t i = 0; i < pattern_segments.size(); ++i) {
        const std::string& pattern_segment = pattern_segments[i];
        const std::string& path_segment = path_segments[i];
        if (!pattern_segment.empty() && pattern_segment.front() == ':') {
            params[pattern_segment.substr(1)] = path_segment;
            continue;
        }
        if (pattern_segment != path_segment) {
            return std::nullopt;
        }
    }

    return params;
}

std::optional<RouteMatch> find_route(const RouterState& router_state, const std::string& method, const std::string& path) {
    const auto routes_it = router_state.routes.find(method);
    if (routes_it == router_state.routes.end()) {
        return std::nullopt;
    }

    const std::string normalized_path = normalize_route_path(path);
    for (const auto& route : routes_it->second) {
        if (const auto params = match_route_pattern(route.pattern, normalized_path)) {
            return RouteMatch{route.handler, *params};
        }
    }

    return std::nullopt;
}

std::optional<WebSocketRouteMatch> find_websocket_route(const RouterState& router_state, const std::string& path) {
    const std::string normalized_path = normalize_route_path(path);
    for (const auto& route : router_state.websocket_routes) {
        if (const auto params = match_route_pattern(route.pattern, normalized_path)) {
            return WebSocketRouteMatch{route.handlers, *params};
        }
    }
    return std::nullopt;
}

std::map<std::string, std::string> build_cors_headers(const HttpRequestData& request, const RouterState* router_state = nullptr) {
    std::map<std::string, std::string> headers;
    const auto origin_it = request.headers.find("origin");
    std::string allow_origin = origin_it != request.headers.end() ? origin_it->second : "*";
    if (router_state != nullptr && router_state->cors_origin.has_value()) {
        allow_origin = *router_state->cors_origin;
    }
    headers["Access-Control-Allow-Origin"] = allow_origin;
    headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, PATCH, DELETE, OPTIONS";
    if (const auto requested_headers = request.headers.find("access-control-request-headers");
        requested_headers != request.headers.end() && !requested_headers->second.empty()) {
        headers["Access-Control-Allow-Headers"] = requested_headers->second;
    } else {
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
    }
    headers["Access-Control-Max-Age"] = "86400";
    headers["Vary"] = "Origin, Access-Control-Request-Headers";
    if (origin_it != request.headers.end()) {
        headers["Access-Control-Allow-Credentials"] = "true";
    }
    return headers;
}

void merge_http_headers(std::map<std::string, std::string>& target, const std::map<std::string, std::string>& extra) {
    for (const auto& [name, value] : extra) {
        target.insert_or_assign(name, value);
    }
}

WebSocketHandlers websocket_handlers_from_value(const Value& value) {
    if (value.is_callable()) {
        return WebSocketHandlers{Value(), value, Value()};
    }
    if (!value.is_object()) {
        throw RuntimeError("websocket handler must be a callable or object");
    }

    const auto object_value = value.as_object();
    auto read_handler = [object_value](const std::string& name) -> Value {
        const auto it = object_value->fields.find(name);
        return it == object_value->fields.end() ? Value() : it->second;
    };

    WebSocketHandlers handlers{read_handler("open"), read_handler("message"), read_handler("close")};
    if (handlers.on_message.is_nil()) {
        throw RuntimeError("websocket route requires a message handler");
    }
    if ((!handlers.on_open.is_nil() && !handlers.on_open.is_callable()) ||
        (!handlers.on_message.is_nil() && !handlers.on_message.is_callable()) ||
        (!handlers.on_close.is_nil() && !handlers.on_close.is_callable())) {
        throw RuntimeError("websocket handlers must be callable");
    }
    return handlers;
}

HttpRequestData parse_http_request(const std::string& request) {
    constexpr std::size_t kMaxRequestBytes = 64 * 1024;
    if (request.size() > kMaxRequestBytes) {
        throw RuntimeError("request too large");
    }

    const std::size_t header_end = request.find("\r\n\r\n");
    const std::string header_text = request.substr(0, header_end);
    const std::string body = header_end == std::string::npos ? "" : request.substr(header_end + 4);

    std::istringstream lines(header_text);
    std::string request_line;
    if (!std::getline(lines, request_line)) {
        throw RuntimeError("invalid http request");
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    HttpRequestData parsed;
    std::istringstream request_stream(request_line);
    request_stream >> parsed.method >> parsed.target >> parsed.version;
    if (parsed.method.empty() || parsed.target.empty()) {
        throw RuntimeError("invalid http request line");
    }
    parsed.path = strip_query_string(parsed.target);
    parsed.query = parse_query_map(parsed.target);
    parsed.body = body;

    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const std::size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        const std::string key = lower_ascii(line.substr(0, colon_pos));
        std::string value = line.substr(colon_pos + 1);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        parsed.headers[key] = value;
    }

    return parsed;
}

Value request_to_value(const HttpRequestData& request, std::map<std::string, std::string> params = {}) {
    std::map<std::string, Value> query_fields;
    for (const auto& [key, value] : request.query) {
        query_fields[key] = Value(value);
    }

    std::map<std::string, Value> header_fields;
    for (const auto& [key, value] : request.headers) {
        header_fields[key] = Value(value);
    }

    std::map<std::string, Value> cookie_fields;
    for (const auto& [key, value] : parse_cookie_map(request)) {
        cookie_fields[key] = Value(value);
    }

    std::map<std::string, Value> param_fields;
    for (auto& [key, value] : params) {
        param_fields[key] = Value(value);
    }

    std::map<std::string, Value> fields = {
        {"method", Value(request.method)},
        {"target", Value(request.target)},
        {"path", Value(normalize_route_path(request.path))},
        {"version", Value(request.version)},
        {"body", Value(request.body)},
        {"query", make_object(std::move(query_fields))},
        {"headers", make_object(std::move(header_fields))},
        {"cookies", make_object(std::move(cookie_fields))},
        {"params", make_object(std::move(param_fields))},
    };

    const auto header_it = request.headers.find("content-type");
    if (header_it != request.headers.end() && header_it->second.find("application/json") != std::string::npos &&
        !request.body.empty()) {
        try {
            fields["json"] = decode_json_text(request.body);
        } catch (const std::exception&) {
            fields["json"] = Value();
        }
    } else {
        fields["json"] = Value();
    }

    return make_object(std::move(fields));
}

Value make_http_response_value(int status_code, Value body_value, std::string content_type,
                               std::map<std::string, Value> header_values = {}) {
    header_values["_response"] = Value(true);
    header_values["status"] = Value(static_cast<double>(status_code));
    header_values["body"] = std::move(body_value);
    header_values["content_type"] = Value(std::move(content_type));
    return make_object(std::move(header_values));
}

bool is_http_response_object(const Value& value) {
    if (!value.is_object()) {
        return false;
    }
    const auto object_value = value.as_object();
    const auto it = object_value->fields.find("_response");
    return it != object_value->fields.end() && it->second.is_truthy();
}

std::map<std::string, std::string> extract_http_headers(const Value& value) {
    std::map<std::string, std::string> headers;
    if (!value.is_object()) {
        return headers;
    }
    const auto object_value = value.as_object();
    const auto it = object_value->fields.find("headers");
    if (it == object_value->fields.end() || !it->second.is_object()) {
        return headers;
    }
    for (const auto& [name, header_value] : it->second.as_object()->fields) {
        headers[name] = header_value.to_string();
    }
    return headers;
}

HttpResponseData normalize_http_response(const Value& value) {
    if (value.is_nil()) {
        return HttpResponseData{204, "text/plain; charset=utf-8", "", {}};
    }
    if (is_http_response_object(value)) {
        const auto object_value = value.as_object();
        HttpResponseData response;
        if (const auto status_it = object_value->fields.find("status"); status_it != object_value->fields.end()) {
            response.status = static_cast<int>(status_it->second.as_number());
        }
        if (const auto type_it = object_value->fields.find("content_type"); type_it != object_value->fields.end()) {
            response.content_type = type_it->second.as_string();
        }
        if (const auto body_it = object_value->fields.find("body"); body_it != object_value->fields.end()) {
            response.body = body_it->second.is_string() ? body_it->second.as_string() : render_value(body_it->second, true);
        }
        response.headers = extract_http_headers(value);
        return response;
    }
    if (value.is_string()) {
        return HttpResponseData{200, "text/plain; charset=utf-8", value.as_string(), {}};
    }
    return HttpResponseData{200, "application/json; charset=utf-8", render_value(value, true), {}}; 
}

struct AuthUserRecord {
    std::int64_t id = 0;
    std::string username;
    std::string password_hash;
    std::string role;
    bool email_verified = false;
    std::string created_at;
};

struct AuthSessionRecord {
    std::int64_t id = 0;
    std::int64_t user_id = 0;
    std::string token;
    std::string created_at;
};

struct AuthTokenRecord {
    std::int64_t id = 0;
    std::int64_t user_id = 0;
    std::string token;
    std::string created_at;
};

AuthUserRecord map_auth_user_row(const sqluna::core::ResultRow& row) {
    return AuthUserRecord{
        row.get<std::int64_t>("id"),
        row.get<std::string>("username"),
        row.get<std::string>("password_hash"),
        row.get<std::string>("role"),
        row.get<std::int64_t>("email_verified") != 0,
        row.get<std::string>("created_at"),
    };
}

AuthSessionRecord map_auth_session_row(const sqluna::core::ResultRow& row) {
    return AuthSessionRecord{
        row.get<std::int64_t>("id"),
        row.get<std::int64_t>("user_id"),
        row.get<std::string>("token"),
        row.get<std::string>("created_at"),
    };
}

AuthTokenRecord map_auth_token_row(const sqluna::core::ResultRow& row) {
    return AuthTokenRecord{
        row.get<std::int64_t>("id"),
        row.get<std::int64_t>("user_id"),
        row.get<std::string>("token"),
        row.get<std::string>("created_at"),
    };
}

Value auth_user_value(const AuthUserRecord& user) {
    return make_object({
        {"id", Value(static_cast<double>(user.id))},
        {"username", Value(user.username)},
        {"role", Value(user.role)},
        {"email_verified", Value(user.email_verified)},
        {"created_at", Value(user.created_at)},
    });
}

Value auth_session_value(const AuthSessionRecord& session) {
    return make_object({
        {"id", Value(static_cast<double>(session.id))},
        {"user_id", Value(static_cast<double>(session.user_id))},
        {"token", Value(session.token)},
        {"created_at", Value(session.created_at)},
    });
}

Value auth_token_value(const AuthTokenRecord& token) {
    return make_object({
        {"id", Value(static_cast<double>(token.id))},
        {"user_id", Value(static_cast<double>(token.user_id))},
        {"token", Value(token.token)},
        {"created_at", Value(token.created_at)},
    });
}

sqluna::core::DbValue to_db_value(const Value& value) {
    if (value.is_nil()) {
        return sqluna::core::DbValue();
    }
    if (value.is_number()) {
        const double raw = value.as_number();
        if (std::floor(raw) == raw) {
            return sqluna::core::DbValue(static_cast<std::int64_t>(raw));
        }
        return sqluna::core::DbValue(raw);
    }
    if (const auto* boolean = std::get_if<bool>(&value.storage())) {
        return sqluna::core::DbValue(*boolean);
    }
    if (value.is_string()) {
        return sqluna::core::DbValue(value.as_string());
    }
    throw RuntimeError("sqluna bridge only supports nil, bool, number, and string values");
}

Value from_db_value(const sqluna::core::DbValue& value) {
    if (value.is_null()) {
        return Value();
    }
    if (value.is_integer()) {
        return Value(static_cast<double>(value.as_integer()));
    }
    if (value.is_real()) {
        return Value(value.as_real());
    }
    return Value(value.as_text());
}

Value row_to_value(const sqluna::core::ResultRow& row) {
    std::map<std::string, Value> fields;
    for (const auto& [name, value] : row.columns()) {
        fields[name] = from_db_value(value);
    }
    return make_object(std::move(fields));
}

std::vector<std::string> field_list_from_value(const Value& value) {
    if (!value.is_list()) {
        throw RuntimeError("sqluna fields must be a list");
    }
    std::vector<std::string> fields;
    for (const auto& item : value.as_list()->items) {
        fields.push_back(item.as_string());
    }
    return fields;
}

std::map<std::string, Value> object_fields_from_value(const Value& value) {
    if (!value.is_object()) {
        throw RuntimeError("sqluna filters/values must be an object");
    }
    return value.as_object()->fields;
}

sqluna::orm::schema::ColumnType column_type_from_value(const Value& value) {
    const std::string type = value.as_string();
    if (type == "integer") {
        return sqluna::orm::schema::ColumnType::Integer;
    }
    if (type == "real") {
        return sqluna::orm::schema::ColumnType::Real;
    }
    if (type == "text") {
        return sqluna::orm::schema::ColumnType::Text;
    }
    throw RuntimeError("sqluna column type must be integer, real, or text");
}

sqluna::orm::schema::ColumnDefinition column_definition_from_value(const Value& value) {
    if (!value.is_object()) {
        throw RuntimeError("sqluna column definition must be an object");
    }
    const auto fields = value.as_object()->fields;
    const auto name_it = fields.find("name");
    const auto type_it = fields.find("type");
    if (name_it == fields.end() || type_it == fields.end()) {
        throw RuntimeError("sqluna column definition requires name and type");
    }

    sqluna::orm::schema::ColumnDefinition column;
    column.name = name_it->second.as_string();
    column.type = column_type_from_value(type_it->second);
    if (const auto primary_key_it = fields.find("primary_key"); primary_key_it != fields.end()) {
        column.primary_key = primary_key_it->second.is_truthy();
    }
    if (const auto not_null_it = fields.find("not_null"); not_null_it != fields.end()) {
        column.not_null = not_null_it->second.is_truthy();
    }
    if (const auto auto_increment_it = fields.find("auto_increment"); auto_increment_it != fields.end()) {
        column.auto_increment = auto_increment_it->second.is_truthy();
    }
    return column;
}

std::vector<sqluna::orm::schema::ColumnDefinition> column_definitions_from_value(const Value& value) {
    if (!value.is_list()) {
        throw RuntimeError("sqluna columns must be a list");
    }
    std::vector<sqluna::orm::schema::ColumnDefinition> columns;
    columns.reserve(value.as_list()->items.size());
    for (const auto& item : value.as_list()->items) {
        columns.push_back(column_definition_from_value(item));
    }
    return columns;
}

class SqlunaDatabaseBridge final : public std::enable_shared_from_this<SqlunaDatabaseBridge> {
  public:
    explicit SqlunaDatabaseBridge(std::string database_path) {
        sqluna::core::config::SQLiteConfig config;
        config.database_path = std::move(database_path);
        config.pool_size = 4;
        db_ = std::make_shared<sqluna::query::builder::Database>(sqluna::query::builder::Database::sqlite(config));
    }

    Value make_value() {
        auto self = shared_from_this();
        return make_object({
            {"create_table", make_native_function("sqluna.sqlite.create_table", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->create_table(args[0].as_string(), args[1]);
                 return Value();
             })},
            {"add_column", make_native_function("sqluna.sqlite.add_column", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->add_column(args[0].as_string(), args[1]);
                 return Value();
             })},
            {"migrate", make_native_function("sqluna.sqlite.migrate", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->migrate(args[0]);
                 return Value();
             })},
            {"select", make_native_function("sqluna.sqlite.select", 4, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->select(args[0].as_string(), args[1], args[2], static_cast<std::size_t>(args[3].as_number()));
             })},
            {"first", make_native_function("sqluna.sqlite.first", 3, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->first(args[0].as_string(), args[1], args[2]);
             })},
            {"insert", make_native_function("sqluna.sqlite.insert", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->insert(args[0].as_string(), args[1]);
             })},
            {"update_where", make_native_function("sqluna.sqlite.update_where", 3, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->update_where(args[0].as_string(), args[1], args[2]);
                 return Value();
             })},
            {"delete_where", make_native_function("sqluna.sqlite.delete_where", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->delete_where(args[0].as_string(), args[1]);
                 return Value();
             })},
            {"count", make_native_function("sqluna.sqlite.count", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return Value(static_cast<double>(self->count(args[0].as_string(), args[1])));
             })},
        });
    }

  private:
    static std::string sanitize_identifier(const std::string& identifier) {
        return sqluna::security::sanitizer::sanitize_identifier(identifier);
    }

    void create_table(const std::string& table, const Value& columns_value) {
        sqluna::orm::schema::Migrator migrator(*db_);
        migrator.apply(sqluna::orm::schema::CreateTableMigration{
            sanitize_identifier(table),
            column_definitions_from_value(columns_value),
            true,
        });
    }

    void add_column(const std::string& table, const Value& column_value) {
        sqluna::orm::schema::Migrator migrator(*db_);
        migrator.apply(sqluna::orm::schema::AddColumnMigration{
            sanitize_identifier(table),
            column_definition_from_value(column_value),
        });
    }

    void migrate(const Value& spec_value) {
        if (!spec_value.is_object()) {
            throw RuntimeError("sqluna migration spec must be an object");
        }
        const auto spec = spec_value.as_object()->fields;
        const auto table_it = spec.find("table");
        if (table_it == spec.end()) {
            throw RuntimeError("sqluna migration spec requires table");
        }

        if (const auto columns_it = spec.find("columns"); columns_it != spec.end()) {
            create_table(table_it->second.as_string(), columns_it->second);
            return;
        }
        if (const auto column_it = spec.find("column"); column_it != spec.end()) {
            add_column(table_it->second.as_string(), column_it->second);
            return;
        }
        throw RuntimeError("sqluna migration spec requires columns or column");
    }

    sqluna::query::ast::SelectQuery build_select_query(const std::string& table,
                                                       const Value& fields_value,
                                                       const Value& filters_value,
                                                       std::optional<std::size_t> limit) const {
        sqluna::query::ast::SelectQuery query;
        query.table = sanitize_identifier(table);
        query.fields = field_list_from_value(fields_value);
        const auto filters = object_fields_from_value(filters_value);
        for (const auto& [name, value] : filters) {
            query.conditions.push_back({name, sqluna::query::ast::ComparisonOperator::Equal, to_db_value(value)});
        }
        query.limit = limit;
        return query;
    }

    Value select(const std::string& table, const Value& fields_value, const Value& filters_value, std::size_t limit) const {
        const auto rows = db_->query(build_select_query(table, fields_value, filters_value, limit));
        std::vector<Value> items;
        items.reserve(rows.size());
        for (const auto& row : rows) {
            items.push_back(row_to_value(row));
        }
        return make_list(std::move(items));
    }

    Value first(const std::string& table, const Value& fields_value, const Value& filters_value) const {
        const auto rows = db_->query(build_select_query(table, fields_value, filters_value, 1));
        return rows.empty() ? Value() : row_to_value(rows.front());
    }

    Value insert(const std::string& table, const Value& values_value) {
        const auto values_map = object_fields_from_value(values_value);
        std::vector<std::pair<std::string, sqluna::core::DbValue>> pairs;
        pairs.reserve(values_map.size());
        for (const auto& [name, value] : values_map) {
            pairs.push_back({name, to_db_value(value)});
        }
        sqluna::query::ast::InsertQuery query;
        query.table = sanitize_identifier(table);
        query.values = std::move(pairs);
        return Value(static_cast<double>(db_->execute_insert(query)));
    }

    void update_where(const std::string& table, const Value& values_value, const Value& filters_value) {
        const auto values_map = object_fields_from_value(values_value);
        if (values_map.empty()) {
            throw RuntimeError("sqluna update_where requires at least one value");
        }

        std::ostringstream sql;
        std::vector<sqluna::core::DbValue> bindings;
        sql << "UPDATE " << sanitize_identifier(table) << " SET ";
        bool first_assignment = true;
        for (const auto& [name, value] : values_map) {
            if (!first_assignment) {
                sql << ", ";
            }
            first_assignment = false;
            sql << sanitize_identifier(name) << " = ?";
            bindings.push_back(to_db_value(value));
        }

        const auto filters = object_fields_from_value(filters_value);
        if (!filters.empty()) {
            sql << " WHERE ";
            bool first_condition = true;
            for (const auto& [name, value] : filters) {
                if (!first_condition) {
                    sql << " AND ";
                }
                first_condition = false;
                sql << sanitize_identifier(name) << " = ?";
                bindings.push_back(to_db_value(value));
            }
        }

        sql << ";";
        db_->execute(sqluna::core::PreparedQuery{sql.str(), bindings});
    }

    void delete_where(const std::string& table, const Value& filters_value) {
        std::ostringstream sql;
        sql << "DELETE FROM " << sanitize_identifier(table);
        std::vector<sqluna::core::DbValue> bindings;
        const auto filters = object_fields_from_value(filters_value);
        if (!filters.empty()) {
            sql << " WHERE ";
            bool first_condition = true;
            for (const auto& [name, value] : filters) {
                if (!first_condition) {
                    sql << " AND ";
                }
                first_condition = false;
                sql << sanitize_identifier(name) << " = ?";
                bindings.push_back(to_db_value(value));
            }
        }
        sql << ";";
        db_->execute(sqluna::core::PreparedQuery{sql.str(), bindings});
    }

    std::size_t count(const std::string& table, const Value& filters_value) const {
        const auto rows = db_->query(build_select_query(table, make_list({Value("id")}), filters_value, std::nullopt));
        return rows.size();
    }

    std::shared_ptr<sqluna::query::builder::Database> db_;
};

std::string signed_token_payload(std::int64_t user_id, const std::string& nonce, const std::string& issued_at) {
    return std::to_string(user_id) + ":" + nonce + ":" + issued_at;
}

bool permission_allowed(const std::string& role, const std::string& permission) {
    if (role == "admin") {
        return true;
    }
    if (permission == "catalog.read") {
        return role == "manager" || role == "support" || role == "customer";
    }
    if (permission == "orders.read") {
        return role == "manager" || role == "support";
    }
    if (permission == "orders.write") {
        return role == "manager";
    }
    if (permission == "users.read") {
        return role == "manager" || role == "support";
    }
    if (permission == "reports.view") {
        return role == "manager";
    }
    return false;
}

class SqlunaAuthStore final : public std::enable_shared_from_this<SqlunaAuthStore> {
  public:
    explicit SqlunaAuthStore(std::string database_path) {
        sqluna::core::config::SQLiteConfig config;
        config.database_path = std::move(database_path);
        config.pool_size = 4;
        db_ = std::make_shared<sqluna::query::builder::Database>(sqluna::query::builder::Database::sqlite(config));
        ensure_schema();
    }

    Value make_value() {
        auto self = shared_from_this();
        return make_object({
            {"cookie_name", Value("session")},
            {"register_user", make_native_function("sqluna.auth_store.register_user", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->register_user(args[0].as_string(), args[1].as_string());
             })},
            {"register_with_role", make_native_function("sqluna.auth_store.register_with_role", 3, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->register_user(args[0].as_string(), args[1].as_string(), args[2].as_string());
             })},
            {"authenticate_user", make_native_function("sqluna.auth_store.authenticate_user", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->authenticate_user(args[0].as_string(), args[1].as_string());
             })},
            {"create_session", make_native_function("sqluna.auth_store.create_session", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->create_session(static_cast<std::int64_t>(args[0].as_number()));
             })},
            {"resolve_session", make_native_function("sqluna.auth_store.resolve_session", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->resolve_session(args[0].as_string());
             })},
            {"delete_session", make_native_function("sqluna.auth_store.delete_session", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->delete_session(args[0].as_string());
                 return Value();
             })},
            {"find_user_by_id", make_native_function("sqluna.auth_store.find_user_by_id", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->find_user_by_id(static_cast<std::int64_t>(args[0].as_number()));
             })},
            {"set_role", make_native_function("sqluna.auth_store.set_role", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 self->set_role(static_cast<std::int64_t>(args[0].as_number()), args[1].as_string());
                 return self->find_user_by_id(static_cast<std::int64_t>(args[0].as_number()));
             })},
            {"login", make_native_function("sqluna.auth_store.login", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->login(args[0].as_string(), args[1].as_string());
             })},
            {"session_middleware", make_native_function("sqluna.auth_store.session_middleware", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->attach_session(args[0]);
             })},
            {"role_middleware", make_native_function("sqluna.auth_store.role_middleware", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->make_role_middleware(args[0].as_string());
             })},
            {"permission_middleware", make_native_function("sqluna.auth_store.permission_middleware", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->make_permission_middleware(args[0].as_string());
             })},
            {"require_user", make_native_function("sqluna.auth_store.require_user", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->require_user(args[0]);
             })},
            {"require_role", make_native_function("sqluna.auth_store.require_role", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->require_role(args[0], args[1].as_string());
             })},
            {"require_permission", make_native_function("sqluna.auth_store.require_permission", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->require_permission(args[0], args[1].as_string());
             })},
            {"begin_email_verification", make_native_function("sqluna.auth_store.begin_email_verification", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->begin_email_verification(static_cast<std::int64_t>(args[0].as_number()));
             })},
            {"verify_email", make_native_function("sqluna.auth_store.verify_email", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->verify_email(args[0].as_string());
             })},
            {"begin_password_reset", make_native_function("sqluna.auth_store.begin_password_reset", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->begin_password_reset(args[0].as_string());
             })},
            {"reset_password", make_native_function("sqluna.auth_store.reset_password", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->reset_password(args[0].as_string(), args[1].as_string());
             })},
            {"issue_signed_token", make_native_function("sqluna.auth_store.issue_signed_token", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return Value(self->issue_signed_token(static_cast<std::int64_t>(args[0].as_number()), args[1].as_string()));
             })},
            {"resolve_signed_token", make_native_function("sqluna.auth_store.resolve_signed_token", 2, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->resolve_signed_token(args[0].as_string(), args[1].as_string());
             })},
            {"bearer_middleware", make_native_function("sqluna.auth_store.bearer_middleware", 1, [self](Interpreter&, const std::vector<Value>& args) -> Value {
                 return self->make_bearer_middleware(args[0].as_string());
             })},
        });
    }

  private:
    void ensure_schema() {
        sqluna::orm::schema::Migrator migrator(*db_);
        migrator.apply(sqluna::orm::schema::CreateTableMigration{
            "users",
            {
                {"id", sqluna::orm::schema::ColumnType::Integer, true, true, true},
                {"username", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"password_hash", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"role", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"email_verified", sqluna::orm::schema::ColumnType::Integer, false, true, false},
                {"created_at", sqluna::orm::schema::ColumnType::Text, false, true, false},
            },
            true,
        });
        migrator.apply(sqluna::orm::schema::CreateTableMigration{
            "sessions",
            {
                {"id", sqluna::orm::schema::ColumnType::Integer, true, true, true},
                {"user_id", sqluna::orm::schema::ColumnType::Integer, false, true, false},
                {"token", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"created_at", sqluna::orm::schema::ColumnType::Text, false, true, false},
            },
            true,
        });
        migrator.apply(sqluna::orm::schema::CreateTableMigration{
            "email_verification_tokens",
            {
                {"id", sqluna::orm::schema::ColumnType::Integer, true, true, true},
                {"user_id", sqluna::orm::schema::ColumnType::Integer, false, true, false},
                {"token", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"created_at", sqluna::orm::schema::ColumnType::Text, false, true, false},
            },
            true,
        });
        migrator.apply(sqluna::orm::schema::CreateTableMigration{
            "password_reset_tokens",
            {
                {"id", sqluna::orm::schema::ColumnType::Integer, true, true, true},
                {"user_id", sqluna::orm::schema::ColumnType::Integer, false, true, false},
                {"token", sqluna::orm::schema::ColumnType::Text, false, true, false},
                {"created_at", sqluna::orm::schema::ColumnType::Text, false, true, false},
            },
            true,
        });
    }

    std::optional<AuthUserRecord> find_user_record_by_username(const std::string& username) const {
        auto row = db_->table("users")
                       .select("id", "username", "password_hash", "role", "email_verified", "created_at")
                       .where("username", "=", username)
                       .first();
        if (!row) {
            return std::nullopt;
        }
        return map_auth_user_row(*row);
    }

    std::optional<AuthUserRecord> find_user_record_by_id(std::int64_t id) const {
        auto row = db_->table("users")
                       .select("id", "username", "password_hash", "role", "email_verified", "created_at")
                       .where("id", "=", id)
                       .first();
        if (!row) {
            return std::nullopt;
        }
        return map_auth_user_row(*row);
    }

    std::optional<AuthSessionRecord> find_session_record(const std::string& token) const {
        if (token.empty()) {
            return std::nullopt;
        }
        auto row = db_->table("sessions")
                       .select("id", "user_id", "token", "created_at")
                       .where("token", "=", token)
                       .first();
        if (!row) {
            return std::nullopt;
        }
        return map_auth_session_row(*row);
    }

    std::optional<AuthTokenRecord> find_token_record(const std::string& table, const std::string& token) const {
        auto row = db_->table(table)
                       .select("id", "user_id", "token", "created_at")
                       .where("token", "=", token)
                       .first();
        if (!row) {
            return std::nullopt;
        }
        return map_auth_token_row(*row);
    }

    Value register_user(const std::string& username, const std::string& password, const std::string& role = "customer") {
        if (username.empty() || password.empty()) {
            throw RuntimeError("username and password are required");
        }
        if (find_user_record_by_username(username)) {
            throw RuntimeError("username already exists");
        }

        const std::int64_t user_id = db_->table("users").insert({
            sqluna::query::builder::field("username", username),
            sqluna::query::builder::field("password_hash", hash_password(password)),
            sqluna::query::builder::field("role", role),
            sqluna::query::builder::field("email_verified", 0),
            sqluna::query::builder::field("created_at", iso_timestamp(true)),
        });
        return find_user_by_id(user_id);
    }

    Value authenticate_user(const std::string& username, const std::string& password) const {
        const auto user = find_user_record_by_username(username);
        if (!user || !verify_password(password, user->password_hash)) {
            return Value();
        }
        return auth_user_value(*user);
    }

    Value create_session(std::int64_t user_id) {
        if (!find_user_record_by_id(user_id)) {
            throw RuntimeError("cannot create session for unknown user");
        }

        const std::string token = random_token(48);
        db_->table("sessions").insert({
            sqluna::query::builder::field("user_id", user_id),
            sqluna::query::builder::field("token", token),
            sqluna::query::builder::field("created_at", iso_timestamp(true)),
        });

        const auto session = find_session_record(token);
        if (!session) {
            throw RuntimeError("session creation failed");
        }
        return auth_session_value(*session);
    }

    Value resolve_session(const std::string& token) const {
        const auto session = find_session_record(token);
        return session ? auth_session_value(*session) : Value();
    }

    void delete_session(const std::string& token) {
        db_->execute(sqluna::core::PreparedQuery{
            "DELETE FROM sessions WHERE token = ?;",
            {sqluna::core::DbValue(token)},
        });
    }

    Value find_user_by_id(std::int64_t user_id) const {
        const auto user = find_user_record_by_id(user_id);
        return user ? auth_user_value(*user) : Value();
    }

    void set_role(std::int64_t user_id, const std::string& role) {
        db_->execute(sqluna::core::PreparedQuery{
            "UPDATE users SET role = ? WHERE id = ?;",
            {sqluna::core::DbValue(role), sqluna::core::DbValue(user_id)},
        });
    }

    Value login(const std::string& username, const std::string& password) {
        const Value user = authenticate_user(username, password);
        if (user.is_nil()) {
            return Value();
        }
        const auto user_object = user.as_object();
        const std::int64_t user_id = static_cast<std::int64_t>(user_object->fields.at("id").as_number());
        const Value session = create_session(user_id);
        return make_object({
            {"user", user},
            {"session", session},
        });
    }

    Value attach_session(const Value& ctx) {
        if (!ctx.is_object()) {
            return Value();
        }
        auto ctx_object = ctx.as_object();
        const auto request_it = ctx_object->fields.find("request");
        if (request_it == ctx_object->fields.end() || !request_it->second.is_object()) {
            return Value();
        }

        auto request_object = request_it->second.as_object();
        const auto cookies_it = request_object->fields.find("cookies");
        if (cookies_it == request_object->fields.end() || !cookies_it->second.is_object()) {
            return Value();
        }

        const auto cookie_fields = cookies_it->second.as_object()->fields;
        const auto session_cookie = cookie_fields.find("session");
        if (session_cookie == cookie_fields.end()) {
            return Value();
        }

        const Value session = resolve_session(session_cookie->second.to_string());
        if (session.is_nil()) {
            return Value();
        }

        request_object->fields["session"] = session;
        const std::int64_t user_id = static_cast<std::int64_t>(session.as_object()->fields.at("user_id").as_number());
        request_object->fields["user"] = find_user_by_id(user_id);
        return Value();
    }

    Value require_user(const Value& ctx) {
        if (!ctx.is_object()) {
            return make_http_response_value(401, make_object({{"error", Value("unauthorized")}}), "application/json; charset=utf-8");
        }
        auto ctx_object = ctx.as_object();
        const auto request_it = ctx_object->fields.find("request");
        if (request_it == ctx_object->fields.end() || !request_it->second.is_object()) {
            return make_http_response_value(401, make_object({{"error", Value("unauthorized")}}), "application/json; charset=utf-8");
        }
        const auto request_object = request_it->second.as_object();
        const auto user_it = request_object->fields.find("user");
        if (user_it == request_object->fields.end() || user_it->second.is_nil()) {
            return make_http_response_value(401, make_object({{"error", Value("unauthorized")}}), "application/json; charset=utf-8");
        }
        return Value();
    }

    Value require_role(const Value& ctx, const std::string& role) {
        const Value guard = require_user(ctx);
        if (!guard.is_nil()) {
            return guard;
        }
        const auto user = ctx.as_object()->fields.at("request").as_object()->fields.at("user");
        if (!user.is_object() || user.as_object()->fields.at("role").as_string() != role) {
            return make_http_response_value(403, make_object({{"error", Value("forbidden")}}), "application/json; charset=utf-8");
        }
        return Value();
    }

    Value require_permission(const Value& ctx, const std::string& permission) {
        const Value guard = require_user(ctx);
        if (!guard.is_nil()) {
            return guard;
        }
        const auto user = ctx.as_object()->fields.at("request").as_object()->fields.at("user");
        const std::string role = user.as_object()->fields.at("role").as_string();
        if (!permission_allowed(role, permission)) {
            return make_http_response_value(403, make_object({{"error", Value("forbidden")}}), "application/json; charset=utf-8");
        }
        return Value();
    }

    Value make_role_middleware(const std::string& role) {
        auto self = shared_from_this();
        return make_native_function("sqluna.auth_store.role_middleware.call", 1, [self, role](Interpreter&, const std::vector<Value>& args) -> Value {
            return self->require_role(args[0], role);
        });
    }

    Value make_permission_middleware(const std::string& permission) {
        auto self = shared_from_this();
        return make_native_function("sqluna.auth_store.permission_middleware.call", 1,
                                    [self, permission](Interpreter&, const std::vector<Value>& args) -> Value {
                                        return self->require_permission(args[0], permission);
                                    });
    }

    Value begin_email_verification(std::int64_t user_id) {
        if (!find_user_record_by_id(user_id)) {
            throw RuntimeError("unknown user");
        }
        const std::string token = random_token(40);
        db_->table("email_verification_tokens").insert({
            sqluna::query::builder::field("user_id", user_id),
            sqluna::query::builder::field("token", token),
            sqluna::query::builder::field("created_at", iso_timestamp(true)),
        });
        const auto record = find_token_record("email_verification_tokens", token);
        return record ? auth_token_value(*record) : Value();
    }

    Value verify_email(const std::string& token) {
        const auto record = find_token_record("email_verification_tokens", token);
        if (!record) {
            return Value();
        }
        db_->execute(sqluna::core::PreparedQuery{
            "UPDATE users SET email_verified = 1 WHERE id = ?;",
            {sqluna::core::DbValue(record->user_id)},
        });
        db_->execute(sqluna::core::PreparedQuery{
            "DELETE FROM email_verification_tokens WHERE token = ?;",
            {sqluna::core::DbValue(token)},
        });
        return find_user_by_id(record->user_id);
    }

    Value begin_password_reset(const std::string& username) {
        const auto user = find_user_record_by_username(username);
        if (!user) {
            return Value();
        }
        const std::string token = random_token(40);
        db_->table("password_reset_tokens").insert({
            sqluna::query::builder::field("user_id", user->id),
            sqluna::query::builder::field("token", token),
            sqluna::query::builder::field("created_at", iso_timestamp(true)),
        });
        const auto record = find_token_record("password_reset_tokens", token);
        return record ? auth_token_value(*record) : Value();
    }

    Value reset_password(const std::string& token, const std::string& new_password) {
        const auto record = find_token_record("password_reset_tokens", token);
        if (!record) {
            return Value();
        }
        db_->execute(sqluna::core::PreparedQuery{
            "UPDATE users SET password_hash = ? WHERE id = ?;",
            {sqluna::core::DbValue(hash_password(new_password)), sqluna::core::DbValue(record->user_id)},
        });
        db_->execute(sqluna::core::PreparedQuery{
            "DELETE FROM password_reset_tokens WHERE token = ?;",
            {sqluna::core::DbValue(token)},
        });
        return find_user_by_id(record->user_id);
    }

    std::string issue_signed_token(std::int64_t user_id, const std::string& secret) const {
        if (!find_user_record_by_id(user_id)) {
            throw RuntimeError("unknown user");
        }
        const std::string nonce = random_token(12);
        const std::string issued_at = iso_timestamp(true);
        const std::string payload = signed_token_payload(user_id, nonce, issued_at);
        const std::string signature = sha256_hex(payload + ":" + secret);
        return payload + "." + signature;
    }

    Value resolve_signed_token(const std::string& token, const std::string& secret) const {
        const std::size_t dot = token.rfind('.');
        if (dot == std::string::npos) {
            return Value();
        }
        const std::string payload = token.substr(0, dot);
        const std::string signature = token.substr(dot + 1);
        if (!constant_time_equals(sha256_hex(payload + ":" + secret), signature)) {
            return Value();
        }

        const std::size_t first = payload.find(':');
        const std::size_t second = payload.find(':', first == std::string::npos ? first : first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            return Value();
        }
        const std::int64_t user_id = std::stoll(payload.substr(0, first));
        Value user = find_user_by_id(user_id);
        if (user.is_nil()) {
            return Value();
        }
        return make_object({
            {"user", user},
            {"token", Value(token)},
            {"issued_at", Value(payload.substr(second + 1))},
        });
    }

    Value make_bearer_middleware(const std::string& secret) {
        auto self = shared_from_this();
        return make_native_function("sqluna.auth_store.bearer_middleware.call", 1,
                                    [self, secret](Interpreter&, const std::vector<Value>& args) -> Value {
                                        if (!args[0].is_object()) {
                                            return Value();
                                        }
                                        auto request = args[0].as_object()->fields.at("request").as_object();
                                        const auto headers_it = request->fields.find("headers");
                                        if (headers_it == request->fields.end() || !headers_it->second.is_object()) {
                                            return Value();
                                        }
                                        const auto headers = headers_it->second.as_object()->fields;
                                        const auto auth_it = headers.find("authorization");
                                        if (auth_it == headers.end()) {
                                            return Value();
                                        }
                                        const std::string header = auth_it->second.as_string();
                                        const std::string prefix = "Bearer ";
                                        if (header.rfind(prefix, 0) != 0) {
                                            return Value();
                                        }
                                        const Value resolved = self->resolve_signed_token(header.substr(prefix.size()), secret);
                                        if (resolved.is_nil()) {
                                            return Value();
                                        }
                                        request->fields["bearer"] = resolved;
                                        request->fields["user"] = resolved.as_object()->fields.at("user");
                                        return Value();
                                    });
    }

    std::shared_ptr<sqluna::query::builder::Database> db_;
};

std::filesystem::path sanitize_request_path(const std::filesystem::path& root, std::string request_target) {
    const std::size_t query_pos = request_target.find('?');
    if (query_pos != std::string::npos) request_target = request_target.substr(0, query_pos);
    if (request_target.empty() || request_target == "/") request_target = "/index.html";
    if (request_target.front() == '/') request_target.erase(0, 1);
    if (request_target.find("..") != std::string::npos) {
        throw RuntimeError("path traversal blocked");
    }
    return root / std::filesystem::path(request_target);
}

std::optional<HttpResponseData> try_static_mounts(const RouterState& router_state, const HttpRequestData& request) {
    const std::string request_path = normalize_route_path(request.path);
    for (const auto& mount : router_state.static_mounts) {
        const std::string prefix = normalize_route_path(mount.prefix);
        if (prefix != "/" && request_path.rfind(prefix, 0) != 0) {
            continue;
        }

        std::string relative_path = prefix == "/" ? request_path : request_path.substr(prefix.size());
        if (relative_path.empty()) {
            relative_path = "/";
        }

        try {
            const std::filesystem::path requested_path = sanitize_request_path(mount.root, relative_path);
            const std::filesystem::path canonical_requested = std::filesystem::weakly_canonical(requested_path);
            const std::string canonical_string = canonical_requested.string();
            const std::string root_string = mount.root.string();
            if (canonical_string.rfind(root_string, 0) != 0 || !std::filesystem::exists(canonical_requested) ||
                std::filesystem::is_directory(canonical_requested)) {
                return HttpResponseData{404, "text/plain; charset=utf-8", "Not found", {}};
            }
            return HttpResponseData{200, mime_type_for_path(canonical_requested), read_text_file(canonical_requested), {}};
        } catch (const RuntimeError&) {
            return HttpResponseData{403, "text/plain; charset=utf-8", "Forbidden", {}};
        }
    }

    return std::nullopt;
}

void serve_static_directory(const std::filesystem::path& root_dir, int port) {
    const std::filesystem::path root = std::filesystem::weakly_canonical(root_dir);
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        throw RuntimeError("web root does not exist: " + root.string());
    }

#if defined(_WIN32)
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw RuntimeError("winsock startup failed");
    }
#endif

    SocketHandle server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == kInvalidSocket) {
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("socket create failed: " + socket_error_string());
    }

    const int opt_value = 1;
#if defined(_WIN32)
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_value), sizeof(opt_value));
#else
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("bind failed: " + socket_error_string());
    }

    if (listen(server_socket, 16) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("listen failed: " + socket_error_string());
    }

    std::cout << "Lunara web server listening on http://127.0.0.1:" << port << '\n';
    std::cout << "Serving " << root.string() << '\n';

    while (true) {
        sockaddr_in client_address{};
#if defined(_WIN32)
        int client_size = sizeof(client_address);
#else
        socklen_t client_size = sizeof(client_address);
#endif
        SocketHandle client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_size);
        if (client_socket == kInvalidSocket) {
            close_socket(server_socket);
#if defined(_WIN32)
            WSACleanup();
#endif
            throw RuntimeError("accept failed: " + socket_error_string());
        }

        try {
            char buffer[4096];
#if defined(_WIN32)
            const int received = recv(client_socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
            const int received = static_cast<int>(recv(client_socket, buffer, sizeof(buffer), 0));
#endif
            if (received <= 0) {
                close_socket(client_socket);
                continue;
            }

            const HttpRequestData request = parse_http_request(std::string(buffer, buffer + received));
            const std::map<std::string, std::string> cors_headers = build_cors_headers(request);

            if (request.method == "OPTIONS") {
                send_all(client_socket, build_http_response(204, "text/plain; charset=utf-8", "", cors_headers));
                close_socket(client_socket);
                continue;
            }

            if (request.method != "GET") {
                send_all(client_socket, build_http_response(405, "text/plain; charset=utf-8", "Only GET is supported", cors_headers));
                close_socket(client_socket);
                continue;
            }

            try {
                const std::filesystem::path requested_path = sanitize_request_path(root, request.target);
                const std::filesystem::path canonical_requested = std::filesystem::weakly_canonical(requested_path);
                const std::string canonical_string = canonical_requested.string();
                const std::string root_string = root.string();
                if (canonical_string.rfind(root_string, 0) != 0 || !std::filesystem::exists(canonical_requested) ||
                    std::filesystem::is_directory(canonical_requested)) {
                    send_all(client_socket, build_http_response(404, "text/plain; charset=utf-8", "Not found", cors_headers));
                } else {
                    send_all(client_socket, build_http_response(200, mime_type_for_path(canonical_requested),
                                                                read_text_file(canonical_requested), cors_headers));
                }
            } catch (const RuntimeError&) {
                send_all(client_socket, build_http_response(403, "text/plain; charset=utf-8", "Forbidden", cors_headers));
            }
        } catch (const std::exception&) {
            try {
                send_all(client_socket, build_http_response(500, "text/plain; charset=utf-8", "Internal server error"));
            } catch (...) {
            }
        }

        close_socket(client_socket);
    }
}

Value invoke_callable(Interpreter& interpreter, const Value& callable_value, const std::vector<Value>& arguments) {
    if (!callable_value.is_callable()) {
        throw RuntimeError("expected callable value");
    }
    auto callable = callable_value.as_callable();
    if (callable->arity() != arguments.size()) {
        throw RuntimeError(callable->debug_name() + " expects " + std::to_string(callable->arity()) + " argument(s), got " +
                           std::to_string(arguments.size()));
    }
    return callable->call(interpreter, arguments);
}

void serve_router_app(Interpreter& interpreter, std::shared_ptr<RouterState> router_state, int port) {
#if defined(_WIN32)
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw RuntimeError("winsock startup failed");
    }
#endif

    SocketHandle server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == kInvalidSocket) {
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("socket create failed: " + socket_error_string());
    }

    const int opt_value = 1;
#if defined(_WIN32)
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_value), sizeof(opt_value));
#else
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("bind failed: " + socket_error_string());
    }

    if (listen(server_socket, 16) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("listen failed: " + socket_error_string());
    }

    std::cout << "Lunara router listening on http://127.0.0.1:" << port << '\n';

    while (true) {
        sockaddr_in client_address{};
#if defined(_WIN32)
        int client_size = sizeof(client_address);
#else
        socklen_t client_size = sizeof(client_address);
#endif
        SocketHandle client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_size);
        if (client_socket == kInvalidSocket) {
            close_socket(server_socket);
#if defined(_WIN32)
            WSACleanup();
#endif
            throw RuntimeError("accept failed: " + socket_error_string());
        }

        try {
            char buffer[65536];
#if defined(_WIN32)
            const int received = recv(client_socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
            const int received = static_cast<int>(recv(client_socket, buffer, sizeof(buffer), 0));
#endif
            if (received <= 0) {
                close_socket(client_socket);
                continue;
            }
            const HttpRequestData request = parse_http_request(std::string(buffer, buffer + received));
            const std::map<std::string, std::string> cors_headers = build_cors_headers(request, router_state.get());

            if (is_websocket_upgrade(request)) {
                const auto websocket_route = find_websocket_route(*router_state, request.path.empty() ? "/" : request.path);
                if (!websocket_route) {
                    send_all(client_socket, build_http_response(404, "text/plain; charset=utf-8", "WebSocket route not found",
                                                                cors_headers));
                    close_socket(client_socket);
                    continue;
                }

                try {
                    run_websocket_connection(interpreter, client_socket, request, websocket_route->handlers, websocket_route->params);
                } catch (const std::exception&) {
                    try {
                        send_websocket_frame(client_socket, 0x8, "");
                    } catch (...) {
                    }
                }
                close_socket(client_socket);
                continue;
            }

            if (request.method == "OPTIONS") {
                send_all(client_socket, build_http_response(204, "text/plain; charset=utf-8", "", cors_headers));
                close_socket(client_socket);
                continue;
            }

            if (request.method != "GET" && request.method != "POST" && request.method != "PUT" && request.method != "PATCH" &&
                request.method != "DELETE") {
                send_all(client_socket, build_http_response(405, "text/plain; charset=utf-8", "Method not allowed", cors_headers));
                close_socket(client_socket);
                continue;
            }

            const auto route_match = find_route(*router_state, request.method, request.path.empty() ? "/" : request.path);
            const Value request_value = request_to_value(request, route_match ? route_match->params : std::map<std::string, std::string>{});
            const Value context_value = make_object({{"request", request_value}});

            bool handled = false;
            for (const auto& middleware : router_state->middlewares) {
                const Value middleware_result = invoke_callable(interpreter, middleware, {context_value});
                if (!middleware_result.is_nil()) {
                    HttpResponseData response = normalize_http_response(middleware_result);
                    merge_http_headers(response.headers, cors_headers);
                    send_all(client_socket, build_http_response(response.status, response.content_type, response.body, response.headers));
                    handled = true;
                    break;
                }
            }

            if (handled) {
                close_socket(client_socket);
                continue;
            }

            if (!route_match) {
                if (request.method == "GET") {
                    if (const auto static_response = try_static_mounts(*router_state, request)) {
                        HttpResponseData response = *static_response;
                        merge_http_headers(response.headers, cors_headers);
                        send_all(client_socket, build_http_response(response.status, response.content_type, response.body,
                                                                    response.headers));
                        close_socket(client_socket);
                        continue;
                    }
                }
                send_all(client_socket, build_http_response(404, "text/plain; charset=utf-8", "Route not found", cors_headers));
                close_socket(client_socket);
                continue;
            }

            const Value handler_result = invoke_callable(interpreter, route_match->handler, {context_value});
            HttpResponseData response = normalize_http_response(handler_result);
            merge_http_headers(response.headers, cors_headers);
            send_all(client_socket, build_http_response(response.status, response.content_type, response.body, response.headers));
        } catch (const RuntimeError& error) {
            try {
                send_all(client_socket, build_http_response(400, "text/plain; charset=utf-8", error.what()));
            } catch (...) {
            }
        } catch (const std::exception&) {
            try {
                send_all(client_socket, build_http_response(500, "text/plain; charset=utf-8", "Internal server error"));
            } catch (...) {
            }
        }

        close_socket(client_socket);
    }
}

struct WebSocketFrame {
    bool fin = true;
    std::uint8_t opcode = 0;
    std::string payload;
};

struct WebSocketClientState {
    SocketHandle socket = kInvalidSocket;
    bool open = true;
    HttpRequestData request;
};

using WebSocketServerState = WebSocketHandlers;

std::array<std::uint8_t, 20> sha1_digest(const std::string& input) {
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) {
        bytes.push_back(0x00);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xff));
    }

    std::uint32_t h0 = 0x67452301;
    std::uint32_t h1 = 0xEFCDAB89;
    std::uint32_t h2 = 0x98BADCFE;
    std::uint32_t h3 = 0x10325476;
    std::uint32_t h4 = 0xC3D2E1F0;

    auto left_rotate = [](std::uint32_t value, int shift) -> std::uint32_t {
        return (value << shift) | (value >> (32 - shift));
    };

    for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        std::uint32_t words[80]{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t index = chunk + static_cast<std::size_t>(i) * 4;
            words[i] = (static_cast<std::uint32_t>(bytes[index]) << 24) |
                       (static_cast<std::uint32_t>(bytes[index + 1]) << 16) |
                       (static_cast<std::uint32_t>(bytes[index + 2]) << 8) |
                       static_cast<std::uint32_t>(bytes[index + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            words[i] = left_rotate(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            const std::uint32_t temp = left_rotate(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = left_rotate(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    return {
        static_cast<std::uint8_t>((h0 >> 24) & 0xff), static_cast<std::uint8_t>((h0 >> 16) & 0xff),
        static_cast<std::uint8_t>((h0 >> 8) & 0xff),  static_cast<std::uint8_t>(h0 & 0xff),
        static_cast<std::uint8_t>((h1 >> 24) & 0xff), static_cast<std::uint8_t>((h1 >> 16) & 0xff),
        static_cast<std::uint8_t>((h1 >> 8) & 0xff),  static_cast<std::uint8_t>(h1 & 0xff),
        static_cast<std::uint8_t>((h2 >> 24) & 0xff), static_cast<std::uint8_t>((h2 >> 16) & 0xff),
        static_cast<std::uint8_t>((h2 >> 8) & 0xff),  static_cast<std::uint8_t>(h2 & 0xff),
        static_cast<std::uint8_t>((h3 >> 24) & 0xff), static_cast<std::uint8_t>((h3 >> 16) & 0xff),
        static_cast<std::uint8_t>((h3 >> 8) & 0xff),  static_cast<std::uint8_t>(h3 & 0xff),
        static_cast<std::uint8_t>((h4 >> 24) & 0xff), static_cast<std::uint8_t>((h4 >> 16) & 0xff),
        static_cast<std::uint8_t>((h4 >> 8) & 0xff),  static_cast<std::uint8_t>(h4 & 0xff),
    };
}

std::string base64_encode(const std::uint8_t* data, std::size_t size) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const std::uint32_t chunk = (static_cast<std::uint32_t>(data[i]) << 16) |
                                    (static_cast<std::uint32_t>(i + 1 < size ? data[i + 1] : 0) << 8) |
                                    static_cast<std::uint32_t>(i + 2 < size ? data[i + 2] : 0);
        encoded.push_back(kAlphabet[(chunk >> 18) & 0x3f]);
        encoded.push_back(kAlphabet[(chunk >> 12) & 0x3f]);
        encoded.push_back(i + 1 < size ? kAlphabet[(chunk >> 6) & 0x3f] : '=');
        encoded.push_back(i + 2 < size ? kAlphabet[chunk & 0x3f] : '=');
    }
    return encoded;
}

std::string websocket_accept_key(const std::string& client_key) {
    static constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto digest = sha1_digest(client_key + kGuid);
    return base64_encode(digest.data(), digest.size());
}

bool is_websocket_upgrade(const HttpRequestData& request) {
    const auto upgrade_it = request.headers.find("upgrade");
    const auto connection_it = request.headers.find("connection");
    const auto key_it = request.headers.find("sec-websocket-key");
    if (upgrade_it == request.headers.end() || connection_it == request.headers.end() || key_it == request.headers.end()) {
        return false;
    }
    return lower_ascii(upgrade_it->second) == "websocket" &&
           lower_ascii(connection_it->second).find("upgrade") != std::string::npos;
}

void recv_exact(SocketHandle socket_handle, void* buffer, std::size_t size) {
    std::size_t total_received = 0;
    auto* bytes = static_cast<char*>(buffer);
    while (total_received < size) {
#if defined(_WIN32)
        const int received = recv(socket_handle, bytes + total_received, static_cast<int>(size - total_received), 0);
#else
        const int received = static_cast<int>(recv(socket_handle, bytes + total_received, size - total_received, 0));
#endif
        if (received <= 0) {
            throw RuntimeError("socket recv failed: " + socket_error_string());
        }
        total_received += static_cast<std::size_t>(received);
    }
}

void send_websocket_frame(SocketHandle socket_handle, std::uint8_t opcode, const std::string& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0x80 | opcode));
    if (payload.size() < 126) {
        frame.push_back(static_cast<char>(payload.size()));
    } else if (payload.size() <= 0xffff) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((payload.size() >> 8) & 0xff));
        frame.push_back(static_cast<char>(payload.size() & 0xff));
    } else {
        frame.push_back(127);
        const std::uint64_t size = static_cast<std::uint64_t>(payload.size());
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<char>((size >> shift) & 0xff));
        }
    }
    frame += payload;
    send_all(socket_handle, frame);
}

WebSocketFrame read_websocket_frame(SocketHandle socket_handle) {
    unsigned char header[2]{};
    recv_exact(socket_handle, header, sizeof(header));

    WebSocketFrame frame;
    frame.fin = (header[0] & 0x80) != 0;
    frame.opcode = static_cast<std::uint8_t>(header[0] & 0x0f);
    const bool masked = (header[1] & 0x80) != 0;
    std::uint64_t payload_size = header[1] & 0x7f;

    if (payload_size == 126) {
        unsigned char extended[2]{};
        recv_exact(socket_handle, extended, sizeof(extended));
        payload_size = (static_cast<std::uint64_t>(extended[0]) << 8) | static_cast<std::uint64_t>(extended[1]);
    } else if (payload_size == 127) {
        unsigned char extended[8]{};
        recv_exact(socket_handle, extended, sizeof(extended));
        payload_size = 0;
        for (const unsigned char byte : extended) {
            payload_size = (payload_size << 8) | static_cast<std::uint64_t>(byte);
        }
    }

    if (!masked) {
        throw RuntimeError("websocket client frames must be masked");
    }
    if (payload_size > 1024 * 1024) {
        throw RuntimeError("websocket frame too large");
    }

    unsigned char mask[4]{};
    recv_exact(socket_handle, mask, sizeof(mask));
    frame.payload.assign(static_cast<std::size_t>(payload_size), '\0');
    if (payload_size > 0) {
        recv_exact(socket_handle, frame.payload.data(), static_cast<std::size_t>(payload_size));
        for (std::size_t i = 0; i < frame.payload.size(); ++i) {
            frame.payload[i] = static_cast<char>(static_cast<unsigned char>(frame.payload[i]) ^ mask[i % 4]);
        }
    }

    return frame;
}

Value make_websocket_client_value(const std::shared_ptr<WebSocketClientState>& client_state,
                                  std::map<std::string, std::string> params = {}) {
    return make_object({
        {"path", Value(normalize_route_path(client_state->request.path))},
        {"request", request_to_value(client_state->request, std::move(params))},
        {"send_text", make_native_function("websocket.send_text", 1, [client_state](Interpreter&, const std::vector<Value>& args) -> Value {
             if (!client_state->open) {
                 throw RuntimeError("websocket is closed");
             }
             send_websocket_frame(client_state->socket, 0x1, args[0].to_string());
             return Value();
         })},
        {"close", make_native_function("websocket.close", 0, [client_state](Interpreter&, const std::vector<Value>&) -> Value {
             if (client_state->open) {
                 send_websocket_frame(client_state->socket, 0x8, "");
                 client_state->open = false;
             }
             return Value();
        })},
    });
}

std::string build_websocket_handshake_response(const HttpRequestData& request) {
    const std::string accept_key = websocket_accept_key(request.headers.at("sec-websocket-key"));
    std::ostringstream handshake;
    handshake << "HTTP/1.1 101 Switching Protocols\r\n";
    handshake << "Upgrade: websocket\r\n";
    handshake << "Connection: Upgrade\r\n";
    handshake << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    return handshake.str();
}

void run_websocket_connection(Interpreter& interpreter,
                              SocketHandle client_socket,
                              const HttpRequestData& request,
                              const WebSocketHandlers& handlers,
                              std::map<std::string, std::string> params) {
    auto client_state = std::make_shared<WebSocketClientState>();
    client_state->socket = client_socket;
    client_state->request = request;

    send_all(client_socket, build_websocket_handshake_response(request));

    const Value client_value = make_websocket_client_value(client_state, std::move(params));
    if (!handlers.on_open.is_nil()) {
        static_cast<void>(invoke_callable(interpreter, handlers.on_open, {client_value}));
    }

    while (client_state->open) {
        const WebSocketFrame frame = read_websocket_frame(client_socket);
        if (frame.opcode == 0x8) {
            client_state->open = false;
            send_websocket_frame(client_socket, 0x8, "");
            break;
        }
        if (frame.opcode == 0x9) {
            send_websocket_frame(client_socket, 0xA, frame.payload);
            continue;
        }
        if (frame.opcode != 0x1) {
            continue;
        }

        if (!handlers.on_message.is_nil()) {
            const Value reply = invoke_callable(interpreter, handlers.on_message, {client_value, Value(frame.payload)});
            if (!reply.is_nil()) {
                send_websocket_frame(client_socket, 0x1, reply.is_string() ? reply.as_string() : render_value(reply, true));
            }
        }
    }

    if (!handlers.on_close.is_nil()) {
        static_cast<void>(invoke_callable(interpreter, handlers.on_close, {client_value}));
    }
}

void serve_websocket_server(Interpreter& interpreter, std::shared_ptr<WebSocketServerState> server_state, int port) {
#if defined(_WIN32)
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw RuntimeError("winsock startup failed");
    }
#endif

    SocketHandle server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == kInvalidSocket) {
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("socket create failed: " + socket_error_string());
    }

    const int opt_value = 1;
#if defined(_WIN32)
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_value), sizeof(opt_value));
#else
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("bind failed: " + socket_error_string());
    }

    if (listen(server_socket, 16) != 0) {
        close_socket(server_socket);
#if defined(_WIN32)
        WSACleanup();
#endif
        throw RuntimeError("listen failed: " + socket_error_string());
    }

    std::cout << "Lunara websocket server listening on ws://127.0.0.1:" << port << '\n';

    while (true) {
        sockaddr_in client_address{};
#if defined(_WIN32)
        int client_size = sizeof(client_address);
#else
        socklen_t client_size = sizeof(client_address);
#endif
        SocketHandle client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_address), &client_size);
        if (client_socket == kInvalidSocket) {
            close_socket(server_socket);
#if defined(_WIN32)
            WSACleanup();
#endif
            throw RuntimeError("accept failed: " + socket_error_string());
        }

        try {
            char buffer[65536];
#if defined(_WIN32)
            const int received = recv(client_socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
            const int received = static_cast<int>(recv(client_socket, buffer, sizeof(buffer), 0));
#endif
            if (received <= 0) {
                close_socket(client_socket);
                continue;
            }

            const HttpRequestData request = parse_http_request(std::string(buffer, buffer + received));
            if (!is_websocket_upgrade(request)) {
                send_all(client_socket, build_http_response(400, "text/plain; charset=utf-8", "Expected websocket upgrade"));
                close_socket(client_socket);
                continue;
            }

            run_websocket_connection(interpreter, client_socket, request, *server_state);
        } catch (const std::exception&) {
            try {
                send_websocket_frame(client_socket, 0x8, "");
            } catch (...) {
            }
        }

        close_socket(client_socket);
    }
}

class JsonParser final {
  public:
    explicit JsonParser(std::string source) : source_(std::move(source)) {}

    Value parse() {
        skip_whitespace();
        Value value = parse_value();
        skip_whitespace();
        if (!is_at_end()) throw RuntimeError("json decode error: unexpected trailing characters");
        return value;
    }

  private:
    bool is_at_end() const { return current_ >= source_.size(); }
    char peek() const { return is_at_end() ? '\0' : source_[current_]; }
    char advance() {
        if (is_at_end()) throw RuntimeError("json decode error: unexpected end of input");
        return source_[current_++];
    }
    void skip_whitespace() {
        while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek()))) ++current_;
    }
    void consume(char expected, const std::string& message) {
        if (advance() != expected) throw RuntimeError("json decode error: " + message);
    }
    void consume_literal(const std::string& literal) {
        for (const char expected : literal) {
            if (advance() != expected) throw RuntimeError("json decode error: invalid literal");
        }
    }
    std::string parse_string() {
        consume('"', "expected string");
        std::ostringstream out;
        while (!is_at_end()) {
            const char ch = advance();
            if (ch == '"') return out.str();
            if (ch == '\\') {
                const char escaped = advance();
                switch (escaped) {
                    case '"': out << '"'; break;
                    case '\\': out << '\\'; break;
                    case '/': out << '/'; break;
                    case 'b': out << '\b'; break;
                    case 'f': out << '\f'; break;
                    case 'n': out << '\n'; break;
                    case 'r': out << '\r'; break;
                    case 't': out << '\t'; break;
                    default: throw RuntimeError("json decode error: unsupported escape sequence");
                }
            } else {
                out << ch;
            }
        }
        throw RuntimeError("json decode error: unterminated string");
    }
    double parse_number() {
        const std::size_t start = current_;
        if (peek() == '-') advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.') {
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        return std::stod(source_.substr(start, current_ - start));
    }
    Value parse_array() {
        consume('[', "expected '['");
        std::vector<Value> items;
        skip_whitespace();
        if (peek() == ']') {
            advance();
            return make_list(std::move(items));
        }
        while (true) {
            skip_whitespace();
            items.push_back(parse_value());
            skip_whitespace();
            if (peek() == ']') {
                advance();
                break;
            }
            consume(',', "expected ',' between array items");
        }
        return make_list(std::move(items));
    }
    Value parse_object() {
        consume('{', "expected '{'");
        std::map<std::string, Value> fields;
        skip_whitespace();
        if (peek() == '}') {
            advance();
            return make_object(std::move(fields));
        }
        while (true) {
            skip_whitespace();
            if (peek() != '"') throw RuntimeError("json decode error: expected string key");
            std::string key = parse_string();
            skip_whitespace();
            consume(':', "expected ':' after object key");
            skip_whitespace();
            fields[key] = parse_value();
            skip_whitespace();
            if (peek() == '}') {
                advance();
                break;
            }
            consume(',', "expected ',' between object items");
        }
        return make_object(std::move(fields));
    }
    Value parse_value() {
        skip_whitespace();
        if (is_at_end()) throw RuntimeError("json decode error: unexpected end of input");
        switch (peek()) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return Value(parse_string());
            case 't': consume_literal("true"); return Value(true);
            case 'f': consume_literal("false"); return Value(false);
            case 'n': consume_literal("null"); return Value();
            default:
                if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) return Value(parse_number());
                throw RuntimeError("json decode error: unexpected character");
        }
    }

    std::string source_;
    std::size_t current_ = 0;
};

Value decode_json_text(const std::string& text) { return JsonParser(text).parse(); }

}  // namespace

std::string runtime::Value::to_string() const { return render_value(*this, false); }

Interpreter::Interpreter(std::filesystem::path entry_script, std::ostream* output)
    : globals_(std::make_shared<Environment>()), environment_(globals_), output_(output ? output : &std::cout) {
    if (!entry_script.empty()) script_stack_.push_back(std::filesystem::absolute(entry_script));

    globals_->define(
        "print",
        make_native_function(
            "print",
            1,
            [this](Interpreter&, const std::vector<Value>& args) -> Value {
                (*output_) << args[0].to_string() << '\n';
                return Value();
            }),
        true);

    globals_->define(
        "len",
        make_native_function(
            "len",
            1,
            [](Interpreter&, const std::vector<Value>& args) -> Value {
                const Value& value = args[0];
                if (value.is_string()) return Value(static_cast<double>(value.as_string().size()));
                if (value.is_list()) return Value(static_cast<double>(value.as_list()->items.size()));
                if (value.is_object()) return Value(static_cast<double>(value.as_object()->fields.size()));
                throw RuntimeError("len expects string, list, or object");
            }),
        true);

    globals_->define(
        "type",
        make_native_function(
            "type",
            1,
            [](Interpreter&, const std::vector<Value>& args) -> Value { return Value(args[0].type_name()); }),
        true);

    globals_->define(
        "keys",
        make_native_function(
            "keys",
            1,
            [](Interpreter&, const std::vector<Value>& args) -> Value {
                if (!args[0].is_object()) throw RuntimeError("keys expects an object");
                std::vector<Value> items;
                items.reserve(args[0].as_object()->fields.size());
                for (const auto& [key, value] : args[0].as_object()->fields) {
                    static_cast<void>(value);
                    items.push_back(Value(key));
                }
                return make_list(std::move(items));
            }),
        true);

    globals_->define(
        "values",
        make_native_function(
            "values",
            1,
            [](Interpreter&, const std::vector<Value>& args) -> Value {
                if (!args[0].is_object()) throw RuntimeError("values expects an object");
                std::vector<Value> items;
                items.reserve(args[0].as_object()->fields.size());
                for (const auto& [key, value] : args[0].as_object()->fields) {
                    static_cast<void>(key);
                    items.push_back(value);
                }
                return make_list(std::move(items));
            }),
        true);

    globals_->define(
        "assert",
        make_native_function(
            "assert",
            2,
            [](Interpreter&, const std::vector<Value>& args) -> Value {
                if (!args[0].is_truthy()) {
                    throw RuntimeError("assert failed: " + args[1].to_string());
                }
                return Value();
            }),
        true);

    const auto json_encode = make_native_function("json.encode", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
        return Value(render_value(args[0], true));
    });
    const auto json_write = make_native_function("json.write", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
        write_text_file(args[0].as_string(), render_value(args[1], true));
        return Value();
    });
    const auto json_decode = make_native_function("json.decode", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
        return decode_json_text(args[0].as_string());
    });
    const auto json_read = make_native_function("json.read", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
        return decode_json_text(read_text_file(args[0].as_string()));
    });

    stdlib_modules_["fs"] = make_object({
        {"exists", make_native_function("fs.exists", 1, [](Interpreter&, const std::vector<Value>& args) -> Value { return Value(std::filesystem::exists(args[0].as_string())); })},
        {"mkdir", make_native_function("fs.mkdir", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const std::filesystem::path path = args[0].as_string();
             if (args[1].is_truthy()) std::filesystem::create_directories(path); else std::filesystem::create_directory(path);
             return Value();
         })},
        {"read_text", make_native_function("fs.read_text", 1, [](Interpreter&, const std::vector<Value>& args) -> Value { return Value(read_text_file(args[0].as_string())); })},
        {"write_text", make_native_function("fs.write_text", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             write_text_file(args[0].as_string(), args[1].to_string());
             return Value();
         })},
    });

    stdlib_modules_["json"] = make_object({
        {"encode", json_encode},
        {"dumps", json_encode},
        {"write", json_write},
        {"dump", json_write},
        {"decode", json_decode},
        {"loads", json_decode},
        {"read", json_read},
        {"load", json_read},
    });

    stdlib_modules_["time"] = make_object({
        {"now", make_native_function("time.now", 0, [](Interpreter&, const std::vector<Value>&) -> Value { return Value(iso_timestamp(false)); })},
        {"utc", make_native_function("time.utc", 0, [](Interpreter&, const std::vector<Value>&) -> Value { return Value(iso_timestamp(true)); })},
    });

    stdlib_modules_["security"] = make_object({
        {"constant_time_equals", make_native_function("security.constant_time_equals", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(constant_time_equals(args[0].to_string(), args[1].to_string()));
         })},
        {"safe_join", make_native_function("security.safe_join", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(safe_join_path(args[0].as_string(), args[1].as_string()).string());
         })},
        {"issue_token", make_native_function("security.issue_token", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(random_token(static_cast<std::size_t>(args[0].as_number())));
         })},
    });

    stdlib_modules_["sqluna"] = make_object({
        {"auth_store", make_native_function("sqluna.auth_store", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             auto store = std::make_shared<SqlunaAuthStore>(args[0].as_string());
             return store->make_value();
         })},
        {"sqlite", make_native_function("sqluna.sqlite", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             auto db = std::make_shared<SqlunaDatabaseBridge>(args[0].as_string());
             return db->make_value();
         })},
        {"column", make_native_function("sqluna.column", 3, [](Interpreter&, const std::vector<Value>& args) -> Value {
             std::map<std::string, Value> column = {
                 {"name", Value(args[0].as_string())},
                 {"type", Value(args[1].as_string())},
             };
             if (args[2].is_object()) {
                 for (const auto& [key, value] : args[2].as_object()->fields) {
                     column[key] = value;
                 }
             }
             return make_object(std::move(column));
         })},
        {"table", make_native_function("sqluna.table", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_object({
                 {"table", Value(args[0].as_string())},
                 {"columns", args[1]},
             });
         })},
    });

    stdlib_modules_["payments"] = make_object({
        {"contract", make_native_function("payments.contract", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             if (!args[1].is_object()) {
                 throw RuntimeError("payment adapter handlers must be an object");
             }
             const auto handlers = args[1].as_object()->fields;
             const std::vector<std::string> required = {"create_checkout", "refund", "webhook"};
             for (const auto& name : required) {
                 const auto it = handlers.find(name);
                 if (it == handlers.end() || !it->second.is_callable()) {
                     throw RuntimeError("payment adapter requires callable '" + name + "'");
                 }
             }
             std::map<std::string, Value> adapter = handlers;
             adapter["name"] = Value(args[0].as_string());
             return make_object(std::move(adapter));
         })},
        {"mock", make_native_function("payments.mock", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             return make_object({
                 {"name", Value("mock")},
                 {"create_checkout", make_native_function("payments.mock.create_checkout", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
                      return make_object({
                          {"provider", Value("mock")},
                          {"checkout_id", Value("chk_mock_001")},
                          {"amount", args[0]},
                          {"status", Value("created")},
                      });
                  })},
                 {"refund", make_native_function("payments.mock.refund", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
                      return make_object({
                          {"provider", Value("mock")},
                          {"reference", args[0]},
                          {"status", Value("refunded")},
                      });
                  })},
                 {"webhook", make_native_function("payments.mock.webhook", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
                      return make_object({
                          {"provider", Value("mock")},
                          {"received", args[0]},
                          {"accepted", Value(true)},
                      });
                  })},
             });
         })},
    });

    const auto capability_info = make_native_function("compute.capabilities", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
        return build_capabilities_object();
    });
    stdlib_modules_["compute"] = make_object({
        {"capabilities", capability_info},
        {"backend", make_native_function("compute.backend", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             return build_capabilities_object().as_object()->fields["preferred_backend"];
         })},
    });
    stdlib_modules_["cpu"] = make_object({
        {"info", make_native_function("cpu.info", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             const double cpu_threads = static_cast<double>(std::thread::hardware_concurrency());
             return make_object({
                 {"available", Value(true)},
                 {"threads", Value(cpu_threads)},
             });
         })},
    });
    stdlib_modules_["gpu"] = make_object({
        {"info", make_native_function("gpu.info", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             const bool cuda_available = detect_cuda_available();
             return make_object({
                 {"available", Value(cuda_available)},
                 {"backend", Value(cuda_available ? "cuda" : "cpu")},
             });
         })},
    });
    stdlib_modules_["cuda"] = make_object({
        {"available", make_native_function("cuda.available", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             return Value(detect_cuda_available());
         })},
        {"info", make_native_function("cuda.info", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             const bool cuda_available = detect_cuda_available();
             return make_object({
                 {"available", Value(cuda_available)},
                 {"driver_detected", Value(cuda_available)},
             });
         })},
    });
    stdlib_modules_["dsp"] = make_object({
        {"rms", make_native_function("dsp.rms", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto samples = list_to_numbers(args[0], "samples");
             if (samples.empty()) return Value(0.0);
             double total = 0.0;
             for (const double sample : samples) total += sample * sample;
             return Value(std::sqrt(total / static_cast<double>(samples.size())));
         })},
        {"peak", make_native_function("dsp.peak", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto samples = list_to_numbers(args[0], "samples");
             double peak = 0.0;
             for (const double sample : samples) peak = (std::max)(peak, std::abs(sample));
             return Value(peak);
         })},
        {"normalize", make_native_function("dsp.normalize", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             auto samples = list_to_numbers(args[0], "samples");
             double peak = 0.0;
             for (const double sample : samples) peak = (std::max)(peak, std::abs(sample));
             if (peak < 1e-12) return numbers_to_list(samples);
             for (double& sample : samples) sample /= peak;
             return numbers_to_list(samples);
         })},
        {"moving_average", make_native_function("dsp.moving_average", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto samples = list_to_numbers(args[0], "samples");
             const std::size_t window = static_cast<std::size_t>(args[1].as_number());
             if (window == 0) throw RuntimeError("moving_average window must be positive");
             std::vector<double> output;
             output.reserve(samples.size());
             double sum = 0.0;
             for (std::size_t i = 0; i < samples.size(); ++i) {
                 sum += samples[i];
                 if (i >= window) sum -= samples[i - window];
                 const std::size_t divisor = i + 1 < window ? i + 1 : window;
                 output.push_back(sum / static_cast<double>(divisor));
             }
             return numbers_to_list(output);
         })},
        {"convolve", make_native_function("dsp.convolve", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto signal = list_to_numbers(args[0], "signal");
             const auto kernel = list_to_numbers(args[1], "kernel");
             if (kernel.empty()) throw RuntimeError("kernel must not be empty");
             std::vector<double> output(signal.size() + kernel.size() - 1, 0.0);
             for (std::size_t i = 0; i < signal.size(); ++i) {
                 for (std::size_t j = 0; j < kernel.size(); ++j) {
                     output[i + j] += signal[i] * kernel[j];
                 }
             }
             return numbers_to_list(output);
         })},
        {"mix", make_native_function("dsp.mix", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto left = list_to_numbers(args[0], "left");
             const auto right = list_to_numbers(args[1], "right");
             const std::size_t size = (std::max)(left.size(), right.size());
             std::vector<double> output(size, 0.0);
             for (std::size_t i = 0; i < size; ++i) {
                 if (i < left.size()) output[i] += left[i];
                 if (i < right.size()) output[i] += right[i];
             }
             return numbers_to_list(output);
         })},
        {"sine_wave", make_native_function("dsp.sine_wave", 3, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const std::size_t length = static_cast<std::size_t>(args[0].as_number());
             const double frequency = args[1].as_number();
             const double sample_rate = args[2].as_number();
             std::vector<double> output;
             output.reserve(length);
             for (std::size_t i = 0; i < length; ++i) {
                 const double t = static_cast<double>(i) / sample_rate;
                 output.push_back(std::sin(2.0 * 3.14159265358979323846 * frequency * t));
             }
             return numbers_to_list(output);
         })},
    });
    stdlib_modules_["fsl"] = make_object({
        {"safe_join", make_native_function("fsl.safe_join", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(safe_join_path(args[0].as_string(), args[1].as_string()).string());
         })},
        {"read_text", make_native_function("fsl.read_text", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(read_text_file(args[0].as_string()));
         })},
        {"write_text", make_native_function("fsl.write_text", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             write_text_file(args[0].as_string(), args[1].to_string());
             return Value();
         })},
        {"read_json", json_read},
        {"write_json", json_write},
    });

    stdlib_modules_["web"] = make_object({
        {"serve_static", make_native_function("web.serve_static", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             serve_static_directory(args[0].as_string(), static_cast<int>(args[1].as_number()));
             return Value();
         })},
        {"response", make_native_function("web.response", 3, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[0].as_number()), args[1], args[2].as_string());
         })},
        {"with_headers", make_native_function("web.with_headers", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             if (!args[0].is_object() || !is_http_response_object(args[0])) {
                 throw RuntimeError("first argument must be a response object");
             }
             if (!args[1].is_object()) {
                 throw RuntimeError("second argument must be an object of headers");
             }
             auto response_fields = args[0].as_object()->fields;
             auto header_fields = response_fields["headers"].is_object()
                                      ? response_fields["headers"].as_object()->fields
                                      : std::map<std::string, Value>{};
             for (const auto& [name, value] : args[1].as_object()->fields) {
                 header_fields[name] = Value(value.to_string());
             }
             response_fields["headers"] = make_object(std::move(header_fields));
             return make_object(std::move(response_fields));
         })},
        {"html", make_native_function("web.html", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[0].as_number()), args[1], "text/html; charset=utf-8");
         })},
        {"text", make_native_function("web.text", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[0].as_number()), args[1], "text/plain; charset=utf-8");
         })},
        {"json", make_native_function("web.json", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[1].as_number()), Value(render_value(args[0], true)),
                                             "application/json; charset=utf-8");
         })},
        {"template", make_native_function("web.template", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return Value(render_template_text(args[0].as_string(), args[1]));
         })},
        {"render", make_native_function("web.render", 3, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const std::string html = render_template_text(read_text_file(args[0].as_string()), args[1]);
             return make_http_response_value(static_cast<int>(args[2].as_number()), Value(html), "text/html; charset=utf-8");
         })},
        {"redirect", make_native_function("web.redirect", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[1].as_number()), Value(""), "text/plain; charset=utf-8",
                                             {{"headers", make_object({{"Location", Value(args[0].as_string())}})}});
         })},
        {"require_json", make_native_function("web.require_json", 1, [](Interpreter&, const std::vector<Value>& args) -> Value {
             const auto required_fields = field_list_from_value(args[0]);
             return make_native_function("web.require_json.middleware", 1,
                                         [required_fields](Interpreter&, const std::vector<Value>& middleware_args) -> Value {
                                             if (middleware_args.empty() || !middleware_args[0].is_object()) {
                                                 return make_http_response_value(
                                                     400, make_object({{"error", Value("invalid_request_context")}}),
                                                     "application/json; charset=utf-8");
                                             }
                                             const auto ctx = middleware_args[0].as_object();
                                             const auto request_it = ctx->fields.find("request");
                                             if (request_it == ctx->fields.end() || !request_it->second.is_object()) {
                                                 return make_http_response_value(
                                                     400, make_object({{"error", Value("invalid_request_context")}}),
                                                     "application/json; charset=utf-8");
                                             }
                                             const auto request = request_it->second.as_object();
                                             const auto json_it = request->fields.find("json");
                                             if (json_it == request->fields.end() || !json_it->second.is_object()) {
                                                 return make_http_response_value(
                                                     400, make_object({{"error", Value("json_body_required")}}),
                                                     "application/json; charset=utf-8");
                                             }
                                             const auto payload = json_it->second.as_object();
                                             for (const auto& field : required_fields) {
                                                 if (payload->fields.find(field) == payload->fields.end() ||
                                                     payload->fields.at(field).is_nil()) {
                                                     return make_http_response_value(
                                                         400,
                                                         make_object({
                                                             {"error", Value("missing_field")},
                                                             {"field", Value(field)},
                                                         }),
                                                         "application/json; charset=utf-8");
                                                 }
                                             }
                                             return Value();
                                         });
         })},
        {"cookie", make_native_function("web.cookie", 3, [](Interpreter&, const std::vector<Value>& args) -> Value {
             if (!args[0].is_object() || !is_http_response_object(args[0])) {
                 throw RuntimeError("first argument must be a response object");
             }
             auto response_fields = args[0].as_object()->fields;
             auto header_fields = response_fields["headers"].is_object()
                                      ? response_fields["headers"].as_object()->fields
                                      : std::map<std::string, Value>{};
             const std::string cookie_value = args[1].as_string() + "=" + args[2].to_string() + "; Path=/";
             const auto existing = header_fields.find("Set-Cookie");
             if (existing == header_fields.end()) {
                 header_fields["Set-Cookie"] = Value(cookie_value);
             } else {
                 header_fields["Set-Cookie"] = Value(existing->second.to_string() + ", " + cookie_value);
             }
             response_fields["headers"] = make_object(std::move(header_fields));
             return make_object(std::move(response_fields));
         })},
        {"clear_cookie", make_native_function("web.clear_cookie", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             if (!args[0].is_object() || !is_http_response_object(args[0])) {
                 throw RuntimeError("first argument must be a response object");
             }
             auto response_fields = args[0].as_object()->fields;
             auto header_fields = response_fields["headers"].is_object()
                                      ? response_fields["headers"].as_object()->fields
                                      : std::map<std::string, Value>{};
             header_fields["Set-Cookie"] = Value(args[1].as_string() + "=; Path=/; Max-Age=0");
             response_fields["headers"] = make_object(std::move(header_fields));
             return make_object(std::move(response_fields));
         })},
        {"websocket_server", make_native_function("web.websocket_server", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             auto server_state = std::make_shared<WebSocketServerState>();
             return make_object({
                 {"on_open", make_native_function("web.websocket_server.on_open", 1, [server_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[0].is_callable()) {
                          throw RuntimeError("open handler must be callable");
                      }
                      server_state->on_open = args[0];
                      return Value();
                  })},
                 {"on_message", make_native_function("web.websocket_server.on_message", 1, [server_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[0].is_callable()) {
                          throw RuntimeError("message handler must be callable");
                      }
                      server_state->on_message = args[0];
                      return Value();
                  })},
                 {"on_close", make_native_function("web.websocket_server.on_close", 1, [server_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[0].is_callable()) {
                          throw RuntimeError("close handler must be callable");
                      }
                      server_state->on_close = args[0];
                      return Value();
                  })},
                 {"listen", make_native_function("web.websocket_server.listen", 1, [server_state](Interpreter& interpreter,
                                                                                                   const std::vector<Value>& args) -> Value {
                      serve_websocket_server(interpreter, server_state, static_cast<int>(args[0].as_number()));
                      return Value();
                  })},
             });
         })},
        {"router", make_native_function("web.router", 0, [](Interpreter&, const std::vector<Value>&) -> Value {
             auto router_state = std::make_shared<RouterState>();
             return make_object({
                 {"use", make_native_function("web.router.use", 1, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[0].is_callable()) {
                          throw RuntimeError("middleware must be callable");
                      }
                      router_state->middlewares.push_back(args[0]);
                      return Value();
                  })},
                 {"get", make_native_function("web.router.get", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["GET"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"post", make_native_function("web.router.post", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["POST"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"put", make_native_function("web.router.put", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["PUT"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"patch", make_native_function("web.router.patch", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["PATCH"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"delete", make_native_function("web.router.delete", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["DELETE"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"options", make_native_function("web.router.options", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->routes["OPTIONS"].push_back(RouteEntry{normalize_route_path(args[0].as_string()), args[1]});
                      return Value();
                  })},
                 {"static", make_native_function("web.router.static", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      const std::filesystem::path root = std::filesystem::weakly_canonical(args[1].as_string());
                      if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
                          throw RuntimeError("static root does not exist: " + root.string());
                      }
                      router_state->static_mounts.push_back(StaticMount{normalize_route_path(args[0].as_string()), root});
                      return Value();
                  })},
                 {"cors", make_native_function("web.router.cors", 1, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      router_state->cors_origin = args[0].as_string();
                      return Value();
                  })},
                 {"ws", make_native_function("web.router.ws", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      router_state->websocket_routes.push_back(
                          WebSocketRoute{normalize_route_path(args[0].as_string()), websocket_handlers_from_value(args[1])});
                      return Value();
                  })},
                 {"listen", make_native_function("web.router.listen", 1, [router_state](Interpreter& interpreter,
                                                                                           const std::vector<Value>& args) -> Value {
                      serve_router_app(interpreter, router_state, static_cast<int>(args[0].as_number()));
                      return Value();
                  })},
             });
        })},
    });

    auto web_fields = stdlib_modules_["web"].as_object()->fields;
    web_fields["backend"] = web_fields["router"];
    web_fields["app"] = web_fields["router"];
    web_fields["api"] = web_fields["router"];
    web_fields["view"] = web_fields["render"];
    stdlib_modules_["web"] = make_object(std::move(web_fields));

    stdlib_modules_["http"] = stdlib_modules_["web"];
}

void Interpreter::interpret(const ast::Program& program) {
    try {
        for (const auto& statement : program.statements) execute(*statement);
    } catch (const ReturnSignal&) {
        throw RuntimeError("return used outside of function");
    } catch (const ThrownSignal& signal) {
        throw RuntimeError("uncaught throw: " + render_value(signal.value, false));
    } catch (const BreakSignal&) {
        throw RuntimeError("break used outside of loop");
    } catch (const ContinueSignal&) {
        throw RuntimeError("continue used outside of loop");
    }
}

void Interpreter::execute_block(const std::vector<ast::StmtPtr>& statements, std::shared_ptr<Environment> environment) {
    const std::shared_ptr<Environment> previous = environment_;
    environment_ = std::move(environment);
    deferred_blocks_.push_back({});

    auto run_defers = [this]() {
        auto deferred = std::move(deferred_blocks_.back());
        deferred_blocks_.pop_back();
        for (auto it = deferred.rbegin(); it != deferred.rend(); ++it) {
            const std::shared_ptr<Environment> saved = environment_;
            environment_ = it->second;
            try {
                static_cast<void>(evaluate(*it->first));
            } catch (...) {
                environment_ = saved;
                throw;
            }
            environment_ = saved;
        }
    };

    try {
        for (const auto& statement : statements) execute(*statement);
    } catch (...) {
        try {
            run_defers();
        } catch (...) {
            environment_ = previous;
            throw;
        }
        environment_ = previous;
        throw;
    }
    run_defers();
    environment_ = previous;
}

runtime::Value Interpreter::load_module(const std::string& module_name) {
    const auto builtin = stdlib_modules_.find(module_name);
    if (builtin != stdlib_modules_.end()) return builtin->second;

    const std::filesystem::path module_path = resolve_module_path(module_name);
    const std::string cache_key = std::filesystem::weakly_canonical(module_path).string();
    const auto cached = module_cache_.find(cache_key);
    if (cached != module_cache_.end()) return cached->second;

    Value module = run_module_file(module_path);
    module_cache_[cache_key] = module;
    return module;
}

std::filesystem::path Interpreter::resolve_module_path(const std::string& module_name) const {
    std::string relative_module = module_name;
    std::replace(relative_module.begin(), relative_module.end(), '.', '/');
    const std::filesystem::path relative_path = std::filesystem::path(relative_module).replace_extension(".lunara");
    const std::vector<std::string> module_segments = split_string(module_name, '.');

    std::vector<std::filesystem::path> bases;
    if (!script_stack_.empty()) bases.push_back(script_stack_.back().parent_path());
    bases.push_back(std::filesystem::current_path());

    for (const auto& base : bases) {
        const std::filesystem::path direct = base / relative_path;
        if (std::filesystem::exists(direct)) return direct;

        const std::filesystem::path src_candidate = base / "src" / relative_path;
        if (std::filesystem::exists(src_candidate)) return src_candidate;

        const std::filesystem::path lib_candidate = base / "lib" / relative_path;
        if (std::filesystem::exists(lib_candidate)) return lib_candidate;

        if (const auto workspace_root = find_workspace_root(base)) {
            if (const auto packaged = resolve_from_package_root(*workspace_root, module_name, module_segments)) {
                return *packaged;
            }
        }
    }

    throw RuntimeError("module not found: '" + module_name + "'");
}

runtime::Value Interpreter::run_module_file(const std::filesystem::path& path) {
    const std::string source = read_text_file(path);
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    Parser parser(std::move(tokens), source);
    auto program = std::make_shared<ast::Program>(parser.parse());
    owned_programs_.push_back(program);

    auto module_environment = std::make_shared<Environment>(globals_);
    script_stack_.push_back(std::filesystem::absolute(path));
    try {
        execute_block(program->statements, module_environment);
    } catch (const ReturnSignal&) {
        script_stack_.pop_back();
        throw RuntimeError("return used outside of function in module " + path.string());
    } catch (const ThrownSignal& signal) {
        script_stack_.pop_back();
        throw RuntimeError("uncaught throw in module " + path.string() + ": " + render_value(signal.value, false));
    } catch (const BreakSignal&) {
        script_stack_.pop_back();
        throw RuntimeError("break used outside of loop in module " + path.string());
    } catch (const ContinueSignal&) {
        script_stack_.pop_back();
        throw RuntimeError("continue used outside of loop in module " + path.string());
    } catch (...) {
        script_stack_.pop_back();
        throw;
    }
    script_stack_.pop_back();

    return make_object(module_environment->exported_values());
}

void Interpreter::execute(const ast::Stmt& statement) {
    if (const auto* expr_stmt = dynamic_cast<const ast::ExpressionStmt*>(&statement)) {
        static_cast<void>(evaluate(*expr_stmt->expression));
        return;
    }
    if (const auto* var_stmt = dynamic_cast<const ast::VarStmt*>(&statement)) {
        const Value value = evaluate(*var_stmt->initializer);
        environment_->define(var_stmt->name, value, var_stmt->is_const, var_stmt->type_hint);
        return;
    }
    if (const auto* import_stmt = dynamic_cast<const ast::ImportStmt*>(&statement)) {
        environment_->define(import_stmt->binding_name, load_module(import_stmt->module_name), true);
        return;
    }
    if (const auto* function_stmt = dynamic_cast<const ast::FunctionStmt*>(&statement)) {
        environment_->define(function_stmt->name, Value(std::make_shared<UserFunction>(function_stmt, environment_)), true);
        return;
    }
    if (const auto* if_stmt = dynamic_cast<const ast::IfStmt*>(&statement)) {
        for (const auto& branch : if_stmt->branches) {
            if (evaluate(*branch.condition).is_truthy()) {
                execute_block(branch.body, std::make_shared<Environment>(environment_));
                return;
            }
        }
        if (!if_stmt->else_branch.empty()) execute_block(if_stmt->else_branch, std::make_shared<Environment>(environment_));
        return;
    }
    if (const auto* while_stmt = dynamic_cast<const ast::WhileStmt*>(&statement)) {
        while (evaluate(*while_stmt->condition).is_truthy()) {
            try {
                execute_block(while_stmt->body, std::make_shared<Environment>(environment_));
            } catch (const ContinueSignal&) {
                continue;
            } catch (const BreakSignal&) {
                break;
            }
        }
        return;
    }
    if (const auto* match_stmt = dynamic_cast<const ast::MatchStmt*>(&statement)) {
        const Value subject = evaluate(*match_stmt->expression);
        for (const auto& match_case : match_stmt->cases) {
            auto match_env = std::make_shared<Environment>(environment_);
            if (bind_match_pattern(match_case.pattern, subject, match_env)) {
                execute_block(match_case.body, match_env);
                return;
            }
        }
        if (!match_stmt->else_branch.empty()) {
            execute_block(match_stmt->else_branch, std::make_shared<Environment>(environment_));
        }
        return;
    }
    if (const auto* for_stmt = dynamic_cast<const ast::ForInStmt*>(&statement)) {
        const Value iterable = evaluate(*for_stmt->iterable);
        if (iterable.is_list()) {
            const auto items = iterable.as_list()->items;
            for (std::size_t i = 0; i < items.size(); ++i) {
                auto loop_env = std::make_shared<Environment>(environment_);
                if (for_stmt->second_name.has_value()) {
                    loop_env->define(for_stmt->name, Value(static_cast<double>(i)), false);
                    loop_env->define(*for_stmt->second_name, items[i], false);
                } else {
                    loop_env->define(for_stmt->name, items[i], false);
                }
                try {
                    execute_block(for_stmt->body, loop_env);
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
            }
            return;
        }
        if (iterable.is_object()) {
            for (const auto& [key, value] : iterable.as_object()->fields) {
                auto loop_env = std::make_shared<Environment>(environment_);
                loop_env->define(for_stmt->name, Value(key), false);
                if (for_stmt->second_name.has_value()) {
                    loop_env->define(*for_stmt->second_name, value, false);
                }
                try {
                    execute_block(for_stmt->body, loop_env);
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
            }
            return;
        }
        throw RuntimeError("for-in expects a list or object iterable");
    }
    if (const auto* return_stmt = dynamic_cast<const ast::ReturnStmt*>(&statement)) {
        throw ReturnSignal(return_stmt->value ? evaluate(*return_stmt->value) : Value());
    }
    if (const auto* defer_stmt = dynamic_cast<const ast::DeferStmt*>(&statement)) {
        if (deferred_blocks_.empty()) {
            throw RuntimeError("defer used outside of block");
        }
        deferred_blocks_.back().push_back({defer_stmt->expression.get(), environment_});
        return;
    }
    if (const auto* throw_stmt = dynamic_cast<const ast::ThrowStmt*>(&statement)) {
        throw ThrownSignal(evaluate(*throw_stmt->value));
    }
    if (const auto* try_stmt = dynamic_cast<const ast::TryStmt*>(&statement)) {
        std::exception_ptr pending_exception;

        try {
            execute_block(try_stmt->body, std::make_shared<Environment>(environment_));
        } catch (const ThrownSignal& signal) {
            if (!try_stmt->catch_branch.empty()) {
                auto catch_env = std::make_shared<Environment>(environment_);
                if (try_stmt->catch_name.has_value()) {
                    catch_env->define(*try_stmt->catch_name, signal.value, false);
                }
                try {
                    execute_block(try_stmt->catch_branch, catch_env);
                } catch (...) {
                    pending_exception = std::current_exception();
                }
            } else {
                pending_exception = std::current_exception();
            }
        } catch (const RuntimeError& error) {
            if (!try_stmt->catch_branch.empty()) {
                auto catch_env = std::make_shared<Environment>(environment_);
                if (try_stmt->catch_name.has_value()) {
                    catch_env->define(*try_stmt->catch_name, runtime_error_value(error.what()), false);
                }
                try {
                    execute_block(try_stmt->catch_branch, catch_env);
                } catch (...) {
                    pending_exception = std::current_exception();
                }
            } else {
                pending_exception = std::current_exception();
            }
        } catch (...) {
            pending_exception = std::current_exception();
        }

        if (!try_stmt->finally_branch.empty()) {
            try {
                execute_block(try_stmt->finally_branch, std::make_shared<Environment>(environment_));
            } catch (...) {
                pending_exception = std::current_exception();
            }
        }

        if (pending_exception) {
            std::rethrow_exception(pending_exception);
        }
        return;
    }
    if (dynamic_cast<const ast::BreakStmt*>(&statement)) {
        throw BreakSignal();
    }
    if (dynamic_cast<const ast::ContinueStmt*>(&statement)) {
        throw ContinueSignal();
    }
    throw RuntimeError("unknown statement type");
}

Value Interpreter::evaluate(const ast::Expr& expression) {
    if (const auto* literal = dynamic_cast<const ast::LiteralExpr*>(&expression)) return literal_to_value(literal->value);
    if (const auto* grouping = dynamic_cast<const ast::GroupingExpr*>(&expression)) return evaluate(*grouping->expression);
    if (const auto* variable = dynamic_cast<const ast::VariableExpr*>(&expression)) return environment_->get(variable->name);
    if (const auto* assign = dynamic_cast<const ast::AssignExpr*>(&expression)) {
        const Value value = evaluate(*assign->value);
        environment_->assign(assign->name, value);
        return value;
    }
    if (const auto* set_member = dynamic_cast<const ast::SetMemberExpr*>(&expression)) {
        const Value object = evaluate(*set_member->object);
        if (!object.is_object()) {
            throw RuntimeError("attempted member assignment on " + object.type_name());
        }
        const Value value = evaluate(*set_member->value);
        object.as_object()->fields[set_member->name] = value;
        return value;
    }
    if (const auto* set_index = dynamic_cast<const ast::SetIndexExpr*>(&expression)) {
        const Value object = evaluate(*set_index->object);
        const Value index = evaluate(*set_index->index);
        const Value value = evaluate(*set_index->value);
        if (object.is_list()) {
            const auto list_value = object.as_list();
            const std::size_t list_index = number_to_index(index.as_number());
            if (list_index > list_value->items.size()) {
                throw RuntimeError("list assignment index out of range");
            }
            if (list_index == list_value->items.size()) {
                list_value->items.push_back(value);
            } else {
                list_value->items[list_index] = value;
            }
            return value;
        }
        if (object.is_object()) {
            object.as_object()->fields[index.as_string()] = value;
            return value;
        }
        throw RuntimeError("index assignment is only supported on list and object values");
    }
    if (const auto* list_expr = dynamic_cast<const ast::ListExpr*>(&expression)) {
        std::vector<Value> items;
        items.reserve(list_expr->elements.size());
        for (const auto& element : list_expr->elements) items.push_back(evaluate(*element));
        return make_list(std::move(items));
    }
    if (const auto* object_expr = dynamic_cast<const ast::ObjectExpr*>(&expression)) {
        std::map<std::string, Value> fields;
        for (const auto& entry : object_expr->entries) fields[entry.key] = evaluate(*entry.value);
        return make_object(std::move(fields));
    }
    if (const auto* member_expr = dynamic_cast<const ast::MemberExpr*>(&expression)) {
        const Value object = evaluate(*member_expr->object);
        if (!object.is_object()) throw RuntimeError("attempted member access on " + object.type_name());
        const auto object_value = object.as_object();
        const auto it = object_value->fields.find(member_expr->name);
        if (it == object_value->fields.end()) throw RuntimeError("object has no field '" + member_expr->name + "'");
        return it->second;
    }
    if (const auto* index_expr = dynamic_cast<const ast::IndexExpr*>(&expression)) {
        const Value object = evaluate(*index_expr->object);
        const Value index = evaluate(*index_expr->index);
        if (object.is_list()) {
            const auto list_value = object.as_list();
            const std::size_t list_index = number_to_index(index.as_number());
            if (list_index >= list_value->items.size()) throw RuntimeError("list index out of range");
            return list_value->items[list_index];
        }
        if (object.is_object()) {
            const auto object_value = object.as_object();
            const auto it = object_value->fields.find(index.as_string());
            if (it == object_value->fields.end()) return Value();
            return it->second;
        }
        throw RuntimeError("index access is only supported on list and object values");
    }
    if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expression)) {
        const Value right = evaluate(*unary->right);
        switch (unary->op.type) {
            case TokenType::Minus: return Value(-right.as_number());
            case TokenType::Not: return Value(!right.is_truthy());
            default: break;
        }
        throw RuntimeError("unsupported unary operator '" + unary->op.lexeme + "'");
    }
    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expression)) {
        if (binary->op.type == TokenType::Or) {
            const Value left = evaluate(*binary->left);
            return left.is_truthy() ? left : evaluate(*binary->right);
        }
        if (binary->op.type == TokenType::And) {
            const Value left = evaluate(*binary->left);
            return !left.is_truthy() ? left : evaluate(*binary->right);
        }

        const Value left = evaluate(*binary->left);
        const Value right = evaluate(*binary->right);
        switch (binary->op.type) {
            case TokenType::Plus:
                if (left.is_number() && right.is_number()) return Value(left.as_number() + right.as_number());
                return Value(left.to_string() + right.to_string());
            case TokenType::Minus:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() - right.as_number());
            case TokenType::Star:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() * right.as_number());
            case TokenType::Slash:
                require_number_operands(binary->op, left, right);
                if (std::abs(right.as_number()) < 1e-12) throw RuntimeError("division by zero");
                return Value(left.as_number() / right.as_number());
            case TokenType::Percent:
                require_number_operands(binary->op, left, right);
                if (std::abs(right.as_number()) < 1e-12) throw RuntimeError("modulo by zero");
                return Value(std::fmod(left.as_number(), right.as_number()));
            case TokenType::Greater:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() > right.as_number());
            case TokenType::GreaterEqual:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() >= right.as_number());
            case TokenType::Less:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() < right.as_number());
            case TokenType::LessEqual:
                require_number_operands(binary->op, left, right);
                return Value(left.as_number() <= right.as_number());
            case TokenType::EqualEqual: return Value(left == right);
            case TokenType::BangEqual: return Value(left != right);
            default: break;
        }
        throw RuntimeError("unsupported binary operator '" + binary->op.lexeme + "'");
    }
    if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expression)) {
        const Value callee = evaluate(*call->callee);
        if (!callee.is_callable()) throw RuntimeError("attempted to call a non-function value of type " + callee.type_name());
        auto callable = callee.as_callable();
        std::vector<Value> arguments;
        arguments.reserve(call->arguments.size());
        for (const auto& argument : call->arguments) arguments.push_back(evaluate(*argument));
        if (arguments.size() != callable->arity()) {
            throw RuntimeError(callable->debug_name() + " expects " + std::to_string(callable->arity()) + " argument(s), got " +
                               std::to_string(arguments.size()));
        }
        return callable->call(*this, arguments);
    }
    if (const auto* lambda = dynamic_cast<const ast::LambdaExpr*>(&expression)) {
        return Value(std::make_shared<LambdaFunction>(lambda, environment_));
    }
    throw RuntimeError("unknown expression type");
}

Value UserFunction::call(Interpreter& interpreter, const std::vector<Value>& arguments) {
    auto environment = std::make_shared<Environment>(closure_);
    for (std::size_t i = 0; i < declaration_->params.size(); ++i) {
        ensure_type_hint(arguments[i], declaration_->params[i].type_hint, "argument '" + declaration_->params[i].name + "'");
        environment->define(declaration_->params[i].name, arguments[i], false, declaration_->params[i].type_hint);
    }
    try {
        interpreter.execute_block(declaration_->body, environment);
    } catch (const ReturnSignal& signal) {
        ensure_type_hint(signal.value, declaration_->return_type, "return value");
        return signal.value;
    }
    ensure_type_hint(Value(), declaration_->return_type, "return value");
    return Value();
}

Value LambdaFunction::call(Interpreter& interpreter, const std::vector<Value>& arguments) {
    auto environment = std::make_shared<Environment>(closure_);
    for (std::size_t i = 0; i < declaration_->params.size(); ++i) {
        ensure_type_hint(arguments[i], declaration_->params[i].type_hint, "argument '" + declaration_->params[i].name + "'");
        environment->define(declaration_->params[i].name, arguments[i], false, declaration_->params[i].type_hint);
    }
    try {
        interpreter.execute_block(declaration_->body, environment);
    } catch (const ReturnSignal& signal) {
        ensure_type_hint(signal.value, declaration_->return_type, "return value");
        return signal.value;
    }
    ensure_type_hint(Value(), declaration_->return_type, "return value");
    return Value();
}

}  // namespace lunara

