#pragma once

#include <string>

namespace pylua {

enum class TokenType {
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Comma,
    Colon,
    Dot,
    Minus,
    Plus,
    Slash,
    Star,
    Equal,
    EqualEqual,
    BangEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Identifier,
    String,
    Number,
    Let,
    Const,
    Import,
    Func,
    For,
    In,
    If,
    Then,
    ElseIf,
    Else,
    End,
    While,
    Do,
    Return,
    True,
    False,
    Nil,
    And,
    Or,
    Not,
    Eof
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

std::string token_type_name(TokenType type);

}  // namespace pylua
