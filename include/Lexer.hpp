#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>

namespace nexa {

// Token types for Nexa source
enum class TokenType {
    Include,
    String,
    Number,
    Float,
    Char,
    Identifier,
    Let,
    Const,
    If,
    Else,
    While,
    For,
    Return,
    Break,
    Continue,
    Goto,
    Try,
    Catch,
    Throw,
    Switch,
    Struct,
    Enum,
    Case,
    Default,
    True,
    False,
    Fn,
    Extern,
    Main,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Semicolon,
    Dot,
    Arrow,
    Ellipsis,
    Comma,
    Colon,
    Assign,
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    PercentAssign,
    BitAndAssign,
    BitOrAssign,
    BitXorAssign,
    ShlAssign,
    ShrAssign,
    Equals,
    NotEquals,
    And,
    Or,
    Not,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    Plus,
    Minus,
    PlusPlus,
    MinusMinus,
    Star,
    Slash,
    Percent,
    Shl,
    Shr,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    InlineCpp,
    InlineCppBlock,
    Eof
};

struct Token {
    TokenType type;
    std::string value;
    size_t line;
};

class Lexer {
public:
    explicit Lexer(const std::string& source)
        : source_(source), pos_(0), line_(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < source_.size()) {
            skipWhitespace();
            if (pos_ >= source_.size()) break;

            char c = source_[pos_];

            if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                while (pos_ < source_.size() && source_[pos_] != '\n') pos_++;
            } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
                pos_ += 2;
                while (pos_ + 1 < source_.size() && !(source_[pos_] == '*' && source_[pos_ + 1] == '/')) {
                    if (source_[pos_] == '\n') line_++;
                    pos_++;
                }
                if (pos_ + 1 < source_.size()) pos_ += 2;
            } else if (c == '#') {
                tokens.push_back(scanInclude());
            } else if (c == 'R' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '"') {
                tokens.push_back(scanRawString());
            } else if (c == '"') {
                tokens.push_back(scanString());
            } else if (c == '\'') {
                tokens.push_back(scanChar());
            } else if (std::isdigit(c) || (c == '-' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1]))) {
                tokens.push_back(scanNumber());
            } else if (std::isalpha(c) || c == '_') {
                Token id = scanIdentifier();
                if (id.type == TokenType::InlineCpp) {
                    size_t lineStart = id.line;
                    skipWhitespace();
                    if (pos_ >= source_.size() || source_[pos_] != '!') {
                        throw std::runtime_error("Expected '!' after inline_cpp at line " + std::to_string(lineStart));
                    }
                    pos_++;
                    skipWhitespace();
                    if (pos_ >= source_.size() || source_[pos_] != '{') {
                        throw std::runtime_error("Expected '{' after inline_cpp! at line " + std::to_string(lineStart));
                    }
                    pos_++;
                    std::string body = scanInlineCppBody();
                    tokens.push_back({TokenType::InlineCppBlock, body, lineStart});
                } else {
                    tokens.push_back(id);
                }
            } else if (c == '(') {
                tokens.push_back({TokenType::LParen, "(", line_});
                pos_++;
            } else if (c == ')') {
                tokens.push_back({TokenType::RParen, ")", line_});
                pos_++;
            } else if (c == '{') {
                tokens.push_back({TokenType::LBrace, "{", line_});
                pos_++;
            } else if (c == '}') {
                tokens.push_back({TokenType::RBrace, "}", line_});
                pos_++;
            } else if (c == '[') {
                tokens.push_back({TokenType::LBracket, "[", line_});
                pos_++;
            } else if (c == ']') {
                tokens.push_back({TokenType::RBracket, "]", line_});
                pos_++;
            } else if (c == ';') {
                tokens.push_back({TokenType::Semicolon, ";", line_});
                pos_++;
            } else if (c == '.' && pos_ + 2 < source_.size() && source_[pos_ + 1] == '.' && source_[pos_ + 2] == '.') {
                tokens.push_back({TokenType::Ellipsis, "...", line_});
                pos_ += 3;
            } else if (c == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
                tokens.push_back(scanFloatFromDot());
            } else if (c == '.') {
                tokens.push_back({TokenType::Dot, ".", line_});
                pos_++;
            } else if (c == ',') {
                tokens.push_back({TokenType::Comma, ",", line_});
                pos_++;
            } else if (c == ':') {
                tokens.push_back({TokenType::Colon, ":", line_});
                pos_++;
            } else if (c == '=' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::Equals, "==", line_});
                pos_ += 2;
            } else if (c == '=') {
                tokens.push_back({TokenType::Assign, "=", line_});
                pos_++;
            } else if (c == '!' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::NotEquals, "!=", line_});
                pos_ += 2;
            } else if (c == '!') {
                tokens.push_back({TokenType::Not, "!", line_});
                pos_++;
            } else if (c == '&' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '&') {
                tokens.push_back({TokenType::And, "&&", line_});
                pos_ += 2;
            } else if (c == '&' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::BitAndAssign, "&=", line_});
                pos_ += 2;
            } else if (c == '&') {
                tokens.push_back({TokenType::BitAnd, "&", line_});
                pos_++;
            } else if (c == '|' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '|') {
                tokens.push_back({TokenType::Or, "||", line_});
                pos_ += 2;
            } else if (c == '|' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::BitOrAssign, "|=", line_});
                pos_ += 2;
            } else if (c == '|') {
                tokens.push_back({TokenType::BitOr, "|", line_});
                pos_++;
            } else if (c == '^' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::BitXorAssign, "^=", line_});
                pos_ += 2;
            } else if (c == '^') {
                tokens.push_back({TokenType::BitXor, "^", line_});
                pos_++;
            } else if (c == '~') {
                tokens.push_back({TokenType::BitNot, "~", line_});
                pos_++;
            } else if (c == '<' && pos_ + 2 < source_.size() && source_[pos_ + 1] == '<' && source_[pos_ + 2] == '=') {
                tokens.push_back({TokenType::ShlAssign, "<<=", line_});
                pos_ += 3;
            } else if (c == '<' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '<') {
                tokens.push_back({TokenType::Shl, "<<", line_});
                pos_ += 2;
            } else if (c == '<' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::LessEq, "<=", line_});
                pos_ += 2;
            } else if (c == '<') {
                tokens.push_back({TokenType::Less, "<", line_});
                pos_++;
            } else if (c == '>' && pos_ + 2 < source_.size() && source_[pos_ + 1] == '>' && source_[pos_ + 2] == '=') {
                tokens.push_back({TokenType::ShrAssign, ">>=", line_});
                pos_ += 3;
            } else if (c == '>' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '>') {
                tokens.push_back({TokenType::Shr, ">>", line_});
                pos_ += 2;
            } else if (c == '>' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::GreaterEq, ">=", line_});
                pos_ += 2;
            } else if (c == '>') {
                tokens.push_back({TokenType::Greater, ">", line_});
                pos_++;
            } else if (c == '+' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '+') {
                tokens.push_back({TokenType::PlusPlus, "++", line_});
                pos_ += 2;
            } else if (c == '+' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::PlusAssign, "+=", line_});
                pos_ += 2;
            } else if (c == '+') {
                tokens.push_back({TokenType::Plus, "+", line_});
                pos_++;
            } else if (c == '-' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '>') {
                tokens.push_back({TokenType::Arrow, "->", line_});
                pos_ += 2;
            } else if (c == '-' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '-') {
                tokens.push_back({TokenType::MinusMinus, "--", line_});
                pos_ += 2;
            } else if (c == '-' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::MinusAssign, "-=", line_});
                pos_ += 2;
            } else if (c == '-') {
                tokens.push_back({TokenType::Minus, "-", line_});
                pos_++;
            } else if (c == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::StarAssign, "*=", line_});
                pos_ += 2;
            } else if (c == '*') {
                tokens.push_back({TokenType::Star, "*", line_});
                pos_++;
            } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::SlashAssign, "/=", line_});
                pos_ += 2;
            } else if (c == '/') {
                tokens.push_back({TokenType::Slash, "/", line_});
                pos_++;
            } else if (c == '%' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
                tokens.push_back({TokenType::PercentAssign, "%=", line_});
                pos_ += 2;
            } else if (c == '%') {
                tokens.push_back({TokenType::Percent, "%", line_});
                pos_++;
            } else {
                pos_++;  // Skip unknown chars
            }
        }
        tokens.push_back({TokenType::Eof, "", line_});
        return tokens;
    }

private:
    std::string source_;
    size_t pos_;
    size_t line_;

    void skipWhitespace() {
        while (pos_ < source_.size()) {
            char c = source_[pos_];
            if (c == ' ' || c == '\t') {
                pos_++;
            } else if (c == '\n') {
                pos_++;
                line_++;
            } else if (c == '\r') {
                pos_++;
            } else {
                break;
            }
        }
    }

    Token scanInclude() {
        size_t start = pos_;
        size_t startLine = line_;
        pos_++;  // skip #

        // #include <std/io>
        while (pos_ < source_.size() && source_[pos_] != '\n') {
            pos_++;
        }
        std::string value = source_.substr(start, pos_ - start);
        return {TokenType::Include, value, startLine};
    }

    Token scanString() {
        size_t startLine = line_;
        pos_++;  // skip opening "
        std::string value;
        while (pos_ < source_.size() && source_[pos_] != '"') {
            if (source_[pos_] == '\\') {
                pos_++;
                if (pos_ >= source_.size()) break;
                char c = source_[pos_++];
                if (c == 'n') value += '\n';
                else if (c == 't') value += '\t';
                else if (c == 'r') value += '\r';
                else if (c == '\\') value += '\\';
                else if (c == '"') value += '"';
                else if (c == 'x' || c == 'X') {
                    int hex = 0;
                    int digits = 0;
                    while (pos_ < source_.size() && digits < 2) {
                        char h = source_[pos_];
                        if (h >= '0' && h <= '9') { hex = hex * 16 + (h - '0'); pos_++; digits++; }
                        else if (h >= 'a' && h <= 'f') { hex = hex * 16 + (h - 'a' + 10); pos_++; digits++; }
                        else if (h >= 'A' && h <= 'F') { hex = hex * 16 + (h - 'A' + 10); pos_++; digits++; }
                        else break;
                    }
                    if (digits > 0) value += static_cast<char>(hex & 0xFF);
                } else {
                    value += c;  // unknown escape, pass through (e.g. \0)
                }
            } else {
                value += source_[pos_++];
            }
        }
        if (pos_ < source_.size()) pos_++;  // skip closing "
        return {TokenType::String, value, startLine};
    }

    Token scanRawString() {
        size_t startLine = line_;
        pos_ += 2;  // skip R"
        std::string delimiter;
        while (pos_ < source_.size()) {
            char c = source_[pos_];
            if (c == '(') { pos_++; break; }
            if (c == ')' || c == '\\' || c == '"' || c == '\n') break;
            delimiter += c;
            pos_++;
        }
        if (pos_ >= source_.size() || source_[pos_ - 1] != '(') {
            return {TokenType::String, "", startLine};  // malformed, return empty
        }
        std::string value;
        std::string closing = ")" + delimiter + "\"";
        size_t closeLen = closing.size();
        while (pos_ + closeLen <= source_.size()) {
            if (source_.substr(pos_, closeLen) == closing) {
                pos_ += closeLen;
                return {TokenType::String, value, startLine};
            }
            if (source_[pos_] == '\n') line_++;
            value += source_[pos_++];
        }
        return {TokenType::String, value, startLine};  // unclosed, return what we have
    }

    Token scanIdentifier() {
        size_t start = pos_;
        size_t startLine = line_;
        while (pos_ < source_.size() && (std::isalnum(source_[pos_]) || source_[pos_] == '_')) {
            pos_++;
        }
        std::string value = source_.substr(start, pos_ - start);

        TokenType type = TokenType::Identifier;
        if (value == "fn") type = TokenType::Fn;
        else if (value == "main") type = TokenType::Main;
        else if (value == "let") type = TokenType::Let;
        else if (value == "const") type = TokenType::Const;
        else if (value == "if") type = TokenType::If;
        else if (value == "else") type = TokenType::Else;
        else if (value == "while") type = TokenType::While;
        else if (value == "for") type = TokenType::For;
        else if (value == "return") type = TokenType::Return;
        else if (value == "break") type = TokenType::Break;
        else if (value == "continue") type = TokenType::Continue;
        else if (value == "goto") type = TokenType::Goto;
        else if (value == "try") type = TokenType::Try;
        else if (value == "catch") type = TokenType::Catch;
        else if (value == "throw") type = TokenType::Throw;
        else if (value == "switch") type = TokenType::Switch;
        else if (value == "struct") type = TokenType::Struct;
        else if (value == "enum") type = TokenType::Enum;
        else if (value == "case") type = TokenType::Case;
        else if (value == "default") type = TokenType::Default;
        else if (value == "true") type = TokenType::True;
        else if (value == "false") type = TokenType::False;
        else if (value == "extern") type = TokenType::Extern;
        else if (value == "inline_cpp") type = TokenType::InlineCpp;

        return {type, value, startLine};
    }

    // Body inside inline_cpp! { ... }; brace depth only (strings/comments with } may need workarounds).
    std::string scanInlineCppBody() {
        int depth = 1;
        std::string out;
        while (pos_ < source_.size() && depth > 0) {
            char c = source_[pos_];
            if (c == '{') {
                depth++;
                out += c;
                pos_++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    pos_++;
                    break;
                }
                out += c;
                pos_++;
            } else {
                if (c == '\n') line_++;
                out += c;
                pos_++;
            }
        }
        if (depth != 0) {
            throw std::runtime_error("Unclosed inline_cpp! { ... } block");
        }
        return out;
    }

    Token scanNumber() {
        size_t start = pos_;
        size_t startLine = line_;
        if (source_[pos_] == '-') pos_++;
        if (pos_ < source_.size() && source_[pos_] == '0' && pos_ + 1 < source_.size() &&
            (source_[pos_ + 1] == 'x' || source_[pos_ + 1] == 'X')) {
            pos_ += 2;
            while (pos_ < source_.size() && std::isxdigit(static_cast<unsigned char>(source_[pos_]))) pos_++;
            return {TokenType::Number, source_.substr(start, pos_ - start), startLine};
        }
        while (pos_ < source_.size() && std::isdigit(source_[pos_])) pos_++;
        if (pos_ < source_.size() && source_[pos_] == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
            pos_++;
            while (pos_ < source_.size() && std::isdigit(source_[pos_])) pos_++;
            return {TokenType::Float, source_.substr(start, pos_ - start), startLine};
        }
        return {TokenType::Number, source_.substr(start, pos_ - start), startLine};
    }

    Token scanChar() {
        size_t startLine = line_;
        pos_++;
        if (pos_ >= source_.size()) {
            return {TokenType::Char, "", startLine};
        }
        std::string value;
        if (source_[pos_] == '\\') {
            pos_++;
            if (pos_ >= source_.size()) return {TokenType::Char, "", startLine};
            char c = source_[pos_++];
            if (c == 'n') value = "\n";
            else if (c == 't') value = "\t";
            else if (c == 'r') value = "\r";
            else if (c == '\\') value = "\\";
            else if (c == '\'') value = "'";
            else if (c == 'x' || c == 'X') {
                int hex = 0;
                int digits = 0;
                while (pos_ < source_.size() && digits < 2) {
                    char h = source_[pos_];
                    if (h >= '0' && h <= '9') { hex = hex * 16 + (h - '0'); pos_++; digits++; }
                    else if (h >= 'a' && h <= 'f') { hex = hex * 16 + (h - 'a' + 10); pos_++; digits++; }
                    else if (h >= 'A' && h <= 'F') { hex = hex * 16 + (h - 'A' + 10); pos_++; digits++; }
                    else break;
                }
                value = std::string(1, static_cast<char>(hex & 0xFF));
            } else value = std::string(1, c);
        } else {
            value = std::string(1, source_[pos_++]);
        }
        if (pos_ < source_.size() && source_[pos_] == '\'') pos_++;
        return {TokenType::Char, value, startLine};
    }

    Token scanFloatFromDot() {
        size_t start = pos_;
        size_t startLine = line_;
        pos_++;
        while (pos_ < source_.size() && std::isdigit(source_[pos_])) pos_++;
        return {TokenType::Float, "0" + source_.substr(start, pos_ - start), startLine};
    }
};

}  // namespace nexa
