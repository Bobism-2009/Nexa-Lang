#pragma once

#include <string>

namespace nexa {

inline std::string resultRuntimeCpp() {
    return R"NEXA_RESULT(
#include <string>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// .value() on an error: an exception a try/catch can catch when the program has one;
// otherwise there is nothing to catch it, so say what the error was and stop, which is
// what an uncaught exception comes to -- without linking the exception runtime for it.
[[noreturn]] inline void __nexa_result_fail(const std::string& __e) {
    const char* __m = __e.empty() ? "Result.value() on error" : __e.c_str();
#if defined(__cpp_exceptions)
    throw std::runtime_error(__m);
#else
    std::fflush(stdout);  // what the program printed before this still reaches its reader
    std::fprintf(stderr, "Result.value() on an error: %s\n", __m);
    std::abort();
#endif
}

template<typename T>
struct __nexa_result {
    bool _ok = false;
    T _value{};
    std::string _error;

    bool ok() const { return _ok; }
    const T& value() const& {
        if (!_ok) __nexa_result_fail(_error);
        return _value;
    }
    T& value() & {
        if (!_ok) __nexa_result_fail(_error);
        return _value;
    }
    // f().value(): the Result is a temporary, so the value is moved out of it. A
    // reference into it would dangle by the time `for (x in f().value())` ran.
    T value() && {
        if (!_ok) __nexa_result_fail(_error);
        return std::move(_value);
    }
    const std::string& error() const { return _error; }

    // A struct with a Result field compares its fields, so a Result compares too:
    // both ok with equal values, or both errors with the same text.
    friend bool operator==(const __nexa_result& __a, const __nexa_result& __b) {
        return __a._ok == __b._ok && (__a._ok ? bool(__a._value == __b._value) : __a._error == __b._error);
    }
    friend bool operator!=(const __nexa_result& __a, const __nexa_result& __b) { return !(__a == __b); }

    static __nexa_result make_ok(T v) {
        __nexa_result r;
        r._ok = true;
        r._value = std::move(v);
        return r;
    }
    static __nexa_result make_err(std::string e) {
        __nexa_result r;
        r._ok = false;
        r._error = std::move(e);
        return r;
    }
};

template<>
struct __nexa_result<void> {
    bool _ok = false;
    std::string _error;

    bool ok() const { return _ok; }
    void value() const {
        if (!_ok) __nexa_result_fail(_error);
    }
    const std::string& error() const { return _error; }

    friend bool operator==(const __nexa_result& __a, const __nexa_result& __b) {
        return __a._ok == __b._ok && __a._error == __b._error;
    }
    friend bool operator!=(const __nexa_result& __a, const __nexa_result& __b) { return !(__a == __b); }

    static __nexa_result make_ok() {
        __nexa_result r;
        r._ok = true;
        return r;
    }
    static __nexa_result make_err(std::string e) {
        __nexa_result r;
        r._ok = false;
        r._error = std::move(e);
        return r;
    }
};

struct __nexa_result_err {
    std::string msg;
    explicit __nexa_result_err(std::string m) : msg(std::move(m)) {}
    template<typename T>
    operator __nexa_result<T>() const {
        return __nexa_result<T>::make_err(msg);
    }
};
)NEXA_RESULT";
}

}  // namespace nexa
