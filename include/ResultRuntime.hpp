#pragma once

#include <string>

namespace nexa {

inline std::string resultRuntimeCpp() {
    return R"NEXA_RESULT(
#include <string>
#include <utility>
#include <stdexcept>

template<typename T>
struct __nexa_result {
    bool _ok = false;
    T _value{};
    std::string _error;

    bool ok() const { return _ok; }
    const T& value() const {
        if (!_ok) throw std::runtime_error(_error.empty() ? "Result.value() on error" : _error);
        return _value;
    }
    T& value() {
        if (!_ok) throw std::runtime_error(_error.empty() ? "Result.value() on error" : _error);
        return _value;
    }
    const std::string& error() const { return _error; }

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
        if (!_ok) throw std::runtime_error(_error.empty() ? "Result.value() on error" : _error);
    }
    const std::string& error() const { return _error; }

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
