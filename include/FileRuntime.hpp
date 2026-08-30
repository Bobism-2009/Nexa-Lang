#pragma once

#include <string>

namespace nexa {

// write/append — C stdio only, no std::string.
inline std::string fileWriteRuntimeCpp() {
    return R"NEXA_FILE_WR(
static void __nexa_file_write(const char* __path, const char* __data, size_t __n, int __append) {
  FILE* __f = std::fopen(__path, __append ? "a" : "w");
  if (!__f) return;
  if (__n && __data) std::fwrite(__data, 1, __n, __f);
  std::fclose(__f);
}
)NEXA_FILE_WR";
}

// read — C stdio + std::string.
inline std::string fileReadRuntimeCpp() {
    return R"NEXA_FILE_RD(
static std::string __nexa_file_read(const char* __path) {
  FILE* __f = std::fopen(__path, "r");
  if (!__f) return std::string();
  std::string __out;
  char __buf[4096];
  size_t __n;
  while ((__n = std::fread(__buf, 1, sizeof(__buf), __f)) > 0) __out.append(__buf, __n);
  std::fclose(__f);
  return __out;
}
)NEXA_FILE_RD";
}

// Path/list/mkdir helpers — Win32 / POSIX, no <filesystem>.
inline std::string fileRuntimeCpp() {
    return R"NEXA_FILE_FS(
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#endif
#include <vector>
#include <string>
#include <cstdio>
static int __nexa_file_is_sep(char __c) {
#ifdef _WIN32
  return __c == '/' || __c == '\\';
#else
  return __c == '/';
#endif
}
static int __nexa_file_exists(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  return GetFileAttributesA(__path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
  struct stat __st;
  return stat(__path, &__st) == 0 ? 1 : 0;
#endif
}
static int __nexa_file_isdir(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  DWORD __a = GetFileAttributesA(__path);
  return (__a != INVALID_FILE_ATTRIBUTES && (__a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
  struct stat __st;
  if (stat(__path, &__st) != 0) return 0;
  return S_ISDIR(__st.st_mode) ? 1 : 0;
#endif
}
static int __nexa_file_isfile(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  DWORD __a = GetFileAttributesA(__path);
  return (__a != INVALID_FILE_ATTRIBUTES && !(__a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
  struct stat __st;
  if (stat(__path, &__st) != 0) return 0;
  return S_ISREG(__st.st_mode) ? 1 : 0;
#endif
}
static int __nexa_file_size(const char* __path) {
  if (!__path || !__path[0]) return -1;
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA __d;
  if (!GetFileAttributesExA(__path, GetFileExInfoStandard, &__d)) return -1;
  if (__d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return -1;
  ULARGE_INTEGER __u;
  __u.LowPart = __d.nFileSizeLow;
  __u.HighPart = __d.nFileSizeHigh;
  if (__u.QuadPart > 0x7FFFFFFF) return 0x7FFFFFFF;
  return static_cast<int>(__u.QuadPart);
#else
  struct stat __st;
  if (stat(__path, &__st) != 0 || !S_ISREG(__st.st_mode)) return -1;
  if (__st.st_size > 0x7FFFFFFF) return 0x7FFFFFFF;
  return static_cast<int>(__st.st_size);
#endif
}
static int __nexa_file_mkdir(const char* __path) {
  if (!__path || !__path[0]) return 0;
  if (__nexa_file_isdir(__path)) return 1;
  char __buf[4096];
  size_t __n = 0;
  size_t __i = 0;
#ifdef _WIN32
  if (((__path[0] >= 'A' && __path[0] <= 'Z') || (__path[0] >= 'a' && __path[0] <= 'z')) && __path[1] == ':') {
    __buf[__n++] = __path[0];
    __buf[__n++] = __path[1];
    __i = 2;
  }
  if (__nexa_file_is_sep(__path[__i])) { __buf[__n++] = __path[__i]; __i++; }
#else
  if (__path[0] == '/') { __buf[__n++] = '/'; __i = 1; }
#endif
  while (__path[__i] && __n + 2 < sizeof(__buf)) {
    while (__nexa_file_is_sep(__path[__i])) __i++;
    if (!__path[__i]) break;
    if (__n && !__nexa_file_is_sep(__buf[__n - 1])) {
#ifdef _WIN32
      __buf[__n++] = '\\';
#else
      __buf[__n++] = '/';
#endif
    }
    while (__path[__i] && !__nexa_file_is_sep(__path[__i]) && __n + 1 < sizeof(__buf)) __buf[__n++] = __path[__i++];
    __buf[__n] = 0;
#ifdef _WIN32
    if (!CreateDirectoryA(__buf, NULL)) {
      DWORD __e = GetLastError();
      if (__e != ERROR_ALREADY_EXISTS) return 0;
    }
#else
    if (mkdir(__buf, 0755) != 0 && errno != EEXIST) return 0;
#endif
  }
  return __nexa_file_isdir(__path) ? 1 : 0;
}
static int __nexa_file_path_abs(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  if (__path[0] && __path[1] == ':' && __nexa_file_is_sep(__path[2])) return 1;
  if (__nexa_file_is_sep(__path[0]) && __nexa_file_is_sep(__path[1])) return 1;
  return 0;
#else
  return __path[0] == '/' ? 1 : 0;
#endif
}
static std::string __nexa_file_join(const char* __a, const char* __b) {
  if (!__b) __b = "";
  if (!__a) __a = "";
  if (!__b[0]) return std::string(__a);
  if (__nexa_file_path_abs(__b)) return std::string(__b);
  if (!__a[0]) return std::string(__b);
  std::string __o(__a);
  if (!__nexa_file_is_sep(__o.back())) {
#ifdef _WIN32
    __o += '\\';
#else
    __o += '/';
#endif
  }
  __o += __b;
  return __o;
}
static std::string __nexa_file_basename(const char* __path) {
  if (!__path || !__path[0]) return std::string();
  size_t __n = 0;
  while (__path[__n]) __n++;
  while (__n > 0 && __nexa_file_is_sep(__path[__n - 1])) __n--;
  if (__n == 0) return std::string();
  size_t __i = __n;
  while (__i > 0 && !__nexa_file_is_sep(__path[__i - 1])) __i--;
  return std::string(__path + __i, __n - __i);
}
static std::string __nexa_file_dirname(const char* __path) {
  if (!__path || !__path[0]) return std::string();
  size_t __n = 0;
  while (__path[__n]) __n++;
  while (__n > 0 && __nexa_file_is_sep(__path[__n - 1])) __n--;
  while (__n > 0 && !__nexa_file_is_sep(__path[__n - 1])) __n--;
  while (__n > 0 && __nexa_file_is_sep(__path[__n - 1])) __n--;
  if (__n == 0) {
#ifdef _WIN32
    if (__path[0] && __path[1] == ':') return std::string(__path, 2);
#endif
    return (__path[0] && __nexa_file_is_sep(__path[0])) ? std::string(__path, 1) : std::string();
  }
  return std::string(__path, __n);
}
static std::string __nexa_file_extension(const char* __path) {
  std::string __b = __nexa_file_basename(__path);
  if (__b.empty()) return std::string();
  size_t __d = __b.rfind('.');
  if (__d == std::string::npos || __d == 0) return std::string();
  return __b.substr(__d);
}
static std::string __nexa_file_cwd() {
#ifdef _WIN32
  DWORD __n = GetCurrentDirectoryA(0, NULL);
  if (!__n) return std::string();
  std::string __s(__n, 0);
  DWORD __m = GetCurrentDirectoryA(__n, &__s[0]);
  if (!__m) return std::string();
  if (__m < __n) __s.resize(__m);
  return __s;
#else
  char __buf[4096];
  if (!getcwd(__buf, sizeof(__buf))) return std::string();
  return std::string(__buf);
#endif
}
static int __nexa_file_chdir(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  return SetCurrentDirectoryA(__path) ? 1 : 0;
#else
  return chdir(__path) == 0 ? 1 : 0;
#endif
}
static std::string __nexa_file_abspath(const char* __path) {
  if (!__path) __path = "";
#ifdef _WIN32
  DWORD __n = GetFullPathNameA(__path, 0, NULL, NULL);
  if (!__n) return std::string(__path);
  std::string __s(__n, 0);
  DWORD __m = GetFullPathNameA(__path, __n, &__s[0], NULL);
  if (!__m) return std::string(__path);
  if (__m < __n) __s.resize(__m);
  return __s;
#else
  std::string __in = __nexa_file_path_abs(__path) ? std::string(__path) : __nexa_file_join(__nexa_file_cwd().c_str(), __path);
  std::string __out;
  if (!__in.empty() && __in[0] == '/') __out += '/';
  size_t __i = 0;
  if (!__in.empty() && __in[0] == '/') __i = 1;
  while (__i < __in.size()) {
    while (__i < __in.size() && __nexa_file_is_sep(__in[__i])) __i++;
    if (__i >= __in.size()) break;
    size_t __j = __i;
    while (__j < __in.size() && !__nexa_file_is_sep(__in[__j])) __j++;
    std::string __part = __in.substr(__i, __j - __i);
    __i = __j;
    if (__part == ".") continue;
    if (__part == "..") {
      if (__out.size() > 1) {
        while (__out.size() > 1 && __out.back() != '/') __out.pop_back();
        if (__out.size() > 1) __out.pop_back();
      }
      continue;
    }
    if (__out.empty() || __out.back() != '/') {
      if (!__out.empty()) __out += '/';
    }
    __out += __part;
  }
  if (__out.empty()) __out = "/";
  return __out;
#endif
}
static std::vector<std::string> __nexa_file_list(const char* __path) {
  std::vector<std::string> __out;
  if (!__path || !__nexa_file_isdir(__path)) return __out;
#ifdef _WIN32
  std::string __pat = __nexa_file_join(__path, "*");
  WIN32_FIND_DATAA __fd;
  HANDLE __h = FindFirstFileA(__pat.c_str(), &__fd);
  if (__h == INVALID_HANDLE_VALUE) return __out;
  do {
    const char* __n = __fd.cFileName;
    if (__n[0] == '.' && (__n[1] == 0 || (__n[1] == '.' && __n[2] == 0))) continue;
    __out.push_back(__n);
  } while (FindNextFileA(__h, &__fd));
  FindClose(__h);
#else
  DIR* __d = opendir(__path);
  if (!__d) return __out;
  struct dirent* __e;
  while ((__e = readdir(__d))) {
    const char* __n = __e->d_name;
    if (__n[0] == '.' && (__n[1] == 0 || (__n[1] == '.' && __n[2] == 0))) continue;
    __out.push_back(__n);
  }
  closedir(__d);
#endif
  return __out;
}
static int __nexa_file_remove(const char* __path) {
  if (!__path || !__path[0]) return 0;
#ifdef _WIN32
  if (__nexa_file_isdir(__path)) return RemoveDirectoryA(__path) ? 1 : 0;
  return DeleteFileA(__path) ? 1 : 0;
#else
  if (__nexa_file_isdir(__path)) return rmdir(__path) == 0 ? 1 : 0;
  return unlink(__path) == 0 ? 1 : 0;
#endif
}
static int __nexa_file_remove_all(const char* __path) {
  if (!__path || !__path[0]) return 0;
  if (!__nexa_file_exists(__path)) return 1;
  if (__nexa_file_isdir(__path)) {
    std::vector<std::string> __names = __nexa_file_list(__path);
    for (size_t __i = 0; __i < __names.size(); ++__i) {
      std::string __c = __nexa_file_join(__path, __names[__i].c_str());
      if (!__nexa_file_remove_all(__c.c_str())) return 0;
    }
  }
  return __nexa_file_remove(__path);
}
static int __nexa_file_copy_file(const char* __from, const char* __to) {
#ifdef _WIN32
  return CopyFileA(__from, __to, FALSE) ? 1 : 0;
#else
  FILE* __in = std::fopen(__from, "rb");
  if (!__in) return 0;
  FILE* __out = std::fopen(__to, "wb");
  if (!__out) { std::fclose(__in); return 0; }
  char __buf[4096];
  size_t __n;
  int __ok = 1;
  while ((__n = std::fread(__buf, 1, sizeof(__buf), __in)) > 0) {
    if (std::fwrite(__buf, 1, __n, __out) != __n) { __ok = 0; break; }
  }
  std::fclose(__in);
  std::fclose(__out);
  return __ok;
#endif
}
static int __nexa_file_copy(const char* __from, const char* __to) {
  if (!__from || !__to || !__nexa_file_exists(__from)) return 0;
  if (__nexa_file_isdir(__from)) {
    if (!__nexa_file_mkdir(__to)) return 0;
    std::vector<std::string> __names = __nexa_file_list(__from);
    for (size_t __i = 0; __i < __names.size(); ++__i) {
      std::string __a = __nexa_file_join(__from, __names[__i].c_str());
      std::string __b = __nexa_file_join(__to, __names[__i].c_str());
      if (!__nexa_file_copy(__a.c_str(), __b.c_str())) return 0;
    }
    return 1;
  }
  return __nexa_file_copy_file(__from, __to);
}
static int __nexa_file_rename(const char* __from, const char* __to) {
  if (!__from || !__to) return 0;
#ifdef _WIN32
  return MoveFileA(__from, __to) ? 1 : 0;
#else
  return rename(__from, __to) == 0 ? 1 : 0;
#endif
}
)NEXA_FILE_FS";
}

}  // namespace nexa
