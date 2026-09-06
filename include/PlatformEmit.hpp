#pragma once

#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace nexa {

enum class CppTarget { Windows, Linux, Darwin, Wasm };

inline CppTarget hostCppTarget() {
#ifdef _WIN32
    return CppTarget::Windows;
#elif defined(__APPLE__)
    return CppTarget::Darwin;
#else
    return CppTarget::Linux;
#endif
}

struct PlatformMacros {
    bool win32 = false;
    bool apple = false;
    bool linux = false;
    bool emscripten = false;
    bool wasi = false;
    bool nexaWasm = false;
    bool wasm = false;
    bool wasm32 = false;
    bool wasm64 = false;
};

inline PlatformMacros macrosFor(CppTarget target) {
    PlatformMacros m;
    switch (target) {
        case CppTarget::Windows:
            m.win32 = true;
            break;
        case CppTarget::Linux:
            m.linux = true;
            break;
        case CppTarget::Darwin:
            m.apple = true;
            break;
        case CppTarget::Wasm:
            m.nexaWasm = true;
            m.emscripten = true;
            m.wasm = true;
            m.wasm32 = true;
            break;
    }
    return m;
}

inline std::optional<bool> platformMacroDefined(const std::string& name, const PlatformMacros& env) {
    if (name == "_WIN32") return env.win32;
    if (name == "__APPLE__") return env.apple;
    if (name == "__linux__") return env.linux;
    if (name == "__EMSCRIPTEN__") return env.emscripten;
    if (name == "__wasi__") return env.wasi;
    if (name == "NEXA_WASM") return env.nexaWasm;
    if (name == "__wasm__") return env.wasm;
    if (name == "__wasm32__") return env.wasm32;
    if (name == "__wasm64__") return env.wasm64;
    return std::nullopt;
}

class PlatformExprParser {
public:
    PlatformExprParser(const std::string& expr, const PlatformMacros& env)
        : s_(expr), env_(env) {}

    std::optional<bool> eval() {
        skipWs();
        if (i_ >= s_.size()) return std::nullopt;
        auto v = parseOr();
        skipWs();
        if (i_ < s_.size() || unknown_) return std::nullopt;
        return v;
    }

private:
    const std::string& s_;
    const PlatformMacros& env_;
    size_t i_ = 0;
    bool unknown_ = false;

    void skipWs() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) i_++;
    }

    static bool isIdentStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    static bool isIdentChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    std::string parseIdent() {
        skipWs();
        if (i_ >= s_.size() || !isIdentStart(s_[i_])) return {};
        size_t a = i_++;
        while (i_ < s_.size() && isIdentChar(s_[i_])) i_++;
        return s_.substr(a, i_ - a);
    }

    bool match(const char* lit) {
        skipWs();
        size_t n = 0;
        while (lit[n]) n++;
        if (i_ + n > s_.size() || s_.compare(i_, n, lit) != 0) return false;
        i_ += n;
        return true;
    }

    std::optional<bool> parseOr() {
        auto left = parseAnd();
        if (unknown_) return std::nullopt;
        while (true) {
            skipWs();
            if (!match("||")) break;
            auto right = parseAnd();
            if (unknown_ || !left || !right) return std::nullopt;
            left = *left || *right;
        }
        return left;
    }

    std::optional<bool> parseAnd() {
        auto left = parseUnary();
        if (unknown_) return std::nullopt;
        while (true) {
            skipWs();
            if (!match("&&")) break;
            auto right = parseUnary();
            if (unknown_ || !left || !right) return std::nullopt;
            left = *left && *right;
        }
        return left;
    }

    std::optional<bool> parseUnary() {
        skipWs();
        if (match("!")) {
            auto v = parseUnary();
            if (!v) return std::nullopt;
            return !*v;
        }
        return parsePrimary();
    }

    std::optional<bool> parsePrimary() {
        skipWs();
        if (match("(")) {
            auto v = parseOr();
            skipWs();
            if (!match(")")) {
                unknown_ = true;
                return std::nullopt;
            }
            return v;
        }
        if (i_ < s_.size() && (s_[i_] == '0' || s_[i_] == '1') &&
            (i_ + 1 >= s_.size() || !isIdentChar(s_[i_ + 1]))) {
            bool v = s_[i_] == '1';
            i_++;
            return v;
        }
        std::string id = parseIdent();
        if (id.empty()) {
            unknown_ = true;
            return std::nullopt;
        }
        if (id == "defined") {
            skipWs();
            bool paren = match("(");
            std::string name = parseIdent();
            if (name.empty()) {
                unknown_ = true;
                return std::nullopt;
            }
            if (paren) {
                skipWs();
                if (!match(")")) {
                    unknown_ = true;
                    return std::nullopt;
                }
            }
            auto d = platformMacroDefined(name, env_);
            if (!d) {
                unknown_ = true;
                return std::nullopt;
            }
            return d;
        }
        auto d = platformMacroDefined(id, env_);
        if (!d) {
            unknown_ = true;
            return std::nullopt;
        }
        return d;
    }
};

inline std::optional<bool> evalPlatformExpr(const std::string& expr, const PlatformMacros& env) {
    return PlatformExprParser(expr, env).eval();
}

enum class PpDirKind { None, If, Ifdef, Ifndef, Elif, Else, Endif };

inline std::string stripLineComment(const std::string& line) {
    bool inStr = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"' && (i == 0 || line[i - 1] != '\\')) inStr = !inStr;
        if (!inStr && c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return line.substr(0, i);
        }
    }
    return line;
}

inline PpDirKind parsePpDirective(const std::string& line, std::string& expr) {
    expr.clear();
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
    if (i >= line.size() || line[i] != '#') return PpDirKind::None;
    i++;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
    size_t kwStart = i;
    while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i]))) i++;
    std::string kw = line.substr(kwStart, i - kwStart);
    PpDirKind kind = PpDirKind::None;
    if (kw == "if") kind = PpDirKind::If;
    else if (kw == "ifdef") kind = PpDirKind::Ifdef;
    else if (kw == "ifndef") kind = PpDirKind::Ifndef;
    else if (kw == "elif") kind = PpDirKind::Elif;
    else if (kw == "else") kind = PpDirKind::Else;
    else if (kw == "endif") kind = PpDirKind::Endif;
    else return PpDirKind::None;
    std::string rest = stripLineComment(line.substr(i));
    size_t a = 0, b = rest.size();
    while (a < b && std::isspace(static_cast<unsigned char>(rest[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(rest[b - 1]))) b--;
    expr = rest.substr(a, b - a);
    return kind;
}

inline std::string normalizePpCond(PpDirKind kind, const std::string& expr) {
    if (kind == PpDirKind::Ifdef) return "defined(" + expr + ")";
    if (kind == PpDirKind::Ifndef) return "!defined(" + expr + ")";
    return expr;
}

struct PpBranch {
    int dirLine = -1;
    std::string cond;
    int bodyStart = 0;
    int bodyEnd = 0;
};

struct PpIfChain {
    bool ok = false;
    std::vector<PpBranch> branches;
    int endifLine = -1;
};

inline PpIfChain parsePpIfChain(const std::vector<std::string>& lines, int start, int limit) {
    PpIfChain chain;
    std::string expr;
    PpDirKind kind = parsePpDirective(lines[start], expr);
    if (kind != PpDirKind::If && kind != PpDirKind::Ifdef && kind != PpDirKind::Ifndef) return chain;

    PpBranch cur;
    cur.dirLine = start;
    cur.cond = normalizePpCond(kind, expr);
    cur.bodyStart = start + 1;
    int depth = 1;
    for (int i = start + 1; i < limit; i++) {
        PpDirKind k = parsePpDirective(lines[i], expr);
        if (k == PpDirKind::If || k == PpDirKind::Ifdef || k == PpDirKind::Ifndef) {
            depth++;
            continue;
        }
        if (k == PpDirKind::Endif) {
            depth--;
            if (depth == 0) {
                cur.bodyEnd = i;
                chain.branches.push_back(cur);
                chain.endifLine = i;
                chain.ok = !chain.branches.empty();
                return chain;
            }
            continue;
        }
        if (depth != 1) continue;
        if (k == PpDirKind::Elif || k == PpDirKind::Else) {
            cur.bodyEnd = i;
            chain.branches.push_back(cur);
            cur = PpBranch{};
            cur.dirLine = i;
            cur.cond = (k == PpDirKind::Else) ? std::string() : normalizePpCond(PpDirKind::If, expr);
            cur.bodyStart = i + 1;
        }
    }
    return chain;
}

inline std::string stripInactivePlatformGuardsRange(
    const std::vector<std::string>& lines, int start, int end, const PlatformMacros& env
) {
    std::ostringstream out;
    int i = start;
    while (i < end) {
        std::string expr;
        PpDirKind kind = parsePpDirective(lines[i], expr);
        if (kind != PpDirKind::If && kind != PpDirKind::Ifdef && kind != PpDirKind::Ifndef) {
            out << lines[i] << "\n";
            i++;
            continue;
        }
        PpIfChain chain = parsePpIfChain(lines, i, end);
        if (!chain.ok) {
            out << lines[i] << "\n";
            i++;
            continue;
        }

        bool allKnown = true;
        std::vector<std::optional<bool>> evs;
        evs.reserve(chain.branches.size());
        for (const PpBranch& b : chain.branches) {
            if (b.cond.empty()) {
                evs.emplace_back(true);
                continue;
            }
            auto v = evalPlatformExpr(b.cond, env);
            evs.push_back(v);
            if (!v) allKnown = false;
        }

        if (allKnown) {
            bool taken = false;
            for (size_t bi = 0; bi < chain.branches.size(); bi++) {
                bool take = evs[bi].value();
                if (chain.branches[bi].cond.empty()) take = !taken;
                if (take) {
                    out << stripInactivePlatformGuardsRange(
                        lines, chain.branches[bi].bodyStart, chain.branches[bi].bodyEnd, env
                    );
                    taken = true;
                    break;
                }
            }
        } else {
            out << lines[chain.branches[0].dirLine] << "\n";
            for (size_t bi = 0; bi < chain.branches.size(); bi++) {
                if (bi > 0) out << lines[chain.branches[bi].dirLine] << "\n";
                out << stripInactivePlatformGuardsRange(
                    lines, chain.branches[bi].bodyStart, chain.branches[bi].bodyEnd, env
                );
            }
            out << lines[chain.endifLine] << "\n";
        }
        i = chain.endifLine + 1;
    }
    return out.str();
}

// Drop #if/#ifdef/#elif/#else/#endif branches that are inactive for `target`
// when every condition in the chain is a known OS/environment macro.
// Architecture checks and unknown macros are left intact.
inline std::string stripInactivePlatformGuards(const std::string& src, CppTarget target) {
    std::vector<std::string> lines;
    std::istringstream in(src);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return stripInactivePlatformGuardsRange(lines, 0, static_cast<int>(lines.size()), macrosFor(target));
}

} // namespace nexa
