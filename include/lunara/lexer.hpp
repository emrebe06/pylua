#pragma once

#include <string>
#include <vector>

#include "lunara/token.hpp"

namespace lunara {

class Lexer {
  public:
    explicit Lexer(std::string source);

    std::vector<Token> scan_tokens();

  private:
    bool is_at_end() const;
    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);
    void skip_whitespace();
    void scan_token();
    void string();
    void number();
    void identifier();
    void add_token(TokenType type);
    [[noreturn]] void error(const std::string& message) const;

    std::string source_;
    std::vector<Token> tokens_;
    std::size_t start_ = 0;
    std::size_t current_ = 0;
    int line_ = 1;
    int column_ = 1;
    int token_line_ = 1;
    int token_column_ = 1;
};

}  // namespace lunara

