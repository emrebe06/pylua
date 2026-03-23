#include "pylua/lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace pylua {

namespace {

const std::unordered_map<std::string, TokenType> kKeywords = {
    {"let", TokenType::Let},
    {"const", TokenType::Const},
    {"import", TokenType::Import},
    {"func", TokenType::Func},
    {"for", TokenType::For},
    {"in", TokenType::In},
    {"if", TokenType::If},
    {"then", TokenType::Then},
    {"elseif", TokenType::ElseIf},
    {"else", TokenType::Else},
    {"end", TokenType::End},
    {"while", TokenType::While},
    {"do", TokenType::Do},
    {"return", TokenType::Return},
    {"true", TokenType::True},
    {"false", TokenType::False},
    {"nil", TokenType::Nil},
    {"and", TokenType::And},
    {"or", TokenType::Or},
    {"not", TokenType::Not},
};

}  // namespace

std::string token_type_name(TokenType type) {
    switch (type) {
        case TokenType::LeftParen:
            return "LeftParen";
        case TokenType::RightParen:
            return "RightParen";
        case TokenType::LeftBracket:
            return "LeftBracket";
        case TokenType::RightBracket:
            return "RightBracket";
        case TokenType::LeftBrace:
            return "LeftBrace";
        case TokenType::RightBrace:
            return "RightBrace";
        case TokenType::Comma:
            return "Comma";
        case TokenType::Colon:
            return "Colon";
        case TokenType::Dot:
            return "Dot";
        case TokenType::Minus:
            return "Minus";
        case TokenType::Plus:
            return "Plus";
        case TokenType::Slash:
            return "Slash";
        case TokenType::Star:
            return "Star";
        case TokenType::Equal:
            return "Equal";
        case TokenType::EqualEqual:
            return "EqualEqual";
        case TokenType::BangEqual:
            return "BangEqual";
        case TokenType::Greater:
            return "Greater";
        case TokenType::GreaterEqual:
            return "GreaterEqual";
        case TokenType::Less:
            return "Less";
        case TokenType::LessEqual:
            return "LessEqual";
        case TokenType::Identifier:
            return "Identifier";
        case TokenType::String:
            return "String";
        case TokenType::Number:
            return "Number";
        case TokenType::Let:
            return "Let";
        case TokenType::Const:
            return "Const";
        case TokenType::Import:
            return "Import";
        case TokenType::Func:
            return "Func";
        case TokenType::For:
            return "For";
        case TokenType::In:
            return "In";
        case TokenType::If:
            return "If";
        case TokenType::Then:
            return "Then";
        case TokenType::ElseIf:
            return "ElseIf";
        case TokenType::Else:
            return "Else";
        case TokenType::End:
            return "End";
        case TokenType::While:
            return "While";
        case TokenType::Do:
            return "Do";
        case TokenType::Return:
            return "Return";
        case TokenType::True:
            return "True";
        case TokenType::False:
            return "False";
        case TokenType::Nil:
            return "Nil";
        case TokenType::And:
            return "And";
        case TokenType::Or:
            return "Or";
        case TokenType::Not:
            return "Not";
        case TokenType::Eof:
            return "Eof";
    }

    return "Unknown";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::scan_tokens() {
    while (!is_at_end()) {
        skip_whitespace();
        if (is_at_end()) {
            break;
        }

        start_ = current_;
        token_line_ = line_;
        token_column_ = column_;
        scan_token();
    }

    tokens_.push_back(Token{TokenType::Eof, "", line_, column_});
    return tokens_;
}

bool Lexer::is_at_end() const {
    return current_ >= source_.size();
}

char Lexer::advance() {
    const char ch = source_[current_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

char Lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) {
        return false;
    }

    advance();
    return true;
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        const char ch = peek();

        if (ch == ' ' || ch == '\r' || ch == '\t' || ch == '\n') {
            advance();
            continue;
        }

        if (ch == '#') {
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

void Lexer::scan_token() {
    const char ch = advance();
    switch (ch) {
        case '(':
            add_token(TokenType::LeftParen);
            break;
        case ')':
            add_token(TokenType::RightParen);
            break;
        case '[':
            add_token(TokenType::LeftBracket);
            break;
        case ']':
            add_token(TokenType::RightBracket);
            break;
        case '{':
            add_token(TokenType::LeftBrace);
            break;
        case '}':
            add_token(TokenType::RightBrace);
            break;
        case ',':
            add_token(TokenType::Comma);
            break;
        case ':':
            add_token(TokenType::Colon);
            break;
        case '.':
            add_token(TokenType::Dot);
            break;
        case '-':
            add_token(TokenType::Minus);
            break;
        case '+':
            add_token(TokenType::Plus);
            break;
        case '*':
            add_token(TokenType::Star);
            break;
        case '/':
            add_token(TokenType::Slash);
            break;
        case '=':
            add_token(match('=') ? TokenType::EqualEqual : TokenType::Equal);
            break;
        case '!':
            if (match('=')) {
                add_token(TokenType::BangEqual);
                break;
            }
            error("unexpected '!'. Did you mean '!=' or 'not'?");
        case '>':
            add_token(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
            break;
        case '<':
            add_token(match('=') ? TokenType::LessEqual : TokenType::Less);
            break;
        case '"':
            string();
            break;
        default:
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                number();
                break;
            }
            if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
                identifier();
                break;
            }
            error(std::string("unexpected character '") + ch + "'");
    }
}

void Lexer::string() {
    while (!is_at_end()) {
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) {
                advance();
            }
            continue;
        }
        if (peek() == '"') {
            break;
        }
        advance();
    }

    if (is_at_end()) {
        error("unterminated string");
    }

    advance();
    add_token(TokenType::String);
}

void Lexer::number() {
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    add_token(TokenType::Number);
}

void Lexer::identifier() {
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        advance();
    }

    const std::string text = source_.substr(start_, current_ - start_);
    const auto keyword = kKeywords.find(text);
    add_token(keyword != kKeywords.end() ? keyword->second : TokenType::Identifier);
}

void Lexer::add_token(TokenType type) {
    tokens_.push_back(Token{
        type,
        source_.substr(start_, current_ - start_),
        token_line_,
        token_column_,
    });
}

[[noreturn]] void Lexer::error(const std::string& message) const {
    throw std::runtime_error(
        "Lexer error at line " + std::to_string(token_line_) + ", column " + std::to_string(token_column_) + ": " + message);
}

}  // namespace pylua
