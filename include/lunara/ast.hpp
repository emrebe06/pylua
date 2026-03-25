#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lunara/token.hpp"

namespace lunara::ast {

using LiteralValue = std::variant<std::monostate, double, bool, std::string>;
using TypeHint = std::optional<std::string>;

struct TypeExpression {
    std::string name;
    std::vector<TypeExpression> generic_args;
};

struct Expr {
    virtual ~Expr() = default;
};

struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct LiteralExpr final : Expr {
    explicit LiteralExpr(LiteralValue literal) : value(std::move(literal)) {}
    LiteralValue value;
};

struct VariableExpr final : Expr {
    explicit VariableExpr(std::string variable_name) : name(std::move(variable_name)) {}
    std::string name;
};

struct AssignExpr final : Expr {
    AssignExpr(std::string variable_name, ExprPtr assigned_value)
        : name(std::move(variable_name)), value(std::move(assigned_value)) {}

    std::string name;
    ExprPtr value;
};

struct SetMemberExpr final : Expr {
    SetMemberExpr(ExprPtr object_expr, std::string property_name, ExprPtr assigned_value)
        : object(std::move(object_expr)), name(std::move(property_name)), value(std::move(assigned_value)) {}

    ExprPtr object;
    std::string name;
    ExprPtr value;
};

struct SetIndexExpr final : Expr {
    SetIndexExpr(ExprPtr object_expr, ExprPtr index_expr, ExprPtr assigned_value)
        : object(std::move(object_expr)), index(std::move(index_expr)), value(std::move(assigned_value)) {}

    ExprPtr object;
    ExprPtr index;
    ExprPtr value;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token operator_token, ExprPtr rhs)
        : op(std::move(operator_token)), right(std::move(rhs)) {}

    Token op;
    ExprPtr right;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr lhs, Token operator_token, ExprPtr rhs)
        : left(std::move(lhs)), op(std::move(operator_token)), right(std::move(rhs)) {}

    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct GroupingExpr final : Expr {
    explicit GroupingExpr(ExprPtr inner) : expression(std::move(inner)) {}
    ExprPtr expression;
};

struct CallExpr final : Expr {
    CallExpr(ExprPtr called, std::vector<ExprPtr> args, Token closing_paren)
        : callee(std::move(called)), arguments(std::move(args)), paren(std::move(closing_paren)) {}

    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    Token paren;
};

struct FunctionParam {
    std::string name;
    TypeHint type_hint;
};

struct LambdaExpr final : Expr {
    LambdaExpr(std::vector<FunctionParam> function_params, TypeHint function_return_type, std::vector<StmtPtr> function_body)
        : params(std::move(function_params)), return_type(std::move(function_return_type)), body(std::move(function_body)) {}

    std::vector<FunctionParam> params;
    TypeHint return_type;
    std::vector<StmtPtr> body;
};

struct ListExpr final : Expr {
    explicit ListExpr(std::vector<ExprPtr> exprs) : elements(std::move(exprs)) {}
    std::vector<ExprPtr> elements;
};

struct ObjectEntry {
    std::string key;
    ExprPtr value;
};

struct ObjectExpr final : Expr {
    explicit ObjectExpr(std::vector<ObjectEntry> pairs) : entries(std::move(pairs)) {}
    std::vector<ObjectEntry> entries;
};

struct MemberExpr final : Expr {
    MemberExpr(ExprPtr object_expr, std::string property_name)
        : object(std::move(object_expr)), name(std::move(property_name)) {}

    ExprPtr object;
    std::string name;
};

struct IndexExpr final : Expr {
    IndexExpr(ExprPtr object_expr, ExprPtr index_expr)
        : object(std::move(object_expr)), index(std::move(index_expr)) {}

    ExprPtr object;
    ExprPtr index;
};

struct Stmt {
    virtual ~Stmt() = default;
};

struct ExpressionStmt final : Stmt {
    explicit ExpressionStmt(ExprPtr expr) : expression(std::move(expr)) {}
    ExprPtr expression;
};

struct VarStmt final : Stmt {
    VarStmt(std::string variable_name, TypeHint declared_type, ExprPtr init, bool constant)
        : name(std::move(variable_name)), type_hint(std::move(declared_type)), initializer(std::move(init)), is_const(constant) {}

    std::string name;
    TypeHint type_hint;
    ExprPtr initializer;
    bool is_const;
};

struct FunctionStmt final : Stmt {
    FunctionStmt(std::string function_name, std::vector<FunctionParam> function_params, TypeHint function_return_type, std::vector<StmtPtr> function_body)
        : name(std::move(function_name)), params(std::move(function_params)), return_type(std::move(function_return_type)), body(std::move(function_body)) {}

    std::string name;
    std::vector<FunctionParam> params;
    TypeHint return_type;
    std::vector<StmtPtr> body;
};

struct ImportStmt final : Stmt {
    ImportStmt(std::string imported_module_name, std::string imported_binding_name)
        : module_name(std::move(imported_module_name)), binding_name(std::move(imported_binding_name)) {}

    std::string module_name;
    std::string binding_name;
};

struct IfBranch {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct IfStmt final : Stmt {
    IfStmt(std::vector<IfBranch> branch_list, std::vector<StmtPtr> else_body)
        : branches(std::move(branch_list)), else_branch(std::move(else_body)) {}

    std::vector<IfBranch> branches;
    std::vector<StmtPtr> else_branch;
};

struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr loop_condition, std::vector<StmtPtr> loop_body)
        : condition(std::move(loop_condition)), body(std::move(loop_body)) {}

    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ForInStmt final : Stmt {
    ForInStmt(std::string loop_name, std::optional<std::string> loop_second_name, ExprPtr loop_iterable, std::vector<StmtPtr> loop_body)
        : name(std::move(loop_name)), second_name(std::move(loop_second_name)), iterable(std::move(loop_iterable)), body(std::move(loop_body)) {}

    std::string name;
    std::optional<std::string> second_name;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
};

struct ReturnStmt final : Stmt {
    explicit ReturnStmt(ExprPtr return_value) : value(std::move(return_value)) {}
    ExprPtr value;
};

struct DeferStmt final : Stmt {
    explicit DeferStmt(ExprPtr deferred_expression) : expression(std::move(deferred_expression)) {}
    ExprPtr expression;
};

struct ThrowStmt final : Stmt {
    explicit ThrowStmt(ExprPtr thrown_value) : value(std::move(thrown_value)) {}
    ExprPtr value;
};

struct TryStmt final : Stmt {
    TryStmt(std::vector<StmtPtr> try_body,
            std::optional<std::string> catch_binding_name,
            std::vector<StmtPtr> catch_body,
            std::vector<StmtPtr> finally_body)
        : body(std::move(try_body)),
          catch_name(std::move(catch_binding_name)),
          catch_branch(std::move(catch_body)),
          finally_branch(std::move(finally_body)) {}

    std::vector<StmtPtr> body;
    std::optional<std::string> catch_name;
    std::vector<StmtPtr> catch_branch;
    std::vector<StmtPtr> finally_branch;
};

enum class MatchPatternKind {
    Literal,
    Wildcard,
    Binding,
    List,
    Object,
};

struct MatchPattern;

struct MatchObjectFieldPattern {
    std::string key;
    std::unique_ptr<MatchPattern> pattern;
};

struct MatchPattern {
    MatchPatternKind kind = MatchPatternKind::Wildcard;
    LiteralValue literal;
    std::string binding_name;
    std::vector<MatchPattern> elements;
    std::vector<MatchObjectFieldPattern> fields;
};

struct MatchCase {
    MatchPattern pattern;
    std::vector<StmtPtr> body;
};

struct MatchStmt final : Stmt {
    MatchStmt(ExprPtr matched_expression, std::vector<MatchCase> match_cases, std::vector<StmtPtr> else_body)
        : expression(std::move(matched_expression)), cases(std::move(match_cases)), else_branch(std::move(else_body)) {}

    ExprPtr expression;
    std::vector<MatchCase> cases;
    std::vector<StmtPtr> else_branch;
};

struct BreakStmt final : Stmt {};

struct ContinueStmt final : Stmt {};

struct Program {
    std::vector<StmtPtr> statements;
};

}  // namespace lunara::ast

