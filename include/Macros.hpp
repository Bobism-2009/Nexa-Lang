#pragma once

// #define and #undef, as in C: object-like (#define MAX 10) and function-like
// (#define SQ(x) ((x) * (x)), with ... and __VA_ARGS__) macros, replaced token by token
// before the parser sees the file. Expansion follows C's rules -- arguments are expanded
// before they are put in, the result is scanned again, and a macro never expands inside
// its own expansion (each token carries the names it came out of: Prosser's hide sets).
// Not here: # and ## in a body, and #if and its family.
//
// A macro is defined from its #define to its #undef or the end of the file, and a file
// that includes another gets what that file leaves defined, as a C header does.

#include "Lexer.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace nexa {

struct NexaMacro {
    bool fnLike = false;
    bool variadic = false;
    std::vector<std::string> params;
    std::vector<Token> body;
    size_t line = 0;
    std::string file;
};
using NexaMacroTable = std::map<std::string, NexaMacro>;

// One included .nxa: its tokens with macros expanded, and what it leaves defined. The
// includer needs the macros before it reads on; the include then parses these tokens,
// so a file is lexed once either way.
struct NexaMacroFile {
    std::vector<Token> tokens;
    NexaMacroTable macros;
    bool done = false;   // false while it is being read: an include cycle gets nothing
    bool taken = false;  // the tokens went to the include's parser
};
using NexaMacroCache = std::map<std::string, NexaMacroFile>;

class NexaMacroExpander {
public:
    using IncludeMacros = std::function<const NexaMacroTable*(const Token&)>;

    NexaMacroExpander(NexaMacroTable& table, const std::string& file) : table_(table), file_(file) {}

    std::vector<Token> run(const std::vector<Token>& in, const IncludeMacros& include) {
        std::deque<HTok> q;
        for (const Token& t : in) q.push_back({t, nullptr});
        std::vector<HTok> expanded;
        expanded.reserve(in.size());
        process(q, expanded, &include);
        std::vector<Token> out;
        out.reserve(expanded.size());
        for (HTok& h : expanded) {
            Token& t = h.t;
            // The lexer reads -1 as one literal where nothing ends an expression before it.
            // After an expansion something may: x NEG where NEG is -1 is x - 1.
            if ((t.type == TokenType::Number || t.type == TokenType::Float) && t.value.size() > 1 && t.value[0] == '-' &&
                !out.empty() && Lexer::endsExpressionToken(out.back().type)) {
                out.push_back({TokenType::Minus, "-", t.line});
                t.value.erase(0, 1);
            }
            out.push_back(std::move(t));
        }
        return out;
    }

    static bool anyDirective(const std::vector<Token>& toks) {
        for (const Token& t : toks) {
            if (t.type == TokenType::Define || t.type == TokenType::Undef) return true;
        }
        return false;
    }

private:
    using HideSet = std::shared_ptr<const std::set<std::string>>;
    struct HTok {
        Token t;
        HideSet hide;
    };

    NexaMacroTable& table_;
    std::string file_;

    [[noreturn]] void fail(const std::string& what, size_t line) const {
        std::string msg;
        if (!file_.empty()) msg += file_ + ": ";
        msg += what + " at line " + std::to_string(line);
        throw std::runtime_error(msg);
    }

    static bool hidden(const HTok& h, const std::string& name) { return h.hide && h.hide->count(name); }

    static HideSet with(const HideSet& a, const HideSet& b) {
        if (!b || b->empty()) return a;
        if (!a || a->empty()) return b;
        auto s = std::make_shared<std::set<std::string>>(*a);
        s->insert(b->begin(), b->end());
        return s;
    }

    static HideSet with(const HideSet& a, const std::string& name) {
        auto s = a ? std::make_shared<std::set<std::string>>(*a) : std::make_shared<std::set<std::string>>();
        s->insert(name);
        return s;
    }

    static bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    static bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

    static std::string readName(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i >= s.size() || !isIdentStart(s[i])) return "";
        const size_t b = i;
        while (i < s.size() && isIdentChar(s[i])) i++;
        return s.substr(b, i - b);
    }

    void checkName(const std::string& name, const std::string& directive, size_t line) const {
        if (name.empty()) fail(directive + " needs a name", line);
        std::vector<Token> t = Lexer(name).tokenize();
        if (t.empty() || t[0].type != TokenType::Identifier) {
            fail(directive + " " + name + ": '" + name + "' is a Nexa keyword and cannot be a macro", line);
        }
    }

    static bool sameMacro(const NexaMacro& a, const NexaMacro& b) {
        if (a.fnLike != b.fnLike || a.variadic != b.variadic || a.params != b.params || a.body.size() != b.body.size()) {
            return false;
        }
        for (size_t i = 0; i < a.body.size(); i++) {
            if (a.body[i].type != b.body[i].type || a.body[i].value != b.body[i].value) return false;
        }
        return true;
    }

    void add(const std::string& name, NexaMacro m, size_t line) {
        auto it = table_.find(name);
        if (it != table_.end() && !sameMacro(it->second, m)) {
            std::string where = "line " + std::to_string(it->second.line);
            if (it->second.file != m.file && !it->second.file.empty()) where += " of " + it->second.file;
            fail("#define " + name + ": already defined differently (" + where + "); #undef it first", line);
        }
        table_[name] = std::move(m);
    }

    void define(const Token& d) {
        const std::string& s = d.value;
        size_t i = 0;
        const std::string name = readName(s, i);
        checkName(name, "#define", d.line);
        NexaMacro m;
        m.line = d.line;
        m.file = file_;
        if (i < s.size() && s[i] == '(') {  // no space before '(': a function-like macro
            m.fnLike = true;
            i++;
            const std::string bad = "#define " + name + "(...): expected a parameter name, ',' or ')'";
            while (true) {
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                if (i < s.size() && s[i] == ')' && m.params.empty()) {
                    i++;
                    break;
                }
                if (s.compare(i, 3, "...") == 0) {
                    m.variadic = true;
                    i += 3;
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                    if (i >= s.size() || s[i] != ')') fail("#define " + name + ": '...' must be the last parameter", d.line);
                    i++;
                    break;
                }
                const std::string p = readName(s, i);
                if (p.empty()) fail(bad, d.line);
                for (const std::string& q : m.params) {
                    if (q == p) fail("#define " + name + ": parameter '" + p + "' is named twice", d.line);
                }
                m.params.push_back(p);
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                if (i < s.size() && s[i] == ',') {
                    i++;
                    continue;
                }
                if (i < s.size() && s[i] == ')') {
                    i++;
                    break;
                }
                fail(bad, d.line);
            }
        }
        m.body = Lexer(s.substr(i), file_, d.line).tokenize();
        while (!m.body.empty() && m.body.back().type == TokenType::Eof) m.body.pop_back();
        for (const Token& b : m.body) {
            if (b.type == TokenType::Include || b.type == TokenType::Define || b.type == TokenType::Undef) {
                fail("#define " + name + ": '#' and '##' are not supported in a macro body", d.line);
            }
        }
        add(name, std::move(m), d.line);
    }

    void undef(const Token& d) {
        size_t i = 0;
        const std::string name = readName(d.value, i);
        checkName(name, "#undef", d.line);
        while (i < d.value.size() && (d.value[i] == ' ' || d.value[i] == '\t')) i++;
        if (i < d.value.size() && d.value.compare(i, 2, "//") != 0) {
            fail("#undef " + name + ": expected the end of the line after the name", d.line);
        }
        table_.erase(name);
    }

    // Takes a function-like macro's arguments off the front of q, after the '(':
    // split at commas outside (), [] and {}, up to the matching ')'.
    std::vector<std::vector<HTok>> collectArgs(std::deque<HTok>& q, const std::string& name, size_t line) {
        std::vector<std::vector<HTok>> args(1);
        std::vector<TokenType> open;  // closers still expected inside the call
        while (true) {
            const bool end = q.empty() || q.front().t.type == TokenType::Eof || q.front().t.type == TokenType::Define ||
                             q.front().t.type == TokenType::Undef;
            const TokenType ty = end ? TokenType::Eof : q.front().t.type;
            const bool closer = ty == TokenType::RParen || ty == TokenType::RBracket || ty == TokenType::RBrace;
            if (end || (closer && (open.empty() ? ty != TokenType::RParen : ty != open.back()))) {
                fail("Unterminated call to macro " + name + " (no closing ')')", line);
            }
            HTok h = std::move(q.front());
            q.pop_front();
            if (ty == TokenType::LParen) open.push_back(TokenType::RParen);
            if (ty == TokenType::LBracket) open.push_back(TokenType::RBracket);
            if (ty == TokenType::LBrace) open.push_back(TokenType::RBrace);
            if (closer) {
                if (open.empty()) break;
                open.pop_back();
            }
            if (ty == TokenType::Comma && open.empty()) {
                args.emplace_back();
                continue;
            }
            args.back().push_back(std::move(h));
        }
        return args;
    }

    std::vector<HTok> expandAll(const std::vector<HTok>& toks) {
        std::deque<HTok> q(toks.begin(), toks.end());
        std::vector<HTok> out;
        process(q, out, nullptr);
        return out;
    }

    void process(std::deque<HTok>& q, std::vector<HTok>& out, const IncludeMacros* include) {
        while (!q.empty()) {
            HTok h = std::move(q.front());
            q.pop_front();
            const Token& t = h.t;
            if (t.type == TokenType::Define) {
                define(t);
                continue;
            }
            if (t.type == TokenType::Undef) {
                undef(t);
                continue;
            }
            if (t.type == TokenType::Include && include && *include) {
                if (const NexaMacroTable* m = (*include)(t)) {
                    for (const auto& kv : *m) add(kv.first, kv.second, t.line);
                }
                out.push_back(std::move(h));
                continue;
            }
            if (t.type != TokenType::Identifier) {
                out.push_back(std::move(h));
                continue;
            }
            auto it = table_.find(t.value);
            if (it == table_.end() || hidden(h, t.value)) {
                out.push_back(std::move(h));
                continue;
            }
            // Not a copy: nothing below changes the table (only a #define token does).
            const NexaMacro& m = it->second;
            const std::string name = t.value;
            const size_t line = t.line;
            if (!m.fnLike) {
                const HideSet hs = with(h.hide, name);
                for (size_t i = m.body.size(); i-- > 0;) {
                    Token b = m.body[i];
                    b.line = line;
                    q.push_front({std::move(b), hs});
                }
                continue;
            }
            // A function-like macro's name with no '(' after it is just a name.
            if (q.empty() || q.front().t.type != TokenType::LParen) {
                out.push_back(std::move(h));
                continue;
            }
            q.pop_front();
            std::vector<std::vector<HTok>> args = collectArgs(q, name, line);
            const size_t named = m.params.size();
            if (args.size() == 1 && args[0].empty() && named == 0) args.clear();
            if (args.size() < named || (!m.variadic && args.size() > named)) {
                fail("Macro " + name + " takes " + std::to_string(named) + (m.variadic ? " or more" : "") +
                         " argument" + (named == 1 && !m.variadic ? "" : "s") + ", given " + std::to_string(args.size()),
                     line);
            }
            std::vector<std::vector<HTok>> expandedArgs;
            for (size_t i = 0; i < named; i++) expandedArgs.push_back(expandAll(args[i]));
            std::vector<HTok> va;
            if (m.variadic) {
                std::vector<HTok> joined;
                for (size_t i = named; i < args.size(); i++) {
                    if (i > named) joined.push_back({{TokenType::Comma, ",", line}, nullptr});
                    for (const HTok& a : args[i]) joined.push_back(a);
                }
                va = expandAll(joined);
            }
            const HideSet hs = with(h.hide, name);
            // An argument's tokens mostly share one hide set: join each with hs once.
            std::vector<std::pair<const std::set<std::string>*, HideSet>> joinedHides;
            auto joinHide = [&](const HideSet& a) -> HideSet {
                for (const auto& j : joinedHides) {
                    if (j.first == a.get()) return j.second;
                }
                HideSet r = with(a, hs);
                joinedHides.emplace_back(a.get(), r);
                return r;
            };
            std::vector<HTok> result;
            result.reserve(m.body.size());
            for (const Token& b : m.body) {
                const std::vector<HTok>* arg = nullptr;
                if (b.type == TokenType::Identifier) {
                    for (size_t p = 0; p < named; p++) {
                        if (m.params[p] == b.value) arg = &expandedArgs[p];
                    }
                    if (m.variadic && b.value == "__VA_ARGS__") arg = &va;
                }
                if (arg) {
                    for (const HTok& a : *arg) {
                        HTok c = a;
                        c.t.line = line;
                        c.hide = joinHide(a.hide);
                        result.push_back(std::move(c));
                    }
                } else {
                    Token c = b;
                    c.line = line;
                    result.push_back({std::move(c), hs});
                }
            }
            for (size_t i = result.size(); i-- > 0;) q.push_front(std::move(result[i]));
        }
    }
};

}  // namespace nexa
