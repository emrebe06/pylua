#include "lunara/analyzer.hpp"

#include <cctype>
#include <functional>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "lunara/lexer.hpp"
#include "lunara/parser.hpp"

namespace lunara::analysis {

namespace {

struct ParsedType {
    std::string name;
    std::vector<ParsedType> args;
};

struct FunctionSignature {
    std::vector<ast::TypeHint> params;
    ast::TypeHint return_type;
};

struct Scope {
    std::unordered_map<std::string, ast::TypeHint> values;
    std::unordered_map<std::string, FunctionSignature> functions;
    Scope* parent = nullptr;

    std::optional<ast::TypeHint> get_value(const std::string& name) const {
        const auto it = values.find(name);
        if (it != values.end()) return it->second;
        return parent ? parent->get_value(name) : std::nullopt;
    }

    std::optional<FunctionSignature> get_function(const std::string& name) const {
        const auto it = functions.find(name);
        if (it != functions.end()) return it->second;
        return parent ? parent->get_function(name) : std::nullopt;
    }
};

void skip_spaces(std::string_view text, std::size_t& cursor) {
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
        ++cursor;
    }
}

ParsedType parse_type_spec(std::string_view text, std::size_t& cursor) {
    skip_spaces(text, cursor);
    ParsedType parsed;
    while (cursor < text.size()) {
        const char ch = text[cursor];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            parsed.name.push_back(ch);
            ++cursor;
            continue;
        }
        break;
    }
    skip_spaces(text, cursor);
    if (cursor < text.size() && text[cursor] == '<') {
        ++cursor;
        do {
            parsed.args.push_back(parse_type_spec(text, cursor));
            skip_spaces(text, cursor);
            if (cursor < text.size() && text[cursor] == ',') {
                ++cursor;
                continue;
            }
            break;
        } while (cursor < text.size());
        skip_spaces(text, cursor);
        if (cursor < text.size() && text[cursor] == '>') {
            ++cursor;
        }
    }
    return parsed;
}

ParsedType parse_type_spec(const ast::TypeHint& type_hint) {
    if (!type_hint.has_value()) {
        return {"any", {}};
    }
    std::size_t cursor = 0;
    return parse_type_spec(std::string_view(*type_hint), cursor);
}

std::string render_type_spec(const ParsedType& parsed) {
    if (parsed.args.empty()) {
        return parsed.name;
    }
    std::ostringstream out;
    out << parsed.name << "<";
    for (std::size_t i = 0; i < parsed.args.size(); ++i) {
        if (i > 0) out << ", ";
        out << render_type_spec(parsed.args[i]);
    }
    out << ">";
    return out.str();
}

ast::TypeHint make_type_hint(const ParsedType& parsed) {
    if (parsed.name.empty()) {
        return std::nullopt;
    }
    return render_type_spec(parsed);
}

bool type_compatible(const ast::TypeHint& expected, const ast::TypeHint& actual) {
    const ParsedType lhs = parse_type_spec(expected);
    const ParsedType rhs = parse_type_spec(actual);
    std::function<bool(const ParsedType&, const ParsedType&)> compatible = [&](const ParsedType& left, const ParsedType& right) -> bool {
        if (left.name.empty() || left.name == "any") return true;
        if (right.name.empty() || right.name == "any") return true;
        if (left.name != right.name) return false;
        if (left.args.empty() || right.args.empty()) return true;
        if (left.args.size() != right.args.size()) return false;
        for (std::size_t i = 0; i < left.args.size(); ++i) {
            if (!compatible(left.args[i], right.args[i])) return false;
        }
        return true;
    };
    return compatible(lhs, rhs);
}

ast::TypeHint literal_type(const ast::LiteralValue& literal) {
    if (std::holds_alternative<std::monostate>(literal)) return std::string("nil");
    if (std::holds_alternative<double>(literal)) return std::string("number");
    if (std::holds_alternative<bool>(literal)) return std::string("bool");
    return std::string("string");
}

ast::TypeHint infer_list_type(const ast::ListExpr& list_expr, Scope* scope, std::function<ast::TypeHint(const ast::Expr&, Scope*)> infer_expr) {
    ast::TypeHint shared_type = std::string("any");
    bool first = true;
    for (const auto& element : list_expr.elements) {
        const ast::TypeHint current = infer_expr(*element, scope);
        if (first) {
            shared_type = current.has_value() ? current : ast::TypeHint(std::string("any"));
            first = false;
            continue;
        }
        if (!type_compatible(shared_type, current) || !type_compatible(current, shared_type)) {
            shared_type = std::string("mixed");
            break;
        }
    }
    if (first) {
        shared_type = std::string("any");
    }
    return std::string("list<" + *shared_type + ">");
}

ast::TypeHint infer_object_type(const ast::ObjectExpr& object_expr, Scope* scope, std::function<ast::TypeHint(const ast::Expr&, Scope*)> infer_expr) {
    ast::TypeHint shared_type = std::string("any");
    bool first = true;
    for (const auto& entry : object_expr.entries) {
        const ast::TypeHint current = infer_expr(*entry.value, scope);
        if (first) {
            shared_type = current.has_value() ? current : ast::TypeHint(std::string("any"));
            first = false;
            continue;
        }
        if (!type_compatible(shared_type, current) || !type_compatible(current, shared_type)) {
            shared_type = std::string("mixed");
            break;
        }
    }
    if (first) {
        shared_type = std::string("any");
    }
    return std::string("object<" + *shared_type + ">");
}

void bind_pattern_types(const ast::MatchPattern& pattern, const ast::TypeHint& subject_type, Scope* scope) {
    switch (pattern.kind) {
        case ast::MatchPatternKind::Literal:
        case ast::MatchPatternKind::Wildcard:
            return;
        case ast::MatchPatternKind::Binding:
            scope->values[pattern.binding_name] = subject_type;
            return;
        case ast::MatchPatternKind::List: {
            const ParsedType parsed = parse_type_spec(subject_type);
            ast::TypeHint item_type = std::string("any");
            if (parsed.name == "list" && !parsed.args.empty()) {
                item_type = make_type_hint(parsed.args.front());
            }
            for (const auto& element : pattern.elements) {
                bind_pattern_types(element, item_type, scope);
            }
            return;
        }
        case ast::MatchPatternKind::Object: {
            const ParsedType parsed = parse_type_spec(subject_type);
            ast::TypeHint value_type = std::string("any");
            if (parsed.name == "object" && !parsed.args.empty()) {
                value_type = make_type_hint(parsed.args.back());
            }
            for (const auto& field : pattern.fields) {
                bind_pattern_types(*field.pattern, value_type, scope);
            }
            return;
        }
    }
}

class Analyzer {
  public:
    std::vector<Diagnostic> run(const ast::Program& program) {
        for (const auto& statement : program.statements) {
            register_function(*statement, &globals_);
        }
        for (const auto& statement : program.statements) {
            analyze_statement(*statement, &globals_, std::nullopt);
        }
        return diagnostics_;
    }

  private:
    ast::TypeHint infer_expr(const ast::Expr& expression, Scope* scope) {
        if (const auto* literal = dynamic_cast<const ast::LiteralExpr*>(&expression)) return literal_type(literal->value);
        if (const auto* list = dynamic_cast<const ast::ListExpr*>(&expression)) {
            return infer_list_type(*list, scope, [&](const ast::Expr& inner, Scope* nested_scope) { return infer_expr(inner, nested_scope); });
        }
        if (const auto* object = dynamic_cast<const ast::ObjectExpr*>(&expression)) {
            return infer_object_type(*object, scope, [&](const ast::Expr& inner, Scope* nested_scope) { return infer_expr(inner, nested_scope); });
        }
        if (const auto* variable = dynamic_cast<const ast::VariableExpr*>(&expression)) {
            if (const auto value = scope->get_value(variable->name)) return *value;
            return std::nullopt;
        }
        if (const auto* grouping = dynamic_cast<const ast::GroupingExpr*>(&expression)) {
            return infer_expr(*grouping->expression, scope);
        }
        if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expression)) {
            const auto right = infer_expr(*unary->right, scope);
            if (unary->op.type == TokenType::Not) return std::string("bool");
            return right;
        }
        if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expression)) {
            const auto left = infer_expr(*binary->left, scope);
            const auto right = infer_expr(*binary->right, scope);
            switch (binary->op.type) {
                case TokenType::Plus:
                    if (left.has_value() && *left == "string") return std::string("string");
                    if (right.has_value() && *right == "string") return std::string("string");
                    return std::string("number");
                case TokenType::Minus:
                case TokenType::Star:
                case TokenType::Slash:
                case TokenType::Percent:
                    return std::string("number");
                case TokenType::EqualEqual:
                case TokenType::BangEqual:
                case TokenType::Greater:
                case TokenType::GreaterEqual:
                case TokenType::Less:
                case TokenType::LessEqual:
                case TokenType::And:
                case TokenType::Or:
                    return std::string("bool");
                default:
                    return std::nullopt;
            }
        }
        if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expression)) {
            if (const auto* callee = dynamic_cast<const ast::VariableExpr*>(call->callee.get())) {
                if (callee->name == "len") return std::string("number");
                if (callee->name == "type") return std::string("string");
                if (callee->name == "keys" || callee->name == "values") return std::string("list");
                if (callee->name == "assert") return std::string("nil");
                if (const auto signature = scope->get_function(callee->name)) {
                    if (signature->params.size() == call->arguments.size()) {
                        for (std::size_t i = 0; i < call->arguments.size(); ++i) {
                            const auto arg_type = infer_expr(*call->arguments[i], scope);
                            if (!type_compatible(signature->params[i], arg_type)) {
                                diagnostics_.push_back(
                                    Diagnostic{"Type mismatch in call to '" + callee->name + "' for argument " + std::to_string(i + 1)});
                            }
                        }
                    }
                    return signature->return_type;
                }
            }
            return std::nullopt;
        }
        if (const auto* lambda = dynamic_cast<const ast::LambdaExpr*>(&expression)) {
            analyze_function_body(lambda->params, lambda->return_type, lambda->body, scope, "<lambda>");
            return std::string("function");
        }
        if (const auto* assign = dynamic_cast<const ast::AssignExpr*>(&expression)) {
            return infer_expr(*assign->value, scope);
        }
        if (const auto* member = dynamic_cast<const ast::MemberExpr*>(&expression)) {
            static_cast<void>(member);
            return std::nullopt;
        }
        if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expression)) {
            const ast::TypeHint object_type = infer_expr(*index->object, scope);
            const ParsedType parsed = parse_type_spec(object_type);
            if (parsed.name == "list" && !parsed.args.empty()) {
                return make_type_hint(parsed.args.front());
            }
            if (parsed.name == "object" && !parsed.args.empty()) {
                return make_type_hint(parsed.args.back());
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    void register_function(const ast::Stmt& statement, Scope* scope) {
        if (const auto* function_stmt = dynamic_cast<const ast::FunctionStmt*>(&statement)) {
            FunctionSignature signature;
            for (const auto& param : function_stmt->params) {
                signature.params.push_back(param.type_hint);
            }
            signature.return_type = function_stmt->return_type;
            scope->functions[function_stmt->name] = signature;
        }
    }

    void analyze_function_body(const std::vector<ast::FunctionParam>& params,
                               const ast::TypeHint& return_type,
                               const std::vector<ast::StmtPtr>& body,
                               Scope* parent,
                               const std::string& name) {
        Scope function_scope{.parent = parent};
        for (const auto& param : params) {
            function_scope.values[param.name] = param.type_hint;
        }
        for (const auto& statement : body) {
            analyze_statement(*statement, &function_scope, return_type);
        }
        static_cast<void>(name);
    }

    void analyze_statement(const ast::Stmt& statement, Scope* scope, const ast::TypeHint& expected_return) {
        if (const auto* var_stmt = dynamic_cast<const ast::VarStmt*>(&statement)) {
            const auto actual = infer_expr(*var_stmt->initializer, scope);
            if (var_stmt->type_hint.has_value() && !type_compatible(var_stmt->type_hint, actual)) {
                diagnostics_.push_back(Diagnostic{"Variable '" + var_stmt->name + "' does not match declared type"});
            }
            scope->values[var_stmt->name] = var_stmt->type_hint.has_value() ? var_stmt->type_hint : actual;
            return;
        }
        if (const auto* expr_stmt = dynamic_cast<const ast::ExpressionStmt*>(&statement)) {
            static_cast<void>(infer_expr(*expr_stmt->expression, scope));
            return;
        }
        if (const auto* function_stmt = dynamic_cast<const ast::FunctionStmt*>(&statement)) {
            analyze_function_body(function_stmt->params, function_stmt->return_type, function_stmt->body, scope, function_stmt->name);
            return;
        }
        if (const auto* if_stmt = dynamic_cast<const ast::IfStmt*>(&statement)) {
            for (const auto& branch : if_stmt->branches) {
                Scope branch_scope{.parent = scope};
                for (const auto& inner : branch.body) analyze_statement(*inner, &branch_scope, expected_return);
            }
            Scope else_scope{.parent = scope};
            for (const auto& inner : if_stmt->else_branch) analyze_statement(*inner, &else_scope, expected_return);
            return;
        }
        if (const auto* while_stmt = dynamic_cast<const ast::WhileStmt*>(&statement)) {
            Scope loop_scope{.parent = scope};
            for (const auto& inner : while_stmt->body) analyze_statement(*inner, &loop_scope, expected_return);
            return;
        }
        if (const auto* for_stmt = dynamic_cast<const ast::ForInStmt*>(&statement)) {
            Scope loop_scope{.parent = scope};
            loop_scope.values[for_stmt->name] = std::nullopt;
            if (for_stmt->second_name.has_value()) loop_scope.values[*for_stmt->second_name] = std::nullopt;
            for (const auto& inner : for_stmt->body) analyze_statement(*inner, &loop_scope, expected_return);
            return;
        }
        if (const auto* match_stmt = dynamic_cast<const ast::MatchStmt*>(&statement)) {
            const auto subject_type = infer_expr(*match_stmt->expression, scope);
            for (const auto& match_case : match_stmt->cases) {
                Scope case_scope{.parent = scope};
                bind_pattern_types(match_case.pattern, subject_type, &case_scope);
                for (const auto& inner : match_case.body) analyze_statement(*inner, &case_scope, expected_return);
            }
            Scope else_scope{.parent = scope};
            for (const auto& inner : match_stmt->else_branch) analyze_statement(*inner, &else_scope, expected_return);
            return;
        }
        if (const auto* try_stmt = dynamic_cast<const ast::TryStmt*>(&statement)) {
            Scope try_scope{.parent = scope};
            for (const auto& inner : try_stmt->body) analyze_statement(*inner, &try_scope, expected_return);
            Scope catch_scope{.parent = scope};
            if (try_stmt->catch_name.has_value()) catch_scope.values[*try_stmt->catch_name] = std::string("object");
            for (const auto& inner : try_stmt->catch_branch) analyze_statement(*inner, &catch_scope, expected_return);
            Scope finally_scope{.parent = scope};
            for (const auto& inner : try_stmt->finally_branch) analyze_statement(*inner, &finally_scope, expected_return);
            return;
        }
        if (const auto* return_stmt = dynamic_cast<const ast::ReturnStmt*>(&statement)) {
            const auto actual = return_stmt->value ? infer_expr(*return_stmt->value, scope) : ast::TypeHint(std::string("nil"));
            if (expected_return.has_value() && !type_compatible(expected_return, actual)) {
                diagnostics_.push_back(Diagnostic{"Return statement does not match declared function return type"});
            }
            return;
        }
        if (const auto* throw_stmt = dynamic_cast<const ast::ThrowStmt*>(&statement)) {
            static_cast<void>(infer_expr(*throw_stmt->value, scope));
            return;
        }
        if (const auto* defer_stmt = dynamic_cast<const ast::DeferStmt*>(&statement)) {
            static_cast<void>(infer_expr(*defer_stmt->expression, scope));
            return;
        }
    }

    Scope globals_;
    std::vector<Diagnostic> diagnostics_;
};

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

std::vector<Diagnostic> analyze_program(const ast::Program& program) {
    Analyzer analyzer;
    return analyzer.run(program);
}

std::vector<Diagnostic> analyze_file(const std::filesystem::path& path) {
    const std::string source = read_text_file(path);
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    Parser parser(std::move(tokens), source);
    return analyze_program(parser.parse());
}

}  // namespace lunara::analysis
