#pragma once

#include <string>

namespace nexa {

inline std::string jsonRuntimeCpp() {
    return R"NEXA_JSON(
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstdint>

struct __nexa_json {
    enum Kind { Null, Bool, Number, String, Array, Object, Error };
    Kind kind = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<__nexa_json> a;
    std::map<std::string, __nexa_json> o;

    static __nexa_json nullv() { return __nexa_json(); }
    static __nexa_json boolean(bool v) { __nexa_json j; j.kind = Bool; j.b = v; return j; }
    static __nexa_json number(double v) { __nexa_json j; j.kind = Number; j.n = v; return j; }
    static __nexa_json str(const std::string& v) { __nexa_json j; j.kind = String; j.s = v; return j; }
    static __nexa_json arr() { __nexa_json j; j.kind = Array; return j; }
    static __nexa_json obj() { __nexa_json j; j.kind = Object; return j; }
    static __nexa_json err(const std::string& m) { __nexa_json j; j.kind = Error; j.s = m; return j; }

    bool ok() const { return kind != Error; }
    bool is_null() const { return kind == Null; }
    bool is_bool() const { return kind == Bool; }
    bool is_number() const { return kind == Number; }
    bool is_string() const { return kind == String; }
    bool is_array() const { return kind == Array; }
    bool is_object() const { return kind == Object; }
    bool is_error() const { return kind == Error; }

    std::string kind_name() const {
        switch (kind) {
            case Null: return "null";
            case Bool: return "bool";
            case Number: return "number";
            case String: return "string";
            case Array: return "array";
            case Object: return "object";
            case Error: return "error";
        }
        return "null";
    }

    bool as_bool() const { return kind == Bool && b; }
    int as_int() const {
        if (kind != Number || !std::isfinite(n)) return 0;
        if (n > 2147483647.0) return 2147483647;
        if (n < -2147483648.0) return (int)(-2147483647 - 1);
        return (int)n;
    }
    double as_float() const { return kind == Number ? n : 0.0; }
    std::string as_string() const {
        if (kind == String || kind == Error) return s;
        return std::string();
    }
    int len() const {
        if (kind == Array) return (int)a.size();
        if (kind == Object) return (int)o.size();
        if (kind == String) return (int)s.size();
        return 0;
    }
    bool has(const std::string& k) const { return kind == Object && o.find(k) != o.end(); }
    __nexa_json get(const std::string& k) const {
        if (kind != Object) return nullv();
        auto it = o.find(k);
        return it == o.end() ? nullv() : it->second;
    }
    __nexa_json get_at(int i) const {
        if (kind != Array || i < 0 || i >= (int)a.size()) return nullv();
        return a[(size_t)i];
    }
    std::vector<std::string> keys() const {
        std::vector<std::string> ks;
        if (kind != Object) return ks;
        ks.reserve(o.size());
        for (const auto& kv : o) ks.push_back(kv.first);
        return ks;
    }
    void set(const std::string& k, __nexa_json v) {
        if (kind != Object) {
            kind = Object;
            b = false; n = 0; s.clear(); a.clear(); o.clear();
        }
        o[k] = std::move(v);
    }
    void push(__nexa_json v) {
        if (kind != Array) {
            kind = Array;
            b = false; n = 0; s.clear(); a.clear(); o.clear();
        }
        a.push_back(std::move(v));
    }
    void remove(const std::string& k) {
        if (kind == Object) o.erase(k);
    }

    __nexa_json& operator[](const std::string& k) {
        if (kind != Object) {
            kind = Object;
            b = false; n = 0; s.clear(); a.clear(); o.clear();
        }
        return o[k];
    }
    __nexa_json operator[](const std::string& k) const { return get(k); }
    __nexa_json& operator[](int i) {
        if (kind != Array) {
            kind = Array;
            b = false; n = 0; s.clear(); a.clear(); o.clear();
        }
        if (i < 0) {
            static __nexa_json dummy;
            dummy = nullv();
            return dummy;
        }
        if ((size_t)i >= a.size()) a.resize((size_t)i + 1);
        return a[(size_t)i];
    }
    __nexa_json operator[](int i) const { return get_at(i); }

    __nexa_json& operator=(bool v) { *this = boolean(v); return *this; }
    __nexa_json& operator=(int v) { *this = number((double)v); return *this; }
    __nexa_json& operator=(double v) { *this = number(v); return *this; }
    __nexa_json& operator=(const std::string& v) { *this = str(v); return *this; }
    __nexa_json& operator=(const char* v) { *this = str(v ? v : ""); return *this; }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) out.push_back((char)cp);
        else if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    static void escape_string(std::string& out, const std::string& in) {
        out.push_back('"');
        for (unsigned char c : in) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                        out += buf;
                    } else {
                        out.push_back((char)c);
                    }
            }
        }
        out.push_back('"');
    }

    void stringify_into(std::string& out, int indent, int depth) const {
        auto nl = [&]() {
            if (indent <= 0) return;
            out.push_back('\n');
            out.append((size_t)(indent * depth), ' ');
        };
        switch (kind) {
            case Null: out += "null"; break;
            case Bool: out += b ? "true" : "false"; break;
            case Number: {
                if (!std::isfinite(n)) { out += "null"; break; }
                char buf[64];
                if (n == (double)(long long)n && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
                    std::snprintf(buf, sizeof(buf), "%.0f", n);
                } else {
                    std::snprintf(buf, sizeof(buf), "%.17g", n);
                }
                out += buf;
                break;
            }
            case String: escape_string(out, s); break;
            case Error: out += "null"; break;
            case Array: {
                out.push_back('[');
                for (size_t i = 0; i < a.size(); i++) {
                    if (i) out.push_back(',');
                    if (indent > 0) nl();
                    a[i].stringify_into(out, indent, depth + 1);
                }
                if (indent > 0 && !a.empty()) {
                    out.push_back('\n');
                    out.append((size_t)(indent * (depth > 0 ? depth - 1 : 0)), ' ');
                }
                out.push_back(']');
                break;
            }
            case Object: {
                out.push_back('{');
                size_t i = 0;
                for (const auto& kv : o) {
                    if (i++) out.push_back(',');
                    if (indent > 0) nl();
                    escape_string(out, kv.first);
                    out.push_back(':');
                    if (indent > 0) out.push_back(' ');
                    kv.second.stringify_into(out, indent, depth + 1);
                }
                if (indent > 0 && !o.empty()) {
                    out.push_back('\n');
                    out.append((size_t)(indent * (depth > 0 ? depth - 1 : 0)), ' ');
                }
                out.push_back('}');
                break;
            }
        }
    }

    std::string stringify(int indent = 0) const {
        std::string out;
        stringify_into(out, indent < 0 ? 0 : indent, 1);
        return out;
    }
};

struct __nexa_json_parser {
    const std::string& src;
    size_t i = 0;
    int depth = 0;
    std::string err;

    explicit __nexa_json_parser(const std::string& s) : src(s) {}

    bool fail(const std::string& m) {
        if (err.empty()) err = m;
        return false;
    }
    void skip() {
        while (i < src.size() && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r')) i++;
    }
    bool peek(char c) { skip(); return i < src.size() && src[i] == c; }
    bool take(char c) {
        skip();
        if (i < src.size() && src[i] == c) { i++; return true; }
        return false;
    }
    bool parse_hex4(unsigned& out) {
        out = 0;
        for (int k = 0; k < 4; k++) {
            if (i >= src.size()) return fail("truncated \\u escape");
            char c = src[i++];
            unsigned v;
            if (c >= '0' && c <= '9') v = (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v = (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = (unsigned)(c - 'A' + 10);
            else return fail("invalid \\u escape");
            out = (out << 4) | v;
        }
        return true;
    }
    bool parse_string(std::string& out) {
        if (!take('"')) return fail("expected string");
        out.clear();
        while (i < src.size()) {
            unsigned char c = (unsigned char)src[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= src.size()) return fail("truncated string escape");
                char e = src[i++];
                switch (e) {
                    case '"': case '\\': case '/': out.push_back(e); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!parse_hex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i + 1 < src.size() && src[i] == '\\' && src[i + 1] == 'u') {
                                i += 2;
                                unsigned lo = 0;
                                if (!parse_hex4(lo)) return false;
                                if (lo < 0xDC00 || lo > 0xDFFF) return fail("invalid surrogate pair");
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            return fail("lone surrogate");
                        }
                        __nexa_json::append_utf8(out, cp);
                        break;
                    }
                    default: return fail("invalid string escape");
                }
            } else if (c < 0x20) {
                return fail("unescaped control character in string");
            } else {
                out.push_back((char)c);
            }
        }
        return fail("unterminated string");
    }
    bool parse_number(__nexa_json& out) {
        skip();
        size_t start = i;
        if (i < src.size() && src[i] == '-') i++;
        if (i >= src.size() || !std::isdigit((unsigned char)src[i])) return fail("invalid number");
        if (src[i] == '0') {
            i++;
            if (i < src.size() && std::isdigit((unsigned char)src[i])) return fail("leading zero");
        } else {
            while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
        }
        if (i < src.size() && src[i] == '.') {
            i++;
            if (i >= src.size() || !std::isdigit((unsigned char)src[i])) return fail("invalid number");
            while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
        }
        if (i < src.size() && (src[i] == 'e' || src[i] == 'E')) {
            i++;
            if (i < src.size() && (src[i] == '+' || src[i] == '-')) i++;
            if (i >= src.size() || !std::isdigit((unsigned char)src[i])) return fail("invalid exponent");
            while (i < src.size() && std::isdigit((unsigned char)src[i])) i++;
        }
        char* end = nullptr;
        std::string slice = src.substr(start, i - start);
        double v = std::strtod(slice.c_str(), &end);
        if (!end || *end) return fail("invalid number");
        out = __nexa_json::number(v);
        return true;
    }
    bool parse_value(__nexa_json& out) {
        if (depth > 64) return fail("nesting too deep");
        skip();
        if (i >= src.size()) return fail("unexpected end of JSON");
        if (src[i] == 'n') {
            if (src.compare(i, 4, "null") == 0) { i += 4; out = __nexa_json::nullv(); return true; }
            return fail("expected null");
        }
        if (src[i] == 't') {
            if (src.compare(i, 4, "true") == 0) { i += 4; out = __nexa_json::boolean(true); return true; }
            return fail("expected true");
        }
        if (src[i] == 'f') {
            if (src.compare(i, 5, "false") == 0) { i += 5; out = __nexa_json::boolean(false); return true; }
            return fail("expected false");
        }
        if (src[i] == '"') {
            std::string s;
            if (!parse_string(s)) return false;
            out = __nexa_json::str(s);
            return true;
        }
        if (src[i] == '-' || std::isdigit((unsigned char)src[i])) return parse_number(out);
        if (src[i] == '[') {
            i++;
            depth++;
            out = __nexa_json::arr();
            skip();
            if (take(']')) { depth--; return true; }
            for (;;) {
                __nexa_json el;
                if (!parse_value(el)) return false;
                out.a.push_back(std::move(el));
                if (take(']')) { depth--; return true; }
                if (!take(',')) return fail("expected ',' or ']' in array");
            }
        }
        if (src[i] == '{') {
            i++;
            depth++;
            out = __nexa_json::obj();
            skip();
            if (take('}')) { depth--; return true; }
            for (;;) {
                std::string key;
                if (!parse_string(key)) return false;
                if (!take(':')) return fail("expected ':' after object key");
                __nexa_json val;
                if (!parse_value(val)) return false;
                out.o[key] = std::move(val);
                if (take('}')) { depth--; return true; }
                if (!take(',')) return fail("expected ',' or '}' in object");
            }
        }
        return fail("unexpected character");
    }
};

static __nexa_json __nexa_json_parse(const std::string& s) {
    __nexa_json_parser p(s);
    __nexa_json v;
    if (!p.parse_value(v)) return __nexa_json::err(p.err.empty() ? "invalid JSON" : p.err);
    p.skip();
    if (p.i != s.size()) return __nexa_json::err("trailing data after JSON value");
    return v;
}

inline __nexa_json __nexa_json_from(const __nexa_json& v) { return v; }
inline __nexa_json __nexa_json_from(bool v) { return __nexa_json::boolean(v); }
inline __nexa_json __nexa_json_from(int v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(unsigned int v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(short v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(unsigned short v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(long v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(unsigned long v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(std::size_t v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(double v) { return __nexa_json::number(v); }
inline __nexa_json __nexa_json_from(float v) { return __nexa_json::number((double)v); }
inline __nexa_json __nexa_json_from(char v) { return __nexa_json::number((double)(unsigned char)v); }
inline __nexa_json __nexa_json_from(const std::string& v) { return __nexa_json::str(v); }
inline __nexa_json __nexa_json_from(const char* v) { return __nexa_json::str(v ? v : ""); }

template<class T>
inline __nexa_json __nexa_json_from(const std::vector<T>& v) {
    __nexa_json a = __nexa_json::arr();
    for (const auto& x : v) a.push(__nexa_json_from(x));
    return a;
}
template<class T>
inline __nexa_json __nexa_json_from(const std::map<std::string, T>& v) {
    __nexa_json o = __nexa_json::obj();
    for (const auto& kv : v) o.set(kv.first, __nexa_json_from(kv.second));
    return o;
}
)NEXA_JSON";
}

}  // namespace nexa
