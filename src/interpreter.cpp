#include "pylua/interpreter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
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
#include <random>
#include <sstream>
#include <stdexcept>
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

#include "pylua/lexer.hpp"
#include "pylua/parser.hpp"

namespace pylua::runtime {

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

void Environment::define(const std::string& name, const Value& value, bool is_const) { values_[name] = Binding{value, is_const}; }

void Environment::assign(const std::string& name, const Value& value) {
    const auto it = values_.find(name);
    if (it != values_.end()) {
        if (it->second.is_const) {
            throw RuntimeError("cannot assign to const variable '" + name + "'");
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

}  // namespace pylua::runtime

namespace pylua {

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

Value literal_to_value(const ast::LiteralValue& literal) {
    if (std::holds_alternative<std::monostate>(literal)) return Value();
    if (const auto* number = std::get_if<double>(&literal)) return Value(*number);
    if (const auto* boolean = std::get_if<bool>(&literal)) return Value(*boolean);
    return Value(std::get<std::string>(literal));
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

struct RouterState {
    std::map<std::string, Value> get_routes;
    std::map<std::string, Value> post_routes;
    std::vector<Value> middlewares;
};

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

Value request_to_value(const HttpRequestData& request) {
    std::map<std::string, Value> query_fields;
    for (const auto& [key, value] : request.query) {
        query_fields[key] = Value(value);
    }

    std::map<std::string, Value> header_fields;
    for (const auto& [key, value] : request.headers) {
        header_fields[key] = Value(value);
    }

    std::map<std::string, Value> fields = {
        {"method", Value(request.method)},
        {"target", Value(request.target)},
        {"path", Value(request.path.empty() ? "/" : request.path)},
        {"version", Value(request.version)},
        {"body", Value(request.body)},
        {"query", make_object(std::move(query_fields))},
        {"headers", make_object(std::move(header_fields))},
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

    std::cout << "PyLua web server listening on http://127.0.0.1:" << port << '\n';
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

            const std::string request(buffer, buffer + received);
            const std::size_t line_end = request.find("\r\n");
            const std::string request_line = request.substr(0, line_end);

            std::istringstream line_stream(request_line);
            std::string method;
            std::string target;
            std::string version;
            line_stream >> method >> target >> version;

            if (method != "GET") {
                send_all(client_socket, build_http_response(403, "text/plain; charset=utf-8", "Only GET is supported"));
                close_socket(client_socket);
                continue;
            }

            try {
                const std::filesystem::path requested_path = sanitize_request_path(root, target);
                const std::filesystem::path canonical_requested = std::filesystem::weakly_canonical(requested_path);
                const std::string canonical_string = canonical_requested.string();
                const std::string root_string = root.string();
                if (canonical_string.rfind(root_string, 0) != 0 || !std::filesystem::exists(canonical_requested) ||
                    std::filesystem::is_directory(canonical_requested)) {
                    send_all(client_socket, build_http_response(404, "text/plain; charset=utf-8", "Not found"));
                } else {
                    send_all(client_socket, build_http_response(200, mime_type_for_path(canonical_requested), read_text_file(canonical_requested)));
                }
            } catch (const RuntimeError&) {
                send_all(client_socket, build_http_response(403, "text/plain; charset=utf-8", "Forbidden"));
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

    std::cout << "PyLua router listening on http://127.0.0.1:" << port << '\n';

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
            if (request.method != "GET" && request.method != "POST") {
                send_all(client_socket, build_http_response(405, "text/plain; charset=utf-8", "Method not allowed"));
                close_socket(client_socket);
                continue;
            }

            const Value request_value = request_to_value(request);
            const Value context_value = make_object({{"request", request_value}});

            bool handled = false;
            for (const auto& middleware : router_state->middlewares) {
                const Value middleware_result = invoke_callable(interpreter, middleware, {context_value});
                if (!middleware_result.is_nil()) {
                    const HttpResponseData response = normalize_http_response(middleware_result);
                    send_all(client_socket, build_http_response(response.status, response.content_type, response.body, response.headers));
                    handled = true;
                    break;
                }
            }

            if (handled) {
                close_socket(client_socket);
                continue;
            }

            const auto& routes = request.method == "POST" ? router_state->post_routes : router_state->get_routes;
            const auto route_it = routes.find(request.path.empty() ? "/" : request.path);
            if (route_it == routes.end()) {
                send_all(client_socket, build_http_response(404, "text/plain; charset=utf-8", "Route not found"));
                close_socket(client_socket);
                continue;
            }

            const Value handler_result = invoke_callable(interpreter, route_it->second, {context_value});
            const HttpResponseData response = normalize_http_response(handler_result);
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
             const auto alphabet = std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
             std::random_device random_device;
             std::mt19937 generator(random_device());
             std::uniform_int_distribution<std::size_t> distribution(0, alphabet.size() - 1);
             std::string token;
             const auto length = static_cast<std::size_t>(args[0].as_number());
             token.reserve(length);
             for (std::size_t i = 0; i < length; ++i) {
                 token.push_back(alphabet[distribution(generator)]);
             }
             return Value(token);
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
        {"text", make_native_function("web.text", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[0].as_number()), args[1], "text/plain; charset=utf-8");
         })},
        {"json", make_native_function("web.json", 2, [](Interpreter&, const std::vector<Value>& args) -> Value {
             return make_http_response_value(static_cast<int>(args[1].as_number()), Value(render_value(args[0], true)),
                                             "application/json; charset=utf-8");
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
                      router_state->get_routes[args[0].as_string()] = args[1];
                      return Value();
                  })},
                 {"post", make_native_function("web.router.post", 2, [router_state](Interpreter&, const std::vector<Value>& args) -> Value {
                      if (!args[1].is_callable()) {
                          throw RuntimeError("route handler must be callable");
                      }
                      router_state->post_routes[args[0].as_string()] = args[1];
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

    stdlib_modules_["http"] = stdlib_modules_["web"];
}

void Interpreter::interpret(const ast::Program& program) {
    try {
        for (const auto& statement : program.statements) execute(*statement);
    } catch (const ReturnSignal&) {
        throw RuntimeError("return used outside of function");
    }
}

void Interpreter::execute_block(const std::vector<ast::StmtPtr>& statements, std::shared_ptr<Environment> environment) {
    const std::shared_ptr<Environment> previous = environment_;
    environment_ = std::move(environment);
    try {
        for (const auto& statement : statements) execute(*statement);
    } catch (...) {
        environment_ = previous;
        throw;
    }
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
    const std::filesystem::path relative_path = std::filesystem::path(relative_module).replace_extension(".pylua");

    std::vector<std::filesystem::path> bases;
    if (!script_stack_.empty()) bases.push_back(script_stack_.back().parent_path());
    bases.push_back(std::filesystem::current_path());

    for (const auto& base : bases) {
        const std::filesystem::path direct = base / relative_path;
        if (std::filesystem::exists(direct)) return direct;

        const std::filesystem::path lib_candidate = base / "lib" / relative_path;
        if (std::filesystem::exists(lib_candidate)) return lib_candidate;
    }

    throw RuntimeError("module not found: '" + module_name + "'");
}

runtime::Value Interpreter::run_module_file(const std::filesystem::path& path) {
    Lexer lexer(read_text_file(path));
    auto tokens = lexer.scan_tokens();
    Parser parser(std::move(tokens));
    auto program = std::make_shared<ast::Program>(parser.parse());
    owned_programs_.push_back(program);

    auto module_environment = std::make_shared<Environment>(globals_);
    script_stack_.push_back(std::filesystem::absolute(path));
    try {
        execute_block(program->statements, module_environment);
    } catch (const ReturnSignal&) {
        script_stack_.pop_back();
        throw RuntimeError("return used outside of function in module " + path.string());
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
        environment_->define(var_stmt->name, value, var_stmt->is_const);
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
        while (evaluate(*while_stmt->condition).is_truthy()) execute_block(while_stmt->body, std::make_shared<Environment>(environment_));
        return;
    }
    if (const auto* for_stmt = dynamic_cast<const ast::ForInStmt*>(&statement)) {
        const Value iterable = evaluate(*for_stmt->iterable);
        if (iterable.is_list()) {
            for (const auto& item : iterable.as_list()->items) {
                auto loop_env = std::make_shared<Environment>(environment_);
                loop_env->define(for_stmt->name, item, false);
                execute_block(for_stmt->body, loop_env);
            }
            return;
        }
        if (iterable.is_object()) {
            for (const auto& [key, value] : iterable.as_object()->fields) {
                static_cast<void>(value);
                auto loop_env = std::make_shared<Environment>(environment_);
                loop_env->define(for_stmt->name, Value(key), false);
                execute_block(for_stmt->body, loop_env);
            }
            return;
        }
        throw RuntimeError("for-in expects a list or object iterable");
    }
    if (const auto* return_stmt = dynamic_cast<const ast::ReturnStmt*>(&statement)) {
        throw ReturnSignal(return_stmt->value ? evaluate(*return_stmt->value) : Value());
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
    throw RuntimeError("unknown expression type");
}

Value UserFunction::call(Interpreter& interpreter, const std::vector<Value>& arguments) {
    auto environment = std::make_shared<Environment>(closure_);
    for (std::size_t i = 0; i < declaration_->params.size(); ++i) environment->define(declaration_->params[i], arguments[i], false);
    try {
        interpreter.execute_block(declaration_->body, environment);
    } catch (const ReturnSignal& signal) {
        return signal.value;
    }
    return Value();
}

}  // namespace pylua
