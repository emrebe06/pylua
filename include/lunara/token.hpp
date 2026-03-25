#pragma once

#include <string>

namespace lunara {

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
    Percent,
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
    As,
    Func,
    For,
    In,
    Match,
    When,
    If,
    Then,
    ElseIf,
    Else,
    End,
    While,
    Do,
    Return,
    Defer,
    Try,
    Catch,
    Finally,
    Throw,
    Break,
    Continue,
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

}  // namespace lunara

