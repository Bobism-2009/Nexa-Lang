#pragma once
#include <string>
#include <vector>

inline double half(int a) { return a / 2.0; }
inline std::string word() { return "word"; }
inline const char* cword() { return "cword"; }
inline const char* lookup(int i) { return i == 0 ? "zero" : nullptr; }
inline std::string join_words(const std::vector<std::string>& v) {
    std::string out;
    for (const std::string& s : v) out += (out.empty() ? "" : "+") + s;
    return out;
}
namespace cfg { constexpr int limit = 42; enum class Color { Red = 3, Green = 7 }; }
inline int code(cfg::Color c) { return static_cast<int>(c); }
