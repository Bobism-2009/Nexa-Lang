#pragma once
#include <string>
#include <cstddef>

// "Did you mean" for error messages.
//
// Nothing here runs unless NexaC is already building an error message: a
// program that compiles never reaches it, so it adds no compile time and
// cannot change the generated C++ or the binary.
//
//     NexaSuggest s(name);
//     for (...) s.consider(candidate);
//     throw std::runtime_error("Unknown thing '" + name + "'" + s.text() + ...);
//
// text() is "" when nothing is close, or " (did you mean 'x'?)".
class NexaSuggest {
public:
    explicit NexaSuggest(const std::string& name, const std::string& prefix = std::string())
        : name_(name), prefix_(prefix) {
        // How far off a name may be and still count: a 1-3 letter name only by
        // case or one swapped pair, then one edit per three letters, at most 3.
        const size_t n = name.size();
        limit_ = n <= 3 ? 1 : (n / 3 < 3 ? n / 3 : 3);
        if (n <= 2) limit_ = 0;
    }

    void consider(const std::string& cand) {
        if (cand.empty() || cand == name_) return;
        if (cand.size() >= 2 && cand[0] == '_' && cand[1] == '_') return;  // NexaC's own
        const size_t a = name_.size(), b = cand.size();
        const size_t lenGap = a > b ? a - b : b - a;
        const size_t cap = limit_ < best_ ? limit_ : best_;
        if (lenGap > cap) return;
        const size_t d = distance(name_, cand, cap, true);
        if (d > cap) return;
        // Ties go to the candidate that also matches letter case, then to the
        // first one seen, so the answer does not depend on anything but input.
        const size_t exact = distance(name_, cand, d + 3, false);
        if (d < best_ || exact < bestExact_) {
            best_ = d;
            bestExact_ = exact;
            pick_ = cand;
        }
    }

    // A comma-separated list ("open, close, poll") as written in "(use ...)".
    void considerList(const char* list) {
        std::string cur;
        for (const char* p = list; ; ++p) {
            if (*p == ',' || *p == '\0') {
                if (!cur.empty()) consider(cur);
                cur.clear();
                if (*p == '\0') break;
            } else if (*p != ' ') {
                cur.push_back(*p);
            }
        }
    }

    bool found() const { return !pick_.empty(); }
    const std::string& pick() const { return pick_; }

    std::string text() const {
        if (pick_.empty()) return std::string();
        return std::string(" (did you mean '") + prefix_ + pick_ + "'?)";
    }

private:
    static char fold(char c, bool ci) {
        return (ci && c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }

    // Optimal string alignment distance (Levenshtein plus swapping two
    // neighbours), stopping once every path is over `cap`. Case-insensitive
    // when ci, where a name that differs only in case scores 0.
    static size_t distance(const std::string& s, const std::string& t, size_t cap, bool ci) {
        const size_t n = s.size(), m = t.size();
        if (n > 64 || m > 64) return cap + 1;
        size_t prev2[65], prev[65], cur[65];
        for (size_t j = 0; j <= m; ++j) prev[j] = j;
        for (size_t i = 1; i <= n; ++i) {
            cur[0] = i;
            size_t rowMin = cur[0];
            const char si = fold(s[i - 1], ci);
            for (size_t j = 1; j <= m; ++j) {
                const char tj = fold(t[j - 1], ci);
                size_t v = prev[j - 1] + (si == tj ? 0 : 1);
                if (prev[j] + 1 < v) v = prev[j] + 1;
                if (cur[j - 1] + 1 < v) v = cur[j - 1] + 1;
                if (i > 1 && j > 1 && si == fold(t[j - 2], ci) && fold(s[i - 2], ci) == tj &&
                    prev2[j - 2] + 1 < v) {
                    v = prev2[j - 2] + 1;
                }
                cur[j] = v;
                if (v < rowMin) rowMin = v;
            }
            if (rowMin > cap) return cap + 1;
            for (size_t j = 0; j <= m; ++j) { prev2[j] = prev[j]; prev[j] = cur[j]; }
        }
        return prev[m];
    }

    std::string name_;
    std::string prefix_;
    std::string pick_;
    size_t limit_ = 0;
    size_t best_ = static_cast<size_t>(-1);
    size_t bestExact_ = static_cast<size_t>(-1);
};

// Puts the suggestion just after `quoted` (as in 'math.sqr') in an error
// message, or leaves the message as it is when nothing is close.
inline std::string nexaWithSuggestion(const std::string& msg, const std::string& quoted, const NexaSuggest& s) {
    if (!s.found()) return msg;
    const size_t at = msg.find(quoted);
    if (at == std::string::npos) return msg + s.text();
    return msg.substr(0, at + quoted.size()) + s.text() + msg.substr(at + quoted.size());
}

// For "Unknown math function 'math.sqr' at line 3 (use abs, sqrt, ...)": the
// message already lists every name, so the suggestion comes from that list.
inline std::string nexaSuggestFromUse(const std::string& msg, const std::string& name, const std::string& prefix) {
    const size_t use = msg.rfind("(use ");
    if (use == std::string::npos) return msg;
    const size_t end = msg.find(')', use);
    const std::string list = msg.substr(use + 5, end == std::string::npos ? std::string::npos : end - use - 5);
    NexaSuggest s(name, prefix);
    s.considerList(list.c_str());
    return nexaWithSuggestion(msg, "'" + prefix + name + "'", s);
}
