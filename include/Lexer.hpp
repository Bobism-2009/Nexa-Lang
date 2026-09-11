#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <limits>
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
    New,
    Delete,
    Sizeof,
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
    ColonColon,
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
    Question,
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
    // filePath is only used to prefix diagnostics; passing it is optional so that
    // ad-hoc lexing (tests, tooling) still works without a file on disk.
    explicit Lexer(const std::string& source, const std::string& filePath = "")
        : source_(source), filePath_(filePath), pos_(0), line_(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < source_.size()) {
            skipWhitespace();
            if (pos_ >= source_.size()) break;

            char c = source_[pos_];
            // '-' only starts a negative literal in prefix position. After a token that can
            // end an expression it is the binary operator, so `x-1` is x - 1, not x (-1).
            const bool minusIsSign = !endsExpression(tokens);

            if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                while (pos_ < source_.size() && source_[pos_] != '\n') pos_++;
            } else if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
                // An unterminated /* used to swallow the rest of the file silently, so a
                // stray comment opener could delete arbitrary code with no diagnostic.
                size_t commentLine = line_;
                pos_ += 2;
                bool closed = false;
                while (pos_ + 1 < source_.size()) {
                    if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
                        pos_ += 2;
                        closed = true;
                        break;
                    }
                    if (source_[pos_] == '\n') line_++;
                    pos_++;
                }
                if (!closed) fail("Unterminated block comment (opened with '/*')", commentLine);
            } else if (c == '#') {
                tokens.push_back(scanInclude());
            } else if (c == 'R' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '"') {
                tokens.push_back(scanRawString());
            } else if (c == '"') {
                tokens.push_back(scanString());
            } else if (c == '\'') {
                tokens.push_back(scanChar());
            } else if (isDigit(c) || (minusIsSign && c == '-' && pos_ + 1 < source_.size() && isDigit(source_[pos_ + 1]))) {
                tokens.push_back(scanNumber());
            } else if (isAlpha(c) || c == '_') {
                Token id = scanIdentifier();
                if (id.type == TokenType::InlineCpp) {
                    size_t lineStart = id.line;
                    skipWhitespace();
                    if (pos_ >= source_.size() || source_[pos_] != '!') {
                        fail("Expected '!' after inline_cpp", lineStart);
                    }
                    pos_++;
                    skipWhitespace();
                    if (pos_ >= source_.size() || source_[pos_] != '{') {
                        fail("Expected '{' after inline_cpp!", lineStart);
                    }
                    pos_++;
                    std::string body = scanInlineCppBody(lineStart);
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
            } else if (c == '.' && pos_ + 1 < source_.size() && isDigit(source_[pos_ + 1])) {
                tokens.push_back(scanFloatFromDot());
            } else if (c == '.') {
                tokens.push_back({TokenType::Dot, ".", line_});
                pos_++;
            } else if (c == ',') {
                tokens.push_back({TokenType::Comma, ",", line_});
                pos_++;
            } else if (c == '?') {
                tokens.push_back({TokenType::Question, "?", line_});
                pos_++;
            } else if (c == ':' && pos_ + 1 < source_.size() && source_[pos_ + 1] == ':') {
                tokens.push_back({TokenType::ColonColon, "::", line_});
                pos_ += 2;
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
    std::string filePath_;
    size_t pos_;
    size_t line_;

    // Every lexer diagnostic goes through here so they all carry the same
    // "<file>: <what> at line N" shape that the parser's errors already use.
    [[noreturn]] void fail(const std::string& what, size_t line) const {
        std::string msg;
        if (!filePath_.empty()) msg += filePath_ + ": ";
        msg += what + " at line " + std::to_string(line);
        throw std::runtime_error(msg);
    }

    // Renders a byte the way it appeared in source, so the diagnostic for an
    // unknown escape can quote it without emitting a raw control character.
    static std::string describeEscape(char c) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u >= 32 && u < 127) return std::string("\\") + c;
        char buf[16];
        snprintf(buf, sizeof(buf), "\\x%02X", u);
        return buf;
    }

    static int hexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // \xHH is documented as exactly two hex digits (SYNTAX/Core.txt). Anything
    // shorter used to silently produce a different byte, or none at all.
    char scanHexEscape(size_t startLine, const char* inWhat) {
        int value = 0;
        for (int i = 0; i < 2; i++) {
            int d = (pos_ < source_.size()) ? hexValue(source_[pos_]) : -1;
            if (d < 0) {
                fail(std::string("Escape '\\x' in ") + inWhat +
                     " needs exactly two hex digits (e.g. \\x41)", startLine);
            }
            value = value * 16 + d;
            pos_++;
        }
        return static_cast<char>(value);
    }

    // Shared by string and character literals so both accept the same set.
    // Returns false when `c` is not a recognized escape letter.
    bool decodeSimpleEscape(char c, char& out) const {
        switch (c) {
            case 'n':  out = '\n'; return true;
            case 't':  out = '\t'; return true;
            case 'r':  out = '\r'; return true;
            case 'a':  out = '\a'; return true;
            case 'b':  out = '\b'; return true;
            case 'f':  out = '\f'; return true;
            case 'v':  out = '\v'; return true;
            case '0':  out = '\0'; return true;
            case '\\': out = '\\'; return true;
            case '"':  out = '"';  return true;
            case '\'': out = '\''; return true;
            default:   return false;
        }
    }

    // <cctype> is undefined for negative values, and `char` is signed on x86, so every
    // byte >= 0x80 (UTF-8 source, \xHH bytes) would be UB. Classify through unsigned char.
    static bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
    static bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
    static bool isAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

    // True when the last token can terminate an expression, which makes a following
    // '-' the binary subtraction operator rather than the sign of a numeric literal.
    static bool endsExpression(const std::vector<Token>& tokens) {
        if (tokens.empty()) return false;
        switch (tokens.back().type) {
            case TokenType::Identifier:
            case TokenType::Number:
            case TokenType::Float:
            case TokenType::String:
            case TokenType::Char:
            case TokenType::True:
            case TokenType::False:
            case TokenType::RParen:
            case TokenType::RBracket:
            case TokenType::PlusPlus:
            case TokenType::MinusMinus:
                return true;
            default:
                return false;
        }
    }

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
            // A raw newline never continues a string: a missing closing quote used to
            // swallow following lines and surface as a bogus parse error far away.
            // Multi-line text is spelled R"(...)" instead.
            if (source_[pos_] == '\n') {
                fail("Unterminated string literal", startLine);
            }
            if (source_[pos_] == '\\') {
                pos_++;
                if (pos_ >= source_.size()) fail("Unterminated string literal", startLine);
                char c = source_[pos_++];
                char decoded;
                if (decodeSimpleEscape(c, decoded)) {
                    value += decoded;
                } else if (c == 'x' || c == 'X') {
                    value += scanHexEscape(startLine, "string literal");
                } else {
                    // Previously the backslash was dropped and the letter kept, so
                    // "\q" compiled to "q" with no diagnostic at all.
                    fail("Unknown escape sequence '" + describeEscape(c) + "' in string literal",
                         startLine);
                }
            } else {
                value += source_[pos_++];
            }
        }
        if (pos_ >= source_.size()) fail("Unterminated string literal", startLine);
        pos_++;  // skip closing "
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
            fail("Malformed raw string literal (expected R\"(...)\" or R\"delim(...)delim\")",
                 startLine);
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
        fail("Unterminated raw string literal (expected closing " + closing + ")", startLine);
    }

    Token scanIdentifier() {
        size_t start = pos_;
        size_t startLine = line_;
        while (pos_ < source_.size() && (isAlnum(source_[pos_]) || source_[pos_] == '_')) {
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
        else if (value == "new") type = TokenType::New;
        else if (value == "delete") type = TokenType::Delete;
        else if (value == "sizeof") type = TokenType::Sizeof;
        else if (value == "inline_cpp") type = TokenType::InlineCpp;
        // Word spellings of the logical operators (SYNTAX/ControlFlow.txt).
        else if (value == "and") type = TokenType::And;
        else if (value == "or") type = TokenType::Or;
        else if (value == "not") type = TokenType::Not;

        return {type, value, startLine};
    }

    // Body inside inline_cpp! { ... }; brace depth only (strings/comments with } may need workarounds).
    std::string scanInlineCppBody(size_t startLine) {
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
            fail("Unclosed inline_cpp! { ... } block", startLine);
        }
        return out;
    }

    // The widest integers Nexa exposes are 64-bit (`long` / `unsigned long` / `size_t`),
    // so a literal must fit u64, or i64 when it carries a leading '-'. Out-of-range
    // literals used to be copied verbatim into the generated C++, where the user got a
    // raw clang "integer literal is too large" error pointing at machine-written code.
    void checkIntegerRange(const std::string& text, size_t line) const {
        bool negative = !text.empty() && text[0] == '-';
        size_t i = negative ? 1 : 0;
        int base = 10;
        if (text.size() >= i + 2 && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            base = 16;
            i += 2;
            if (i >= text.size()) {
                fail("Hexadecimal literal '" + text + "' has no digits", line);
            }
        } else if (text.size() >= i + 2 && text[i] == '0') {
            // The literal is copied through to C++, where a leading 0 means octal.
            // Read it the same way so the range check matches the emitted value and
            // so an out-of-octal-range digit is a Nexa error instead of a clang one.
            base = 8;
            i++;
        }

        const unsigned long long uMax = std::numeric_limits<unsigned long long>::max();
        // Magnitude of the most negative i64; `-9223372036854775808` is in range.
        const unsigned long long negMax = 9223372036854775808ULL;
        const unsigned long long limit = negative ? negMax : uMax;

        unsigned long long value = 0;
        bool overflow = false;
        for (; i < text.size() && !overflow; i++) {
            int d = hexValue(text[i]);
            if (d < 0) break;  // scanNumber only ever produces digits for the base it scanned
            if (d >= base) {
                if (base == 8) {
                    fail("Invalid digit '" + std::string(1, text[i]) + "' in octal literal '" +
                             text + "' (a leading 0 makes a literal octal, as in C)",
                         line);
                }
                break;
            }
            if (value > (uMax - static_cast<unsigned long long>(d)) / base) {
                overflow = true;  // past u64 entirely, so past every limit we check
                break;
            }
            value = value * base + static_cast<unsigned long long>(d);
        }
        if (overflow || value > limit) {
            fail("Integer literal '" + text + "' is out of range for a 64-bit integer (" +
                     (negative ? std::string("min -9223372036854775808")
                               : "max " + std::to_string(uMax)) + ")",
                 line);
        }
    }

    Token scanNumber() {
        size_t start = pos_;
        size_t startLine = line_;
        if (source_[pos_] == '-') pos_++;
        if (pos_ < source_.size() && source_[pos_] == '0' && pos_ + 1 < source_.size() &&
            (source_[pos_ + 1] == 'x' || source_[pos_ + 1] == 'X')) {
            pos_ += 2;
            while (pos_ < source_.size() && std::isxdigit(static_cast<unsigned char>(source_[pos_]))) pos_++;
            std::string text = source_.substr(start, pos_ - start);
            checkIntegerRange(text, startLine);
            return {TokenType::Number, text, startLine};
        }
        while (pos_ < source_.size() && isDigit(source_[pos_])) pos_++;
        if (pos_ < source_.size() && source_[pos_] == '.' && pos_ + 1 < source_.size() && isDigit(source_[pos_ + 1])) {
            pos_++;
            while (pos_ < source_.size() && isDigit(source_[pos_])) pos_++;
            return {TokenType::Float, source_.substr(start, pos_ - start), startLine};
        }
        std::string text = source_.substr(start, pos_ - start);
        checkIntegerRange(text, startLine);
        return {TokenType::Number, text, startLine};
    }

    Token scanChar() {
        size_t startLine = line_;
        pos_++;
        if (pos_ >= source_.size()) fail("Unterminated character literal", startLine);
        std::string value;
        if (source_[pos_] == '\\') {
            pos_++;
            if (pos_ >= source_.size()) fail("Unterminated character literal", startLine);
            char c = source_[pos_++];
            char decoded;
            if (decodeSimpleEscape(c, decoded)) {
                value = std::string(1, decoded);
            } else if (c == 'x' || c == 'X') {
                value = std::string(1, scanHexEscape(startLine, "character literal"));
            } else {
                fail("Unknown escape sequence '" + describeEscape(c) + "' in character literal",
                     startLine);
            }
        } else {
            if (source_[pos_] == '\n') fail("Unterminated character literal", startLine);
            value = std::string(1, source_[pos_++]);
        }
        if (pos_ < source_.size() && source_[pos_] == '\'') {
            pos_++;
        } else {
            fail("Unterminated character literal", startLine);
        }
        return {TokenType::Char, value, startLine};
    }

    Token scanFloatFromDot() {
        size_t start = pos_;
        size_t startLine = line_;
        pos_++;
        while (pos_ < source_.size() && isDigit(source_[pos_])) pos_++;
        return {TokenType::Float, "0" + source_.substr(start, pos_ - start), startLine};
    }
};

}  // namespace nexa
