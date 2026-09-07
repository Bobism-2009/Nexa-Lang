#pragma once

namespace nexa_cpp {
inline int mul(int a, int b) {
    return a * b;
}

struct Box {
    int v;
    int get() const { return v; }
    void set(int x) { v = x; }
};

inline Box make_box(int v) {
    Box b;
    b.v = v;
    return b;
}
}

inline int cpp_add(int a, int b) {
    return a + b;
}
