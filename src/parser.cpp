#include "pylua/parser.hpp"

#include <sstream>
#include <stdexcept>

namespace pylua {

namespace {

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

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

ast::Program Parser::parse() {
    ast::Program program;
    while (!is_at_end()) {
        program.statements.push_back(declaration());
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
    consume(TokenType::Equal, "expected '=' after variable name");
    return std::make_unique<ast::VarStmt>(name.lexeme, expression(), is_const);
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

    return std::make_unique<ast::ImportStmt>(module_name, binding_name);
}

ast::StmtPtr Parser::function_declaration() {
    const Token& name = consume(TokenType::Identifier, "expected function name");
    consume(TokenType::LeftParen, "expected '(' after function name");

    std::vector<std::string> params;
    if (!check(TokenType::RightParen)) {
        do {
            params.push_back(consume(TokenType::Identifier, "expected parameter name").lexeme);
        } while (match({TokenType::Comma}));
    }

    consume(TokenType::RightParen, "expected ')' after function parameters");
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after function body");
    return std::make_unique<ast::FunctionStmt>(name.lexeme, std::move(params), std::move(body));
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
    if (match({TokenType::Return})) {
        return return_statement();
    }
    return expression_statement();
}

ast::StmtPtr Parser::for_statement() {
    const Token& name = consume(TokenType::Identifier, "expected loop variable name");
    consume(TokenType::In, "expected 'in' after loop variable");
    auto iterable = expression();
    consume(TokenType::Do, "expected 'do' after for iterable");
    auto body = block_until({TokenType::End});
    consume(TokenType::End, "expected 'end' after for loop");
    return std::make_unique<ast::ForInStmt>(name.lexeme, std::move(iterable), std::move(body));
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

ast::StmtPtr Parser::return_statement() {
    if (check(TokenType::End) || check(TokenType::Else) || check(TokenType::ElseIf) || is_at_end()) {
        return std::make_unique<ast::ReturnStmt>(nullptr);
    }
    return std::make_unique<ast::ReturnStmt>(expression());
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
    while (match({TokenType::Star, TokenType::Slash})) {
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

ast::ExprPtr Parser::primary() {
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
    throw std::runtime_error(out.str());
}

bool Parser::is_block_terminator(const std::vector<TokenType>& terminators) const {
    for (const TokenType type : terminators) {
        if (check(type)) {
            return true;
        }
    }
    return false;
}

}  // namespace pylua
