#pragma once

// C's preprocessor, on Nexa tokens, before the parser sees a file:
//   #define and #undef: object-like (#define MAX 10) and function-like (#define SQ(x)
//     ((x) * (x)), with ... , __VA_ARGS__ and __VA_OPT__) macros, with # (stringize) and
//     ## (paste) in a body;
//   #if, #ifdef, #ifndef, #elif, #elifdef, #elifndef, #else and #endif, with defined;
//   #error and #warning; #line N ["file"]; #pragma once and #pragma message("text"),
//     and _Pragma("...") for a pragma a macro can make;
//   __has_include(<std/io>) and __has_include("file.nxa") in an #if;
//   predefined macros: __LINE__, __FILE__, __COUNTER__, __DATE__, __TIME__, __NEXA__ and
//     the target's own,
//     as C compilers name them (_WIN32, __linux__, __APPLE__, __x86_64__, __aarch64__ ...).
// Expansion follows C's rules -- arguments are expanded before they are put in (except
// beside # and ##), the result is scanned again, and a macro never expands inside its
// own expansion (each token carries the names it came out of: Prosser's hide sets).
//
// A macro is defined from its #define to its #undef or the end of the file, and a file
// that includes another gets what that file leaves defined, as a C header does. Lines an
// #if leaves out are still read as Nexa tokens, so a string in them must be closed.

#include "Lexer.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace nexa {

struct NexaMacro {
    enum class Builtin { None, Line, File, Counter };
    bool fnLike = false;
    bool variadic = false;
    bool paste = false;  // the body has # or ## (or __VA_OPT__): the full substitution
    Builtin builtin = Builtin::None;
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

// What every file of one build shares.
struct NexaMacroCache {
    std::map<std::string, NexaMacroFile> files;
    NexaMacroTable predefined;  // nexaPredefinedMacros(the target's triple)
    long long counter = 0;      // __COUNTER__
};

// The macros a C compiler predefines for `triple` (as clang names targets: x86_64-w64-mingw32,
// aarch64-linux-gnu, wasm32-wasi ...), plus Nexa's own and the three that change as they go.
inline NexaMacroTable nexaPredefinedMacros(const std::string& triple) {
    NexaMacroTable t;
    auto one = [&t](const char* name) {
        NexaMacro m;
        m.body.push_back({TokenType::Number, "1", 0});
        t[name] = std::move(m);
    };
    auto builtin = [&t](const char* name, NexaMacro::Builtin b) {
        NexaMacro m;
        m.builtin = b;
        t[name] = std::move(m);
    };
    builtin("__LINE__", NexaMacro::Builtin::Line);
    builtin("__FILE__", NexaMacro::Builtin::File);
    builtin("__COUNTER__", NexaMacro::Builtin::Counter);
    // __DATE__ "Nov 25 2026" and __TIME__ "14:03:07": when the build started, as C's are.
    // SOURCE_DATE_EPOCH (seconds since 1970, UTC), when set, is the time instead, as GCC
    // and clang read it, so a build that names them can still be made the same twice.
    {
        std::time_t now = std::time(nullptr);
        bool utc = false;
        if (const char* sde = std::getenv("SOURCE_DATE_EPOCH")) {
            char* end = nullptr;
            const long long v = std::strtoll(sde, &end, 10);
            if (*sde && end && *end == '\0' && v >= 0) {
                now = static_cast<std::time_t>(v);
                utc = true;
            }
        }
        const std::tm* tm = utc ? std::gmtime(&now) : std::localtime(&now);
        static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        char date[32] = "??? ?? ????";
        char clock[32] = "??:??:??";
        if (tm) {
            std::snprintf(date, sizeof(date), "%s %2d %d", months[tm->tm_mon % 12], tm->tm_mday, tm->tm_year + 1900);
            std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
        }
        auto text = [&t](const char* name, const char* value) {
            NexaMacro m;
            m.body.push_back({TokenType::String, value, 0});
            t[name] = std::move(m);
        };
        text("__DATE__", date);
        text("__TIME__", clock);
    }
    one("__NEXA__");
    const std::string arch = triple.substr(0, triple.find('-'));
    auto has = [&triple](const char* s) { return triple.find(s) != std::string::npos; };
    bool bits64 = false;
    if (arch == "x86_64" || arch == "amd64") {
        one("__x86_64__");
        bits64 = true;
    } else if (arch == "aarch64" || arch == "arm64") {
        one("__aarch64__");
        bits64 = true;
    } else if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686") {
        one("__i386__");
    } else if (arch.compare(0, 3, "arm") == 0 || arch.compare(0, 5, "thumb") == 0) {
        one("__arm__");
    } else if (arch.compare(0, 4, "mips") == 0) {
        one("__mips__");
        bits64 = arch.compare(0, 6, "mips64") == 0;
    } else if (arch.compare(0, 5, "riscv") == 0) {
        one("__riscv");
        bits64 = arch == "riscv64";
    } else if (arch.compare(0, 4, "wasm") == 0) {
        one("__wasm__");
        one(arch == "wasm64" ? "__wasm64__" : "__wasm32__");
        bits64 = arch == "wasm64";
    }
    if (has("windows") || has("mingw") || has("win32")) {
        one("_WIN32");
        if (bits64) one("_WIN64");
    } else if (has("linux")) {
        one("__linux__");
        one("__unix__");
    } else if (has("darwin") || has("macos") || has("apple")) {
        one("__APPLE__");
        one("__MACH__");
    }
    if (has("wasi")) one("__wasi__");
    return t;
}

class NexaMacroExpander {
public:
    using IncludeMacros = std::function<const NexaMacroTable*(const Token&)>;
    // __has_include: whether the include written as given ("<std/io>", "\"a.nxa\"") is there.
    using HasInclude = std::function<bool(const std::string&)>;
    void setHasInclude(HasInclude f) { hasInclude_ = std::move(f); }

    NexaMacroExpander(NexaMacroTable& table, const std::string& file, NexaMacroCache* shared = nullptr)
        : table_(table), file_(file), shared_(shared) {}

    std::vector<Token> run(const std::vector<Token>& in, const IncludeMacros& include) {
        std::deque<HTok> q;
        for (const Token& t : in) q.push_back({t, nullptr});
        std::vector<HTok> expanded;
        expanded.reserve(in.size());
        process(q, expanded, &include);
        if (!conds_.empty()) {
            fail("#" + conds_.back().word + " has no #endif", conds_.back().line);
        }
        std::vector<Token> out;
        out.reserve(expanded.size());
        for (HTok& h : expanded) out.push_back(std::move(h.t));
        if (!lineMarks_.empty()) {
            for (Token& t : out) t.line = mapLine(t.line);
        }
        splitNegatives(out);
        return out;
    }

    // Whether a file needs the expander at all: a directive, or a predefined macro's name.
    static bool needed(const std::vector<Token>& toks, const NexaMacroTable* predefined) {
        for (const Token& t : toks) {
            if (t.type == TokenType::Define || t.type == TokenType::Undef || t.type == TokenType::Directive) return true;
            if (t.type == TokenType::Identifier && !t.value.empty() && t.value[0] == '_' &&
                ((predefined && predefined->count(t.value)) || t.value == "_Pragma")) {
                return true;
            }
        }
        return false;
    }

private:
    using HideSet = std::shared_ptr<const std::set<std::string>>;
    struct HTok {
        Token t;
        HideSet hide;
    };
    // One #if ... #endif: whether its lines are read now, whether a branch was taken,
    // and whether the lines around it are read at all.
    struct Cond {
        std::string word;
        size_t line;
        bool active;
        bool taken;
        bool outerActive;
        bool sawElse;
    };
    // A function-like macro being put in: its arguments as written (the variadic ones
    // joined last) and, when needed, expanded.
    struct Call {
        const NexaMacro* m;
        std::string name;
        size_t line;
        HideSet hs;
        std::vector<std::vector<HTok>> raw;
        std::vector<std::vector<HTok>> exp;
        std::vector<char> expDone;
        bool vaGiven = false;
        std::vector<std::pair<const std::set<std::string>*, HideSet>> joined;
    };

    NexaMacroTable& table_;
    std::string file_;
    NexaMacroCache* shared_;
    HasInclude hasInclude_;
    // #line: from the line after each one, the lines are counted from its number, and
    // __FILE__ is its file when it names one. Applied to the tokens as run() hands them on.
    struct LineMark {
        size_t from;
        long long delta;
        std::string file;
    };
    std::vector<LineMark> lineMarks_;
    const LineMark* markAt(size_t line) const {
        const LineMark* m = nullptr;
        for (const LineMark& lm : lineMarks_) {
            if (lm.from <= line) m = &lm;
        }
        return m;
    }
    size_t mapLine(size_t line) const {
        const LineMark* m = markAt(line);
        return m ? static_cast<size_t>(static_cast<long long>(line) + m->delta) : line;
    }
    std::vector<Cond> conds_;

    [[noreturn]] void fail(const std::string& what, size_t line) const {
        std::string msg;
        if (!file_.empty()) msg += file_ + ": ";
        msg += what + " at line " + std::to_string(mapLine(line));
        throw std::runtime_error(msg);
    }

    bool skipping() const { return !conds_.empty() && !conds_.back().active; }

    const NexaMacroTable* predefined() const { return shared_ ? &shared_->predefined : nullptr; }

    const NexaMacro* lookup(const std::string& name) const {
        auto it = table_.find(name);
        if (it != table_.end()) return &it->second;
        if (const NexaMacroTable* p = predefined()) {
            auto pit = p->find(name);
            if (pit != p->end()) return &pit->second;
        }
        return nullptr;
    }

    // The lexer reads -1 as one literal where nothing ends an expression before it.
    // After an expansion something may: x NEG where NEG is -1 is x - 1.
    static void splitNegatives(std::vector<Token>& toks) {
        bool any = false;
        for (size_t i = 1; i < toks.size() && !any; i++) {
            const Token& t = toks[i];
            any = (t.type == TokenType::Number || t.type == TokenType::Float) && t.value.size() > 1 &&
                  t.value[0] == '-' && Lexer::endsExpressionToken(toks[i - 1].type);
        }
        if (!any) return;
        std::vector<Token> out;
        out.reserve(toks.size() + 4);
        for (Token& t : toks) {
            if ((t.type == TokenType::Number || t.type == TokenType::Float) && t.value.size() > 1 && t.value[0] == '-' &&
                !out.empty() && Lexer::endsExpressionToken(out.back().type)) {
                out.push_back({TokenType::Minus, "-", t.line});
                t.value.erase(0, 1);
            }
            out.push_back(std::move(t));
        }
        toks = std::move(out);
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

    static void skipBlanks(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    }

    static std::string readName(const std::string& s, size_t& i) {
        skipBlanks(s, i);
        if (i >= s.size() || !isIdentStart(s[i])) return "";
        const size_t b = i;
        while (i < s.size() && isIdentChar(s[i])) i++;
        return s.substr(b, i - b);
    }

    // Nothing but blanks or a // comment from i on.
    static bool restEmpty(const std::string& s, size_t i) {
        skipBlanks(s, i);
        return i >= s.size() || s.compare(i, 2, "//") == 0 || s.compare(i, 2, "/*") == 0;
    }

    void checkName(const std::string& name, const std::string& directive, size_t line) const {
        if (name.empty()) fail(directive + " needs a name", line);
        std::vector<Token> t = Lexer(name).tokenize();
        if (t.empty() || t[0].type != TokenType::Identifier) {
            fail(directive + " " + name + ": '" + name + "' is a Nexa keyword and cannot be a macro", line);
        }
    }

    static bool sameMacro(const NexaMacro& a, const NexaMacro& b) {
        if (a.fnLike != b.fnLike || a.variadic != b.variadic || a.builtin != b.builtin || a.params != b.params ||
            a.body.size() != b.body.size()) {
            return false;
        }
        for (size_t i = 0; i < a.body.size(); i++) {
            if (a.body[i].type != b.body[i].type || a.body[i].value != b.body[i].value) return false;
        }
        return true;
    }

    void add(const std::string& name, NexaMacro m, size_t line) {
        if (const NexaMacroTable* p = predefined()) {
            auto pit = p->find(name);
            if (pit != p->end()) {
                if (sameMacro(pit->second, m)) return;
                fail("#define " + name + ": " + name + " is predefined and cannot be changed", line);
            }
        }
        auto it = table_.find(name);
        if (it != table_.end() && !sameMacro(it->second, m)) {
            std::string where = "line " + std::to_string(it->second.line);
            if (it->second.file != m.file && !it->second.file.empty()) where += " of " + it->second.file;
            fail("#define " + name + ": already defined differently (" + where + "); #undef it first", line);
        }
        table_[name] = std::move(m);
    }

    static int paramIndex(const NexaMacro& m, const Token& b) {
        if (b.type != TokenType::Identifier) return -1;
        for (size_t p = 0; p < m.params.size(); p++) {
            if (m.params[p] == b.value) return static_cast<int>(p);
        }
        if (m.variadic && b.value == "__VA_ARGS__") return static_cast<int>(m.params.size());
        return -1;
    }

    static bool isVaOpt(const NexaMacro& m, const Token& b) {
        return m.variadic && b.type == TokenType::Identifier && b.value == "__VA_OPT__";
    }

    // The body index of the ')' closing the '(' at `open`; body.size() if none.
    static size_t closeParen(const std::vector<Token>& body, size_t open) {
        int depth = 0;
        for (size_t j = open; j < body.size(); j++) {
            if (body[j].type == TokenType::LParen) depth++;
            if (body[j].type == TokenType::RParen && --depth == 0) return j;
        }
        return body.size();
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
                skipBlanks(s, i);
                if (i < s.size() && s[i] == ')' && m.params.empty()) {
                    i++;
                    break;
                }
                if (s.compare(i, 3, "...") == 0) {
                    m.variadic = true;
                    i += 3;
                    skipBlanks(s, i);
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
                skipBlanks(s, i);
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
        Lexer lex(s.substr(i), file_, d.line);
        lex.setMacroBody(true);
        m.body = lex.tokenize();
        while (!m.body.empty() && m.body.back().type == TokenType::Eof) m.body.pop_back();
        const std::string what = "#define " + name + ": ";
        for (size_t k = 0; k < m.body.size(); k++) {
            const Token& b = m.body[k];
            if (b.type == TokenType::HashHash) {
                if (k == 0 || k + 1 == m.body.size()) fail(what + "'##' cannot be at either end of a macro body", d.line);
                m.paste = true;
            } else if (b.type == TokenType::Hash) {
                if (!m.fnLike) fail(what + "'#' only works in a function-like macro, before a parameter", d.line);
                if (k + 1 == m.body.size() || paramIndex(m, m.body[k + 1]) < 0) {
                    fail(what + "'#' must be followed by a parameter name", d.line);
                }
                m.paste = true;
            } else if (b.type == TokenType::Identifier && b.value == "__VA_ARGS__" && !m.variadic) {
                fail(what + "__VA_ARGS__ needs '...' in the parameter list", d.line);
            } else if (b.type == TokenType::Identifier && b.value == "__VA_OPT__") {
                if (!m.variadic) fail(what + "__VA_OPT__ needs '...' in the parameter list", d.line);
                if (k + 1 >= m.body.size() || m.body[k + 1].type != TokenType::LParen ||
                    closeParen(m.body, k + 1) == m.body.size()) {
                    fail(what + "__VA_OPT__ needs (tokens) after it", d.line);
                }
                m.paste = true;
            }
        }
        add(name, std::move(m), d.line);
    }

    void undef(const Token& d) {
        size_t i = 0;
        const std::string name = readName(d.value, i);
        checkName(name, "#undef", d.line);
        if (!restEmpty(d.value, i)) fail("#undef " + name + ": expected the end of the line after the name", d.line);
        if (const NexaMacroTable* p = predefined()) {
            if (p->count(name)) fail("#undef " + name + ": " + name + " is predefined and cannot be undefined", d.line);
        }
        table_.erase(name);
    }

    // ---- #if and its family ----

    bool isDefined(const std::string& name) const { return name == "__has_include" || lookup(name) != nullptr; }

    // #ifdef NAME, #ifndef NAME: the name, checked.
    std::string condName(const std::string& word, const std::string& rest, size_t line) const {
        size_t i = 0;
        const std::string name = readName(rest, i);
        if (name.empty()) fail("#" + word + " needs a name", line);
        if (!restEmpty(rest, i)) fail("#" + word + " " + name + ": expected the end of the line after the name", line);
        return name;
    }

    void directive(const Token& d) {
        const size_t sp = d.value.find(' ');
        const std::string word = d.value.substr(0, sp);
        const std::string rest = sp == std::string::npos ? "" : d.value.substr(sp + 1);
        const size_t line = d.line;
        if (word.empty()) return;  // a '#' alone: C's null directive
        if (word == "if" || word == "ifdef" || word == "ifndef") {
            const bool outer = !skipping();
            bool v = false;
            if (outer) {
                if (word == "if") v = evalCondition(rest, line, "#if");
                else v = isDefined(condName(word, rest, line)) == (word == "ifdef");
            }
            conds_.push_back({word, line, v, v, outer, false});
            return;
        }
        if (word == "elif" || word == "elifdef" || word == "elifndef") {
            if (conds_.empty()) fail("#" + word + " without #if", line);
            Cond& c = conds_.back();
            if (c.sawElse) fail("#" + word + " after #else", line);
            if (!c.outerActive || c.taken) {
                c.active = false;
                return;
            }
            bool v;
            if (word == "elif") v = evalCondition(rest, line, "#elif");
            else v = isDefined(condName(word, rest, line)) == (word == "elifdef");
            c.active = v;
            c.taken = v;
            return;
        }
        if (word == "else") {
            if (conds_.empty()) fail("#else without #if", line);
            Cond& c = conds_.back();
            if (c.sawElse) fail("#else after #else", line);
            c.sawElse = true;
            c.active = c.outerActive && !c.taken;
            c.taken = true;
            return;
        }
        if (word == "endif") {
            if (conds_.empty()) fail("#endif without #if", line);
            conds_.pop_back();
            return;
        }
        if (skipping()) return;
        size_t i = 0;
        skipBlanks(rest, i);
        std::string msg = rest.substr(i);
        while (!msg.empty() && (msg.back() == ' ' || msg.back() == '\t')) msg.pop_back();
        if (word == "error") fail("#error" + (msg.empty() ? std::string() : " " + msg), line);
        const std::string where = (file_.empty() ? std::string() : file_ + ":") + std::to_string(mapLine(line)) + ": ";
        if (word == "warning") {
            std::cerr << "[Nexa] Warning: " << where << "#warning" << (msg.empty() ? std::string() : " " + msg) << "\n";
        }
        if (word == "line") lineDirective(msg, line);
        if (word == "pragma") pragma(msg, line, where);
    }

    // #line N or #line N "file": the next line is line N (of that file, for __FILE__).
    void lineDirective(const std::string& text, size_t line) {
        size_t i = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') i++;
        if (i == 0 || i > 10) fail("#line needs a line number: #line 100 or #line 100 \"name.nxa\"", line);
        const long long n = std::strtoll(text.substr(0, i).c_str(), nullptr, 10);
        if (n < 1 || n > 2147483647) fail("#line " + text.substr(0, i) + ": a line number is 1 to 2147483647", line);
        skipBlanks(text, i);
        std::string file = markAt(line) ? markAt(line)->file : std::string();
        if (i < text.size()) {
            const size_t close = text.find('"', i + 1);
            if (text[i] != '"' || close == std::string::npos || !restEmpty(text, close + 1)) {
                fail("#line " + text.substr(0, text.find_first_of(" \t")) +
                     ": after the number comes a file name in quotes, or the end of the line", line);
            }
            file = text.substr(i + 1, close - i - 1);
        }
        lineMarks_.push_back({line + 1, n - static_cast<long long>(line + 1), file});
    }

    // #pragma once: every file is read once already, so it asks for nothing more.
    // #pragma message("text") or #pragma message "text": prints the text. Another
    // pragma is told to be ignored, as a C compiler warns of one it does not know.
    void pragma(const std::string& text, size_t line, const std::string& where) {
        size_t i = 0;
        const std::string name = readName(text, i);
        if (name == "once" && restEmpty(text, i)) return;
        if (name == "message") {
            // One string or several side by side, joined as C joins them: a macro can
            // put "TODO: " before its argument's text.
            skipBlanks(text, i);
            const bool paren = i < text.size() && text[i] == '(';
            if (paren) i++;
            skipBlanks(text, i);
            std::string said;
            bool any = false, bad = false;
            while (!bad && i < text.size() && text[i] == '"') {
                any = true;
                size_t k = i + 1;
                for (; k < text.size() && text[k] != '"'; k++) {
                    if (text[k] == '\\' && k + 1 < text.size()) k++;
                    said += text[k];
                }
                if (k >= text.size()) bad = true;
                i = k + 1;
                skipBlanks(text, i);
            }
            if (paren && !bad) {
                if (i < text.size() && text[i] == ')') i++;
                else bad = true;
            }
            if (!any || bad || !restEmpty(text, i)) {
                fail("#pragma message takes the text in quotes: #pragma message(\"text\")", line);
            }
            std::cerr << "[Nexa] " << where << "#pragma message: " << said << "\n";
            return;
        }
        std::cerr << "[Nexa] Warning: " << where << "#pragma " << text
                  << " is ignored: Nexa knows #pragma once and #pragma message(\"text\")\n";
    }

    // An #if's expression: defined X and defined(X) first, then macros, then C's integer
    // arithmetic. A name left over is 0, as in C.
    bool evalCondition(const std::string& text, size_t line, const char* what) {
        std::vector<Token> toks = Lexer(text, file_, line).tokenize();
        while (!toks.empty() && toks.back().type == TokenType::Eof) toks.pop_back();
        if (toks.empty()) fail(std::string(what) + " needs a condition", line);
        std::vector<HTok> pre;
        for (size_t i = 0; i < toks.size(); i++) {
            if (toks[i].type == TokenType::Identifier && toks[i].value == "__has_include") {
                // __has_include(<std/io>) or __has_include("file.nxa"): 1 when that #include
                // would find what it names.
                const std::string how = std::string(what) + ": __has_include takes (<std/io>) or (\"file.nxa\")";
                size_t j = i + 1;
                if (j >= toks.size() || toks[j].type != TokenType::LParen) fail(how, line);
                j++;
                std::string raw;
                if (j < toks.size() && toks[j].type == TokenType::String) {
                    raw = "\"" + toks[j].value + "\"";
                    j++;
                } else if (j < toks.size() && toks[j].type == TokenType::Less) {
                    for (j++; j < toks.size() && toks[j].type != TokenType::Greater; j++) {
                        if (toks[j].type == TokenType::RParen) break;
                        raw += toks[j].value;
                    }
                    if (j >= toks.size() || toks[j].type != TokenType::Greater || raw.empty()) fail(how, line);
                    raw = "<" + raw + ">";
                    j++;
                } else {
                    fail(how, line);
                }
                if (j >= toks.size() || toks[j].type != TokenType::RParen) fail(how, line);
                const bool v = hasInclude_ && hasInclude_(raw);
                pre.push_back({{TokenType::Number, v ? "1" : "0", line}, nullptr});
                i = j;
                continue;
            }
            if (toks[i].type == TokenType::Identifier && toks[i].value == "defined") {
                size_t j = i + 1;
                const bool paren = j < toks.size() && toks[j].type == TokenType::LParen;
                if (paren) j++;
                if (j >= toks.size() || toks[j].value.empty() || !isIdentStart(toks[j].value[0])) {
                    fail(std::string(what) + ": defined needs a macro name", line);
                }
                const bool v = isDefined(toks[j].value);
                if (paren) {
                    if (j + 1 >= toks.size() || toks[j + 1].type != TokenType::RParen) {
                        fail(std::string(what) + ": defined(" + toks[j].value + " needs a ')'", line);
                    }
                    j++;
                }
                pre.push_back({{TokenType::Number, v ? "1" : "0", line}, nullptr});
                i = j;
                continue;
            }
            pre.push_back({toks[i], nullptr});
        }
        std::vector<HTok> ex = expandAll(pre);
        std::vector<Token> e;
        e.reserve(ex.size());
        for (HTok& h : ex) e.push_back(std::move(h.t));
        splitNegatives(e);
        CondEval ev{e, 0, what, line, this};
        const long long v = ev.ternary(true);
        if (ev.pos < e.size()) ev.unexpected();
        return v != 0;
    }

    struct CondEval {
        const std::vector<Token>& t;
        size_t pos;
        const char* what;
        size_t line;
        const NexaMacroExpander* self;

        [[noreturn]] void unexpected() const {
            if (pos >= t.size()) self->fail(std::string(what) + ": the condition ends too soon", line);
            self->fail(std::string(what) + ": unexpected '" + t[pos].value + "' in the condition", line);
        }
        bool at(TokenType ty) const { return pos < t.size() && t[pos].type == ty; }
        bool eat(TokenType ty) {
            if (!at(ty)) return false;
            pos++;
            return true;
        }

        long long ternary(bool live) {
            const long long c = binary(0, live);
            if (!eat(TokenType::Question)) return c;
            const long long a = ternary(live && c != 0);
            if (!eat(TokenType::Colon)) unexpected();
            const long long b = ternary(live && c == 0);
            return c ? a : b;
        }

        static int level(TokenType ty) {
            switch (ty) {
                case TokenType::Or: return 1;
                case TokenType::And: return 2;
                case TokenType::BitOr: return 3;
                case TokenType::BitXor: return 4;
                case TokenType::BitAnd: return 5;
                case TokenType::Equals: case TokenType::NotEquals: return 6;
                case TokenType::Less: case TokenType::LessEq: case TokenType::Greater: case TokenType::GreaterEq: return 7;
                case TokenType::Shl: case TokenType::Shr: return 8;
                case TokenType::Plus: case TokenType::Minus: return 9;
                case TokenType::Star: case TokenType::Slash: case TokenType::Percent: return 10;
                default: return 0;
            }
        }

        long long binary(int min, bool live) {
            long long l = unary(live);
            while (pos < t.size()) {
                const TokenType op = t[pos].type;
                const int lv = level(op);
                if (lv == 0 || lv <= min) break;
                pos++;
                const bool rliveAnd = op == TokenType::And ? live && l != 0 : op == TokenType::Or ? live && l == 0 : live;
                const long long r = binary(lv, rliveAnd);
                const unsigned long long ul = static_cast<unsigned long long>(l), ur = static_cast<unsigned long long>(r);
                switch (op) {
                    case TokenType::Or: l = (l || r) ? 1 : 0; break;
                    case TokenType::And: l = (l && r) ? 1 : 0; break;
                    case TokenType::BitOr: l = l | r; break;
                    case TokenType::BitXor: l = l ^ r; break;
                    case TokenType::BitAnd: l = l & r; break;
                    case TokenType::Equals: l = l == r; break;
                    case TokenType::NotEquals: l = l != r; break;
                    case TokenType::Less: l = l < r; break;
                    case TokenType::LessEq: l = l <= r; break;
                    case TokenType::Greater: l = l > r; break;
                    case TokenType::GreaterEq: l = l >= r; break;
                    case TokenType::Shl: l = static_cast<long long>(ul << (ur & 63)); break;
                    case TokenType::Shr: l = l >> (ur & 63); break;
                    case TokenType::Plus: l = static_cast<long long>(ul + ur); break;
                    case TokenType::Minus: l = static_cast<long long>(ul - ur); break;
                    case TokenType::Star: l = static_cast<long long>(ul * ur); break;
                    case TokenType::Slash:
                    case TokenType::Percent:
                        if (r == 0) {
                            if (live) self->fail(std::string(what) + ": division by zero in the condition", line);
                            l = 0;
                        } else if (r == -1) {
                            l = op == TokenType::Slash ? static_cast<long long>(0 - ul) : 0;
                        } else {
                            l = op == TokenType::Slash ? l / r : l % r;
                        }
                        break;
                    default: break;
                }
            }
            return l;
        }

        long long unary(bool live) {
            if (eat(TokenType::Not)) return unary(live) == 0;
            if (eat(TokenType::BitNot)) return ~unary(live);
            if (eat(TokenType::Minus)) return static_cast<long long>(0 - static_cast<unsigned long long>(unary(live)));
            if (eat(TokenType::Plus)) return unary(live);
            if (eat(TokenType::LParen)) {
                const long long v = ternary(live);
                if (!eat(TokenType::RParen)) unexpected();
                return v;
            }
            if (pos >= t.size()) unexpected();
            const Token& k = t[pos];
            switch (k.type) {
                case TokenType::Number: {
                    pos++;
                    const bool neg = k.value[0] == '-';
                    const unsigned long long u = std::strtoull(k.value.c_str() + (neg ? 1 : 0), nullptr, 0);
                    return neg ? static_cast<long long>(0 - u) : static_cast<long long>(u);
                }
                case TokenType::Char: pos++; return static_cast<unsigned char>(k.value[0]);
                case TokenType::True: pos++; return 1;
                case TokenType::False: pos++; return 0;
                case TokenType::Identifier: pos++; return 0;
                case TokenType::Float: self->fail(std::string(what) + ": " + k.value + " is not an integer", line);
                case TokenType::String: self->fail(std::string(what) + ": a string cannot be a condition", line);
                default: break;
            }
            // A keyword's word (fn, main, let ...) is a name too, and names are 0.
            if (!k.value.empty() && isIdentStart(k.value[0])) {
                pos++;
                return 0;
            }
            unexpected();
        }
    };

    // ---- expansion ----

    // Takes a function-like macro's arguments off the front of q, after the '(':
    // split at commas outside (), [] and {}, up to the matching ')'.
    std::vector<std::vector<HTok>> collectArgs(std::deque<HTok>& q, const std::string& name, size_t line) {
        std::vector<std::vector<HTok>> args(1);
        std::vector<TokenType> open;  // closers still expected inside the call
        while (true) {
            if (!q.empty() && (q.front().t.type == TokenType::Directive || q.front().t.type == TokenType::Define ||
                               q.front().t.type == TokenType::Undef)) {
                fail("A directive cannot be inside a call to macro " + name, q.front().t.line);
            }
            const bool end = q.empty() || q.front().t.type == TokenType::Eof;
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

    // A token as it would be written: what # makes a string of and ## joins.
    static std::string spell(const Token& t) {
        if (t.type != TokenType::String && t.type != TokenType::Char) return t.value;
        const char q = t.type == TokenType::String ? '"' : '\'';
        std::string s(1, q);
        for (char c : t.value) {
            switch (c) {
                case '\n': s += "\\n"; break;
                case '\t': s += "\\t"; break;
                case '\r': s += "\\r"; break;
                case '\0': s += "\\0"; break;
                case '\\': s += "\\\\"; break;
                case '"': s += q == '"' ? "\\\"" : "\""; break;
                case '\'': s += q == '\'' ? "\\'" : "'"; break;
                default:
                    if (static_cast<unsigned char>(c) < 32 || c == 127) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned char>(c));
                        s += buf;
                    } else {
                        s += c;
                    }
            }
        }
        s += q;
        return s;
    }

    HideSet joinHide(Call& c, const HideSet& a) {
        for (const auto& j : c.joined) {
            if (j.first == a.get()) return j.second;
        }
        HideSet r = with(a, c.hs);
        c.joined.emplace_back(a.get(), r);
        return r;
    }

    void put(Call& c, const HTok& a, std::vector<HTok>& out) {
        HTok x = a;
        x.t.line = c.line;
        x.hide = joinHide(c, a.hide);
        out.push_back(std::move(x));
    }

    HTok fromBody(const Call& c, const Token& b) const {
        Token t = b;
        t.line = c.line;
        return {std::move(t), c.hs};
    }

    const std::vector<HTok>& expandedArg(Call& c, int p) {
        if (!c.expDone[p]) {
            c.exp[p] = expandAll(c.raw[p]);
            c.expDone[p] = 1;
        }
        return c.exp[p];
    }

    HTok stringize(const Call& c, int p) const {
        std::string s;
        for (size_t i = 0; i < c.raw[p].size(); i++) {
            const Token& a = c.raw[p][i].t;
            if (i > 0 && a.spaced) s += ' ';
            s += spell(a);
        }
        Token t{TokenType::String, s, c.line};
        return {std::move(t), c.hs};
    }

    HTok paste(const Call& c, const HTok& l, const HTok& r) const {
        const std::string text = spell(l.t) + spell(r.t);
        std::vector<Token> toks = Lexer(text, file_, c.line).tokenize();
        while (!toks.empty() && toks.back().type == TokenType::Eof) toks.pop_back();
        if (toks.size() != 1) {
            fail("Macro " + c.name + ": '##' joins '" + spell(l.t) + "' and '" + spell(r.t) + "' into '" + text +
                     "', which is not one token",
                 c.line);
        }
        Token t = std::move(toks[0]);
        t.line = c.line;
        t.spaced = l.t.spaced;
        return {std::move(t), c.hs};
    }

    // The body from `from` to `to`, its parameters, # and ## put in (C 6.10.3.2-3).
    void substitute(Call& c, size_t from, size_t to, std::vector<HTok>& out) {
        const std::vector<Token>& body = c.m->body;
        const int vaIndex = c.m->variadic ? static_cast<int>(c.m->params.size()) : -1;
        bool placemarker = false;  // the operand before a ## put in nothing
        for (size_t i = from; i < to;) {
            const Token& b = body[i];
            if (b.type == TokenType::HashHash) {
                i++;
                std::vector<HTok> rhs;
                bool commaRule = false;
                if (body[i].type == TokenType::Hash) {
                    rhs.push_back(stringize(c, paramIndex(*c.m, body[i + 1])));
                    i += 2;
                } else if (paramIndex(*c.m, body[i]) >= 0) {
                    const int p = paramIndex(*c.m, body[i]);
                    for (const HTok& a : c.raw[p]) put(c, a, rhs);
                    i++;
                    // GNU's , ## __VA_ARGS__: the comma goes when there is nothing after it.
                    commaRule = p == vaIndex && !placemarker && !out.empty() && out.back().t.type == TokenType::Comma;
                } else {
                    rhs.push_back(fromBody(c, body[i]));
                    i++;
                }
                if (commaRule) {
                    if (rhs.empty()) out.pop_back();
                    for (HTok& r : rhs) out.push_back(std::move(r));
                    placemarker = false;
                    continue;
                }
                if (placemarker || out.empty()) {
                    for (HTok& r : rhs) out.push_back(std::move(r));
                    placemarker = rhs.empty();
                    continue;
                }
                placemarker = false;
                if (rhs.empty()) continue;
                HTok l = std::move(out.back());
                out.pop_back();
                out.push_back(paste(c, l, rhs[0]));
                for (size_t k = 1; k < rhs.size(); k++) out.push_back(std::move(rhs[k]));
                continue;
            }
            const bool pasteNext = i + 1 < to && body[i + 1].type == TokenType::HashHash;
            if (b.type == TokenType::Hash) {
                out.push_back(stringize(c, paramIndex(*c.m, body[i + 1])));
                placemarker = false;
                i += 2;
                continue;
            }
            if (isVaOpt(*c.m, b)) {
                const size_t close = closeParen(body, i + 1);
                const size_t before = out.size();
                if (c.vaGiven) substitute(c, i + 2, close, out);
                placemarker = out.size() == before;
                i = close + 1;
                continue;
            }
            const int p = paramIndex(*c.m, b);
            if (p >= 0) {
                const size_t before = out.size();
                if (pasteNext) {
                    for (const HTok& a : c.raw[p]) put(c, a, out);
                } else {
                    for (const HTok& a : expandedArg(c, p)) put(c, a, out);
                }
                placemarker = out.size() == before;
                i++;
                continue;
            }
            out.push_back(fromBody(c, b));
            placemarker = false;
            i++;
        }
    }

    void putBuiltin(const NexaMacro& m, const HTok& h, std::vector<HTok>& out) {
        Token t;
        t.line = h.t.line;
        t.spaced = h.t.spaced;
        switch (m.builtin) {
            case NexaMacro::Builtin::Line:
                t.type = TokenType::Number;
                t.value = std::to_string(mapLine(h.t.line));
                break;
            case NexaMacro::Builtin::File: {
                t.type = TokenType::String;
                const LineMark* lm = markAt(h.t.line);
                t.value = lm && !lm->file.empty() ? lm->file : file_;
                break;
            }
            default:
                t.type = TokenType::Number;
                t.value = std::to_string(shared_ ? shared_->counter++ : 0);
                break;
        }
        out.push_back({std::move(t), h.hide});
    }

    void process(std::deque<HTok>& q, std::vector<HTok>& out, const IncludeMacros* include) {
        while (!q.empty()) {
            HTok h = std::move(q.front());
            q.pop_front();
            const Token& t = h.t;
            // Directives only come from a file's own lines, never out of an expansion.
            if (t.type == TokenType::Directive) {
                directive(t);
                continue;
            }
            if (include && skipping()) {
                if (t.type == TokenType::Eof) out.push_back(std::move(h));
                continue;
            }
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
            const NexaMacro* found = lookup(t.value);
            // _Pragma("message(\"text\")"): #pragma's text, from a string a macro can build.
            if (!found && t.value == "_Pragma") {
                if (q.size() < 3 || q[0].t.type != TokenType::LParen || q[1].t.type != TokenType::String ||
                    q[2].t.type != TokenType::RParen) {
                    fail("_Pragma takes the pragma as one string: _Pragma(\"message(\\\"text\\\")\")", t.line);
                }
                const std::string text = q[1].t.value;
                const size_t line = t.line;
                q.erase(q.begin(), q.begin() + 3);
                pragma(text, line, (file_.empty() ? std::string() : file_ + ":") + std::to_string(mapLine(line)) + ": ");
                continue;
            }
            if (!found || hidden(h, t.value)) {
                out.push_back(std::move(h));
                continue;
            }
            // Not a copy: nothing below changes the table (only a #define token does).
            const NexaMacro& m = *found;
            if (m.builtin != NexaMacro::Builtin::None) {
                putBuiltin(m, h, out);
                continue;
            }
            const std::string name = t.value;
            const size_t line = t.line;
            if (!m.fnLike && !m.paste) {
                const HideSet hs = with(h.hide, name);
                for (size_t i = m.body.size(); i-- > 0;) {
                    Token b = m.body[i];
                    b.line = line;
                    q.push_front({std::move(b), hs});
                }
                continue;
            }
            Call c;
            c.m = &m;
            c.name = name;
            c.line = line;
            if (m.fnLike) {
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
                             " argument" + (named == 1 && !m.variadic ? "" : "s") + ", given " +
                             std::to_string(args.size()),
                         line);
                }
                c.raw.assign(args.begin(), args.begin() + static_cast<std::ptrdiff_t>(named));
                if (m.variadic) {
                    std::vector<HTok> joined;
                    for (size_t i = named; i < args.size(); i++) {
                        if (i > named) joined.push_back({{TokenType::Comma, ",", line}, nullptr});
                        for (HTok& a : args[i]) joined.push_back(std::move(a));
                    }
                    c.vaGiven = !joined.empty();
                    c.raw.push_back(std::move(joined));
                }
                c.exp.resize(c.raw.size());
                c.expDone.assign(c.raw.size(), 0);
            }
            c.hs = with(h.hide, name);
            std::vector<HTok> result;
            result.reserve(m.body.size());
            substitute(c, 0, m.body.size(), result);
            for (size_t i = result.size(); i-- > 0;) q.push_front(std::move(result[i]));
        }
    }
};

}  // namespace nexa
