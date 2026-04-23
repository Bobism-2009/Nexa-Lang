#include <cstdio>
#include <thread>
#include <vector>
static std::vector<std::thread> __nexa_threads;
static int __nexa_thread_spawn(void (*fn)()) {
  __nexa_threads.emplace_back(fn);
  return static_cast<int>(__nexa_threads.size()) - 1;
}
static void __nexa_thread_join(int idx) {
  if (idx >= 0 && static_cast<size_t>(idx) < __nexa_threads.size() && __nexa_threads[idx].joinable()) {
    __nexa_threads[idx].join();
  }
}
#include <string>

static void __nexa_fn_0() {
    printf("%s\n", std::string("worker").c_str());
}

int main() {
    int __nexa_var_0 = __nexa_thread_spawn(&__nexa_fn_0);
    __nexa_thread_join(__nexa_var_0);
    return 0;
}
