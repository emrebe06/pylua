#include "lunara/parser.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace lunara {

namespace {

class ParseAbort final : public std::runtime_error {
  public:
    explicit ParseAbort(const std::string& message) : std::runtime_error(message) {}
};

std::string unescape_string_literal(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char ch = raw[i];
        if (ch != '\\' || i + 1 >= raw.size()) {
            result.push_back(ch);
            continue;
        }

        const char escaped = raw[++i];
        switch (escaped) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            default:
                result.push_back(escaped);
                break;
        }
    }

    return result;
}

}  // namespace

Parser::Parser(std::vector<Token> tokens, std::string source) : tokens_(std::move(tokens)), source_(std::move(source)) {}

ast::Program Parser::parse() {
    ast::Program program;
    while (!is_at_end()) {
        try {
            program.statements.push_back(declaration());
        } catch (const ParseAbort&) {
            synchronize();
        }
    }
    if (!errors_.empty()) {
        std::ostringstream out;
        out << "Parser found " << errors_.size() << " error(s):\n";
        for (const auto& error : errors_) {
            out << " - " << error << '\n';
        }
        throw std::runtime_error(out.str());
    }
    return program;
}

ast::StmtPtr Parser::declaration() {
    if (match({TokenType::Let})) {
        return variable_declaration(false);
    }
    if (match({TokenType::Const})) {
        return variable_declaration(true);
    }
    if (match({TokenType::Import})) {
        return import_statement();
    }
    if (match({TokenType::Func})) {
        return function_declaration();
    }
    return statement();
}

ast::StmtPtr Parser::variable_declaration(bool is_const) {
    const Token& name = consume(TokenType::Identifier, "expected variable name");
    ast::TypeHint type_hint = optional_type_hint();
    consume(TokenType::Equal, "expected '=' after variable name");
    return std::make_unique<ast::VarStmt>(name.lexeme, std::move(type_hint), expression(), is_const);
}

ast::StmtPtr Parser::import_statement() {
    const Token& first = consume(TokenType::Identifier, "expected module name after import");
    std::string module_name = first.lexeme;
    std::string binding_name = first.lexeme;

    while (match({TokenType::Dot})) {
        const Token& segment = consume(TokenType::Identifier, "expected module segment after '.'");
        module_name += "." + segment.lexeme;
        binding_name = segment.lexeme;
    }

    if (match({TokenType::As})) {
        binding_name = consume(TokenType::Identifier, "expected alias after 'as'").lexeme;
    }

    return std::make_unique<ast::ImportStmt>(module_name, binding_name);
}

ast::StmtPtr Parser::function_declaration() {
    const Token& name = consume(TokenType::Identifier, "expected function name");
    consume(TokenType::LeftParen, "expected '(' after function name");
    auto params = parameter_list();
    consume(TokenType::RightParen, "expected ')' after function parameters");
    ast::TypeHint return_type = optional_type_hint();
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after function body");
    return std::make_unique<ast::FunctionStmt>(name.lexeme, std::move(params), std::move(return_type), std::move(body));
}

ast::StmtPtr Parser::statement() {
    if (match({TokenType::For})) {
        return for_statement();
    }
    if (match({TokenType::If})) {
        return if_statement();
    }
    if (match({TokenType::While})) {
        return while_statement();
    }
    if (match({TokenType::Match})) {
        return match_statement();
    }
    if (match({TokenType::Return})) {
        return return_statement();
    }
    if (match({TokenType::Defer})) {
        return defer_statement();
    }
    if (match({TokenType::Throw})) {
        return throw_statement();
    }
    if (match({TokenType::Try})) {
        return try_statement();
    }
    if (match({TokenType::Break})) {
        return break_statement();
    }
    if (match({TokenType::Continue})) {
        return continue_statement();
    }
    return expression_statement();
}

ast::StmtPtr Parser::for_statement() {
    const Token& name = consume(TokenType::Identifier, "expected loop variable name");
    std::optional<std::string> second_name;
    if (match({TokenType::Comma})) {
        second_name = consume(TokenType::Identifier, "expected second loop variable name").lexeme;
    }
    consume(TokenType::In, "expected 'in' after loop variable");
    auto iterable = expression();
    consume(TokenType::Do, "expected 'do' after for iterable");
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after for loop");
    return std::make_unique<ast::ForInStmt>(name.lexeme, std::move(second_name), std::move(iterable), std::move(body));
}

ast::StmtPtr Parser::if_statement() {
    std::vector<ast::IfBranch> branches;
    ast::IfBranch first_branch;
    first_branch.condition = expression();
    consume(TokenType::Then, "expected 'then' after if condition");
    first_branch.body = block_until({TokenType::ElseIf, TokenType::Else, TokenType::End});
    branches.push_back(std::move(first_branch));

    while (match({TokenType::ElseIf})) {
        ast::IfBranch branch;
        branch.condition = expression();
        consume(TokenType::Then, "expected 'then' after elseif condition");
        branch.body = block_until({TokenType::ElseIf, TokenType::Else, TokenType::End});
        branches.push_back(std::move(branch));
    }

    std::vector<ast::StmtPtr> else_branch;
    if (match({TokenType::Else})) {
        else_branch = block_until({TokenType::End});
    }

    consume(TokenType::End, "expected 'end' after if statement");
    return std::make_unique<ast::IfStmt>(std::move(branches), std::move(else_branch));
}

ast::StmtPtr Parser::while_statement() {
    auto condition = expression();
    consume(TokenType::Do, "expected 'do' after while condition");
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after while body");
    return std::make_unique<ast::WhileStmt>(std::move(condition), std::move(body));
}

ast::StmtPtr Parser::match_statement() {
    auto subject = expression();
    consume(TokenType::Do, "expected 'do' after match expression");

    std::vector<ast::MatchCase> cases;
    while (match({TokenType::When})) {
        ast::MatchCase match_case;
        match_case.pattern = parse_match_pattern();
        consume(TokenType::Then, "expected 'then' after match pattern");
        match_case.body = block_until({TokenType::When, TokenType::Else, TokenType::End});
        cases.push_back(std::move(match_case));
    }

    std::vector<ast::StmtPtr> else_branch;
    if (match({TokenType::Else})) {
        else_branch = block_until({TokenType::End});
    }

    consume(TokenType::End, "expected 'end' after match statement");
    return std::make_unique<ast::MatchStmt>(std::move(subject), std::move(cases), std::move(else_branch));
}

ast::StmtPtr Parser::return_statement() {
    if (check(TokenType::End) || check(TokenType::Else) || check(TokenType::ElseIf) || check(TokenType::Break) ||
        check(TokenType::Continue) || check(TokenType::Catch) || check(TokenType::Finally) || check(TokenType::When) ||
        is_at_end()) {
        return std::make_unique<ast::ReturnStmt>(nullptr);
    }
    return std::make_unique<ast::ReturnStmt>(expression());
}

ast::StmtPtr Parser::defer_statement() {
    return std::make_unique<ast::DeferStmt>(expression());
}

ast::StmtPtr Parser::throw_statement() {
    return std::make_unique<ast::ThrowStmt>(expression());
}

ast::StmtPtr Parser::try_statement() {
    auto try_body = block_until({TokenType::Catch, TokenType::Finally, TokenType::End});

    bool has_catch = false;
    std::optional<std::string> catch_name;
    std::vector<ast::StmtPtr> catch_body;
    if (match({TokenType::Catch})) {
        has_catch = true;
        if (check(TokenType::Identifier)) {
            catch_name = advance().lexeme;
        }
        catch_body = block_until({TokenType::Finally, TokenType::End});
    }

    std::vector<ast::StmtPtr> finally_body;
    if (match({TokenType::Finally})) {
        finally_body = block_until({TokenType::End});
    }

    if (!has_catch && finally_body.empty()) {
        error(peek(), "try statement requires catch or finally");
    }

    consume(TokenType::End, "expected 'end' after try statement");
    return std::make_unique<ast::TryStmt>(std::move(try_body), std::move(catch_name), std::move(catch_body), std::move(finally_body));
}

ast::StmtPtr Parser::break_statement() {
    return std::make_unique<ast::BreakStmt>();
}

ast::StmtPtr Parser::continue_statement() {
    return std::make_unique<ast::ContinueStmt>();
}

ast::StmtPtr Parser::expression_statement() {
    return std::make_unique<ast::ExpressionStmt>(expression());
}

std::vector<ast::StmtPtr> Parser::block_until(const std::vector<TokenType>& terminators) {
    std::vector<ast::StmtPtr> statements;
    while (!is_at_end() && !is_block_terminator(terminators)) {
        statements.push_back(declaration());
    }
    return statements;
}

ast::TypeHint Parser::optional_type_hint() {
    if (!match({TokenType::Colon})) {
        return std::nullopt;
    }
    return type_expression_to_string(parse_type_expression());
}

ast::TypeExpression Parser::parse_type_expression() {
    const Token& name = consume(TokenType::Identifier, "expected type name after ':'");
    ast::TypeExpression expression{name.lexeme, {}};
    if (match({TokenType::Less})) {
        do {
            expression.generic_args.push_back(parse_type_expression());
        } while (match({TokenType::Comma}));
        consume(TokenType::Greater, "expected '>' after generic type arguments");
    }
    return expression;
}

std::string Parser::type_expression_to_string(const ast::TypeExpression& expression) const {
    if (expression.generic_args.empty()) {
        return expression.name;
    }

    std::ostringstream out;
    out << expression.name << "<";
    for (std::size_t i = 0; i < expression.generic_args.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << type_expression_to_string(expression.generic_args[i]);
    }
    out << ">";
    return out.str();
}

std::vector<ast::FunctionParam> Parser::parameter_list() {
    std::vector<ast::FunctionParam> params;
    if (!check(TokenType::RightParen)) {
        do {
            const Token& param_name = consume(TokenType::Identifier, "expected parameter name");
            params.push_back(ast::FunctionParam{param_name.lexeme, optional_type_hint()});
        } while (match({TokenType::Comma}));
    }
    return params;
}

ast::MatchPattern Parser::parse_match_pattern() {
    if (match({TokenType::LeftBracket})) {
        ast::MatchPattern pattern;
        pattern.kind = ast::MatchPatternKind::List;
        if (!check(TokenType::RightBracket)) {
            do {
                pattern.elements.push_back(parse_match_pattern());
            } while (match({TokenType::Comma}));
        }
        consume(TokenType::RightBracket, "expected ']' after list pattern");
        return pattern;
    }
    if (match({TokenType::LeftBrace})) {
        ast::MatchPattern pattern;
        pattern.kind = ast::MatchPatternKind::Object;
        if (!check(TokenType::RightBrace)) {
            do {
                std::string key;
                if (match({TokenType::Identifier})) {
                    key = previous().lexeme;
                } else if (match({TokenType::String})) {
                    const std::string& raw = previous().lexeme;
                    key = unescape_string_literal(raw.substr(1, raw.size() - 2));
                } else {
                    error(peek(), "expected identifier or string as object pattern key");
                }

                auto field_pattern = std::make_unique<ast::MatchPattern>();
                if (match({TokenType::Colon})) {
                    *field_pattern = parse_match_pattern();
                } else {
                    field_pattern->kind = ast::MatchPatternKind::Binding;
                    field_pattern->binding_name = key;
                }
                pattern.fields.push_back(ast::MatchObjectFieldPattern{key, std::move(field_pattern)});
            } while (match({TokenType::Comma}));
        }
        consume(TokenType::RightBrace, "expected '}' after object pattern");
        return pattern;
    }
    if (match({TokenType::False})) {
        return ast::MatchPattern{ast::MatchPatternKind::Literal, false, ""};
    }
    if (match({TokenType::True})) {
        return ast::MatchPattern{ast::MatchPatternKind::Literal, true, ""};
    }
    if (match({TokenType::Nil})) {
        return ast::MatchPattern{ast::MatchPatternKind::Literal, std::monostate{}, ""};
    }
    if (match({TokenType::Number})) {
        return ast::MatchPattern{ast::MatchPatternKind::Literal, std::stod(previous().lexeme), ""};
    }
    if (match({TokenType::String})) {
        const std::string& raw = previous().lexeme;
        return ast::MatchPattern{ast::MatchPatternKind::Literal, unescape_string_literal(raw.substr(1, raw.size() - 2)), ""};
    }
    if (match({TokenType::Identifier})) {
        const std::string& name = previous().lexeme;
        if (name == "_") {
            return ast::MatchPattern{ast::MatchPatternKind::Wildcard, std::monostate{}, ""};
        }
        return ast::MatchPattern{ast::MatchPatternKind::Binding, std::monostate{}, name};
    }
    error(peek(), "expected match pattern");
}

ast::ExprPtr Parser::expression() {
    return assignment();
}

ast::ExprPtr Parser::assignment() {
    auto expr = logical_or();

    if (match({TokenType::Equal})) {
        const Token equals = previous();
        auto value = assignment();

        if (auto* variable = dynamic_cast<ast::VariableExpr*>(expr.get())) {
            return std::make_unique<ast::AssignExpr>(variable->name, std::move(value));
        }
        if (auto* member = dynamic_cast<ast::MemberExpr*>(expr.get())) {
            return std::make_unique<ast::SetMemberExpr>(std::move(member->object), member->name, std::move(value));
        }
        if (auto* index = dynamic_cast<ast::IndexExpr*>(expr.get())) {
            return std::make_unique<ast::SetIndexExpr>(std::move(index->object), std::move(index->index), std::move(value));
        }

        error(equals, "invalid assignment target");
    }

    return expr;
}

ast::ExprPtr Parser::logical_or() {
    auto expr = logical_and();
    while (match({TokenType::Or})) {
        const Token op = previous();
        auto right = logical_and();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::logical_and() {
    auto expr = equality();
    while (match({TokenType::And})) {
        const Token op = previous();
        auto right = equality();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::equality() {
    auto expr = comparison();
    while (match({TokenType::EqualEqual, TokenType::BangEqual})) {
        const Token op = previous();
        auto right = comparison();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::comparison() {
    auto expr = term();
    while (match({TokenType::Greater, TokenType::GreaterEqual, TokenType::Less, TokenType::LessEqual})) {
        const Token op = previous();
        auto right = term();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::term() {
    auto expr = factor();
    while (match({TokenType::Plus, TokenType::Minus})) {
        const Token op = previous();
        auto right = factor();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::factor() {
    auto expr = unary();
    while (match({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        const Token op = previous();
        auto right = unary();
        expr = std::make_unique<ast::BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

ast::ExprPtr Parser::unary() {
    if (match({TokenType::Not, TokenType::Minus})) {
        const Token op = previous();
        auto right = unary();
        return std::make_unique<ast::UnaryExpr>(op, std::move(right));
    }
    return call();
}

ast::ExprPtr Parser::call() {
    auto expr = primary();

    while (true) {
        if (match({TokenType::LeftParen})) {
            std::vector<ast::ExprPtr> arguments;
            if (!check(TokenType::RightParen)) {
                do {
                    arguments.push_back(expression());
                } while (match({TokenType::Comma}));
            }

            const Token& paren = consume(TokenType::RightParen, "expected ')' after arguments");
            expr = std::make_unique<ast::CallExpr>(std::move(expr), std::move(arguments), paren);
        } else if (match({TokenType::Dot})) {
            const Token& name = consume(TokenType::Identifier, "expected property name after '.'");
            expr = std::make_unique<ast::MemberExpr>(std::move(expr), name.lexeme);
        } else if (match({TokenType::LeftBracket})) {
            auto index = expression();
            consume(TokenType::RightBracket, "expected ']' after index expression");
            expr = std::make_unique<ast::IndexExpr>(std::move(expr), std::move(index));
        } else {
            break;
        }
    }

    return expr;
}

ast::ExprPtr Parser::lambda_expression() {
    consume(TokenType::LeftParen, "expected '(' after func in lambda");
    auto params = parameter_list();
    consume(TokenType::RightParen, "expected ')' after lambda parameters");
    ast::TypeHint return_type = optional_type_hint();
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after lambda body");
    return std::make_unique<ast::LambdaExpr>(std::move(params), std::move(return_type), std::move(body));
}

ast::ExprPtr Parser::primary() {
    if (check(TokenType::Func)) {
        advance();
        return lambda_expression();
    }
    if (match({TokenType::False})) {
        return std::make_unique<ast::LiteralExpr>(false);
    }
    if (match({TokenType::True})) {
        return std::make_unique<ast::LiteralExpr>(true);
    }
    if (match({TokenType::Nil})) {
        return std::make_unique<ast::LiteralExpr>(std::monostate{});
    }
    if (match({TokenType::Number})) {
        return std::make_unique<ast::LiteralExpr>(std::stod(previous().lexeme));
    }
    if (match({TokenType::String})) {
        const std::string& raw = previous().lexeme;
        return std::make_unique<ast::LiteralExpr>(unescape_string_literal(raw.substr(1, raw.size() - 2)));
    }
    if (match({TokenType::LeftBracket})) {
        std::vector<ast::ExprPtr> elements;
        if (!check(TokenType::RightBracket)) {
            do {
                elements.push_back(expression());
            } while (match({TokenType::Comma}));
        }
        consume(TokenType::RightBracket, "expected ']' after list literal");
        return std::make_unique<ast::ListExpr>(std::move(elements));
    }
    if (match({TokenType::LeftBrace})) {
        std::vector<ast::ObjectEntry> entries;
        if (!check(TokenType::RightBrace)) {
            do {
                std::string key;
                if (match({TokenType::Identifier})) {
                    key = previous().lexeme;
                } else if (match({TokenType::String})) {
                    const std::string& raw = previous().lexeme;
                    key = unescape_string_literal(raw.substr(1, raw.size() - 2));
                } else {
                    error(peek(), "expected identifier or string as object key");
                }

                consume(TokenType::Colon, "expected ':' after object key");
                entries.push_back(ast::ObjectEntry{key, expression()});
            } while (match({TokenType::Comma}));
        }
        consume(TokenType::RightBrace, "expected '}' after object literal");
        return std::make_unique<ast::ObjectExpr>(std::move(entries));
    }
    if (match({TokenType::Identifier})) {
        return std::make_unique<ast::VariableExpr>(previous().lexeme);
    }
    if (match({TokenType::LeftParen})) {
        auto expr = expression();
        consume(TokenType::RightParen, "expected ')' after expression");
        return std::make_unique<ast::GroupingExpr>(std::move(expr));
    }

    error(peek(), "expected expression");
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::Eof;
}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        ++current_;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return type == TokenType::Eof;
    }
    return peek().type == type;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (const TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(peek(), message);
}

[[noreturn]] void Parser::error(const Token& token, const std::string& message) const {
    std::ostringstream out;
    out << "Parser error at line " << token.line << ", column " << token.column << ": " << message
        << " (found " << token_type_name(token.type) << ")";
    const std::string frame = code_frame_for_token(token);
    if (!frame.empty()) {
        out << '\n' << frame;
    }
    errors_.push_back(out.str());
    throw ParseAbort(out.str());
}

std::string Parser::code_frame_for_token(const Token& token) const {
    if (source_.empty() || token.line <= 0) {
        return {};
    }

    std::istringstream input(source_);
    std::string line_text;
    for (int current_line = 1; current_line <= token.line; ++current_line) {
        if (!std::getline(input, line_text)) {
            return {};
        }
    }

    std::ostringstream out;
    out << "   " << line_text << '\n';
    out << "   ";
    const int caret_column = std::max(token.column, 1);
    for (int i = 1; i < caret_column; ++i) {
        if (static_cast<std::size_t>(i - 1) < line_text.size() && line_text[static_cast<std::size_t>(i - 1)] == '\t') {
            out << '\t';
        } else {
            out << ' ';
        }
    }
    const std::size_t marker_width = std::max<std::size_t>(1, token.lexeme.empty() ? 1 : token.lexeme.size());
    for (std::size_t i = 0; i < marker_width; ++i) {
        out << '^';
    }
    return out.str();
}

bool Parser::is_block_terminator(const std::vector<TokenType>& terminators) const {
    for (const TokenType type : terminators) {
        if (check(type)) {
            return true;
        }
    }
    return false;
}

void Parser::synchronize() {
    if (is_at_end()) {
        return;
    }
    advance();
    while (!is_at_end()) {
        switch (peek().type) {
            case TokenType::Let:
            case TokenType::Const:
            case TokenType::Import:
            case TokenType::Func:
            case TokenType::For:
            case TokenType::If:
            case TokenType::While:
            case TokenType::Match:
            case TokenType::Return:
            case TokenType::Defer:
            case TokenType::Throw:
            case TokenType::Try:
            case TokenType::Catch:
            case TokenType::Finally:
            case TokenType::Else:
            case TokenType::ElseIf:
            case TokenType::When:
            case TokenType::End:
            case TokenType::Eof:
                return;
            default:
                break;
        }
        advance();
    }
}

}  // namespace lunara

