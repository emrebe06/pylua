#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "pylua/ast.hpp"
#include "pylua/token.hpp"

namespace pylua {

class Parser {
  public:
    explicit Parser(std::vector<Token> tokens);

    ast::Program parse();

  private:
    ast::StmtPtr declaration();
    ast::StmtPtr variable_declaration(bool is_const);
    ast::StmtPtr import_statement();
    ast::StmtPtr function_declaration();
    ast::StmtPtr statement();
    ast::StmtPtr for_statement();
    ast::StmtPtr if_statement();
    ast::StmtPtr while_statement();
    ast::StmtPtr return_statement();
    ast::StmtPtr expression_statement();
    std::vector<ast::StmtPtr> block_until(const std::vector<TokenType>& terminators);

    ast::ExprPtr expression();
    ast::ExprPtr assignment();
    ast::ExprPtr logical_or();
    ast::ExprPtr logical_and();
    ast::ExprPtr equality();
    ast::ExprPtr comparison();
    ast::ExprPtr term();
    ast::ExprPtr factor();
    ast::ExprPtr unary();
    ast::ExprPtr call();
    ast::ExprPtr primary();

    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(std::initializer_list<TokenType> types);
    const Token& consume(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& token, const std::string& message) const;
    bool is_block_terminator(const std::vector<TokenType>& terminators) const;

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};

}  // namespace pylua
