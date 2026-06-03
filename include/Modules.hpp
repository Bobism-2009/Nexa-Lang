#pragma once

#include <string>
#include <set>
#include <sstream>

namespace nexa {

// Tracks which standard library modules are enabled by #include directives
class Modules {
public:
    struct CppUsage {
        bool ioPrint = false;
        bool ioFlush = false;
        bool ioReadln = false;
        bool ioGetline = false;
        bool ioToInt = false;
        bool osSystem = false;
        bool osGetenv = false;
        bool osPlatform = false;
        bool osExeDir = false;
        bool osGetProcessId = false;
        bool osWindowControl = false;
        bool osMessageBox = false;
        bool osGrepKeys = false;
        bool osKeyPressed = false;
        bool file = false;
        bool random = false;
        bool math = false;
        bool time = false;
        bool thread = false;
        bool dll = false;
        bool exceptions = false;
    };

    void enable(const std::string& path) {
        enabled_.insert(path);
    }

    bool hasIo() const {
        return enabled_.count("std/io") > 0;
    }

    bool hasOs() const {
        return enabled_.count("std/os") > 0;
    }

    bool hasDll() const {
        return enabled_.count("std/dll") > 0;
    }

    bool hasFile() const {
        return enabled_.count("std/file") > 0;
    }

    bool hasRandom() const {
        return enabled_.count("std/random") > 0;
    }

    bool hasMath() const {
        return enabled_.count("std/math") > 0;
    }

    bool hasTime() const {
        return enabled_.count("std/time") > 0;
    }

    bool hasInlineCpp() const {
        return enabled_.count("std/inline") > 0;
    }

    bool hasThread() const {
        return enabled_.count("std/thread") > 0;
    }

    std::string getCppIncludes(const CppUsage& usage) const {
        std::string out;
        if (usage.exceptions) {
            out += "#include <stdexcept>\n";
        }
        if (hasIo() && (usage.ioPrint || usage.ioReadln || usage.ioFlush)) {
            out += "#include <cstdio>\n";
        }
        if (hasIo() && usage.ioReadln) {
            out += "#include <cstring>\n";
            out += "#include <string>\n";
        }
        if (hasIo() && usage.ioGetline) {
            out += "#include <string>\n";
            out += "static std::string __nexa_io_getline(const std::string& src, int lineNo) {\n";
            out += "  if (lineNo < 1) return \"\";\n";
            out += "  size_t start = 0;\n";
            out += "  int cur = 1;\n";
            out += "  while (cur < lineNo) {\n";
            out += "    size_t nl = src.find('\\n', start);\n";
            out += "    if (nl == std::string::npos) return \"\";\n";
            out += "    start = nl + 1;\n";
            out += "    cur++;\n";
            out += "  }\n";
            out += "  size_t end = src.find('\\n', start);\n";
            out += "  std::string line = (end == std::string::npos) ? src.substr(start) : src.substr(start, end - start);\n";
            out += "  if (!line.empty() && line.back() == '\\r') line.pop_back();\n";
            out += "  return line;\n";
            out += "}\n";
            out += "static std::string __nexa_io_getline_by_key(const std::string& src, const std::string& key) {\n";
            out += "  if (key.empty()) return \"\";\n";
            out += "  size_t pos = 0;\n";
            out += "  while (pos <= src.size()) {\n";
            out += "    size_t end = src.find('\\n', pos);\n";
            out += "    std::string line = (end == std::string::npos) ? src.substr(pos) : src.substr(pos, end - pos);\n";
            out += "    if (!line.empty() && line.back() == '\\r') line.pop_back();\n";
            out += "    size_t i = 0;\n";
            out += "    while (i < line.size() && (line[i] == ' ' || line[i] == '\\t')) i++;\n";
            out += "    if (line.compare(i, key.size(), key) == 0) {\n";
            out += "      size_t j = i + key.size();\n";
            out += "      while (j < line.size() && (line[j] == ' ' || line[j] == '\\t')) j++;\n";
            out += "      if (j < line.size() && line[j] == ':') {\n";
            out += "        return line.substr(i);\n";
            out += "      }\n";
            out += "    }\n";
            out += "    if (end == std::string::npos) break;\n";
            out += "    pos = end + 1;\n";
            out += "  }\n";
            out += "  return \"\";\n";
            out += "}\n";
        }
        if (hasIo() && usage.ioToInt) {
            out += "#include <string>\n";
            out += "static int __nexa_to_int(const std::string& s) { try { return std::stoi(s); } catch(...) { return 0; } }\n";
        }
        if (hasOs() && (usage.osSystem || usage.osGetenv)) {
            out += "#include <cstdlib>\n";
        }
        if (hasOs() && usage.osSystem) {
            out += "#include <cstdio>\n";
        }
        if (hasOs() && (usage.osGetenv || usage.osPlatform || usage.osExeDir || usage.osMessageBox || usage.osGrepKeys)) {
            out += "#include <string>\n";
        }
        if (hasOs() && usage.osGetProcessId) {
            out += "#include <string>\n";
            out += "#ifdef _WIN32\n#include <windows.h>\n#include <tlhelp32.h>\n#else\n#include <unistd.h>\n#endif\n";
        }
        if (hasOs() && usage.osExeDir) {
            out += "#ifdef __linux__\n#include <unistd.h>\n#endif\n";
            out += "#ifdef __APPLE__\n#include <mach-o/dyld.h>\n#endif\n";
        }
        if (hasOs() && (usage.osWindowControl || usage.osMessageBox || usage.osExeDir)) {
            out += "#ifdef _WIN32\n#include <windows.h>\n#endif\n";
        }
        if (hasOs() && (usage.osGrepKeys || usage.osKeyPressed)) {
            out += "#ifdef _WIN32\n#include <conio.h>\n#endif\n";
        }
        if (hasOs() && usage.osPlatform) {
            out += "static std::string __nexa_os_platform() {\n";
            out += "#ifdef _WIN32\n";
            out += "  return \"windows\";\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  return \"darwin\";\n";
            out += "#elif defined(__linux__)\n";
            out += "  return \"linux\";\n";
            out += "#else\n";
            out += "  return \"unknown\";\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osGetProcessId) {
            out += "static std::string __nexa_ascii_lower(const std::string& s) {\n";
            out += "  std::string t = s;\n";
            out += "  for (size_t i = 0; i < t.size(); ++i) {\n";
            out += "    unsigned char c = static_cast<unsigned char>(t[i]);\n";
            out += "    if (c >= 'A' && c <= 'Z') t[i] = static_cast<char>(c - 'A' + 'a');\n";
            out += "  }\n";
            out += "  return t;\n";
            out += "}\n";
            out += "static int __nexa_os_getprocessid() {\n";
            out += "#ifdef _WIN32\n";
            out += "  return static_cast<int>(GetCurrentProcessId());\n";
            out += "#else\n";
            out += "  return static_cast<int>(getpid());\n";
            out += "#endif\n";
            out += "}\n";
            out += "static int __nexa_os_getprocessid_by_name(const std::string& name) {\n";
            out += "#ifdef _WIN32\n";
            out += "  std::string target = __nexa_ascii_lower(name);\n";
            out += "  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);\n";
            out += "  if (snap == INVALID_HANDLE_VALUE) return 0;\n";
            out += "  PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);\n";
            out += "  if (Process32First(snap, &pe)) {\n";
            out += "    do {\n";
            out += "#ifdef UNICODE\n";
            out += "      char exe[260];\n";
            out += "      int n = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exe, sizeof(exe), NULL, NULL);\n";
            out += "      std::string proc = (n > 0) ? std::string(exe) : std::string();\n";
            out += "#else\n";
            out += "      std::string proc = std::string(pe.szExeFile);\n";
            out += "#endif\n";
            out += "      if (__nexa_ascii_lower(proc) == target) {\n";
            out += "        CloseHandle(snap);\n";
            out += "        return static_cast<int>(pe.th32ProcessID);\n";
            out += "      }\n";
            out += "    } while (Process32Next(snap, &pe));\n";
            out += "  }\n";
            out += "  CloseHandle(snap);\n";
            out += "  return 0;\n";
            out += "#else\n";
            out += "  (void)name;\n";
            out += "  return 0;\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osExeDir) {
            out += "static std::string __nexa_exe_dir() {\n";
            out += "#ifdef __linux__\n";
            out += "  char buf[4096]; ssize_t n = readlink(\"/proc/self/exe\", buf, sizeof(buf)-1);\n";
            out += "  if (n > 0) { buf[n]=0; std::string s(buf); size_t p=s.find_last_of('/'); return p!=std::string::npos ? s.substr(0,p+1) : \"./\"; }\n";
            out += "#elif defined(_WIN32)\n";
            out += "  char buf[4096]; if (GetModuleFileNameA(NULL, buf, sizeof(buf))) {\n";
            out += "    std::string s(buf); size_t p=s.find_last_of(\"/\\\\\"); return p!=std::string::npos ? s.substr(0,p+1) : \".\\\\\"; }\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  char buf[4096]; uint32_t sz=sizeof(buf); if (_NSGetExecutablePath(buf,&sz)==0) {\n";
            out += "    std::string s(buf); size_t p=s.find_last_of('/'); return p!=std::string::npos ? s.substr(0,p+1) : \"./\"; }\n";
            out += "#endif\n";
            out += "  return \"./\";\n";
            out += "}\n";
        }
        if (hasOs() && usage.osWindowControl) {
            out += "static void __nexa_os_hide_console_window() {\n";
            out += "#ifdef _WIN32\n";
            out += "  HWND h = GetConsoleWindow(); if (h) ShowWindow(h, SW_HIDE);\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_show_console_window() {\n";
            out += "#ifdef _WIN32\n";
            out += "  HWND h = GetConsoleWindow(); if (h) ShowWindow(h, SW_SHOW);\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_minimize_console_window() {\n";
            out += "#ifdef _WIN32\n";
            out += "  HWND h = GetConsoleWindow(); if (h) ShowWindow(h, SW_MINIMIZE);\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_maximize_console_window() {\n";
            out += "#ifdef _WIN32\n";
            out += "  HWND h = GetConsoleWindow(); if (h) ShowWindow(h, SW_MAXIMIZE);\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osMessageBox) {
            out += "static void __nexa_os_messagebox(const std::string& text, const std::string& title) {\n";
            out += "#ifdef _WIN32\n";
            out += "  MessageBoxA(NULL, text.c_str(), title.c_str(), MB_OK);\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osGrepKeys) {
            out += "static std::string __nexa_os_grepkeys() {\n";
            out += "#ifdef _WIN32\n";
            out += "  int c = _getch();\n";
            out += "  if (c == 0 || c == 224) { int c2 = _getch();\n";
            out += "    if (c2 == 72) return \"Up\"; if (c2 == 80) return \"Down\";\n";
            out += "    if (c2 == 75) return \"Left\"; if (c2 == 77) return \"Right\";\n";
            out += "    if (c2 == 71) return \"Home\"; if (c2 == 79) return \"End\";\n";
            out += "    if (c2 == 73) return \"PageUp\"; if (c2 == 81) return \"PageDown\";\n";
            out += "    return std::string(\"#\") + std::to_string(c2); }\n";
            out += "  if (c == 13) return \"Enter\"; if (c == 27) return \"Escape\";\n";
            out += "  if (c == 9) return \"Tab\"; if (c == 8) return \"Backspace\";\n";
            out += "  if (c >= 32 && c < 127) return std::string(1, (char)c);\n";
            out += "  return std::string(\"#\") + std::to_string(c);\n";
            out += "#else\n";
            out += "  return \"\";\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osKeyPressed) {
            out += "static int __nexa_os_keypressed() {\n";
            out += "#ifdef _WIN32\n";
            out += "  return _kbhit() ? 1 : 0;\n";
            out += "#else\n";
            out += "  return 0;\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasFile() && usage.file) {
            out += "#include <fstream>\n";
            out += "#include <sstream>\n";
            out += "#include <filesystem>\n";
        }
        if (hasRandom() && usage.random) {
            out += "#include <random>\n";
            out += "static std::mt19937& __nexa_rng() { static std::mt19937 gen(std::random_device{}()); return gen; }\n";
            out += "static void __nexa_random_seed(int s) { __nexa_rng().seed(static_cast<unsigned>(s)); }\n";
            out += "static int __nexa_random_int(int a, int b) { return std::uniform_int_distribution<int>(a, b)(__nexa_rng()); }\n";
        }
        if (hasMath() && usage.math) {
            out += "#include <cmath>\n";
            out += "#include <cstdlib>\n";
            out += "#include <algorithm>\n";
        }
        if (hasTime() && usage.time) {
            out += "#include <thread>\n";
            out += "#include <chrono>\n";
            out += "static double __nexa_time_now_ms() {\n";
            out += "  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();\n";
            out += "}\n";
        }
        if (hasThread() && usage.thread) {
            out += "#include <thread>\n";
            out += "#include <vector>\n";
            out += "static std::vector<std::thread> __nexa_threads;\n";
            out += "static int __nexa_thread_spawn(void (*fn)()) {\n";
            out += "  __nexa_threads.emplace_back(fn);\n";
            out += "  return static_cast<int>(__nexa_threads.size()) - 1;\n";
            out += "}\n";
            out += "static void __nexa_thread_join(int idx) {\n";
            out += "  if (idx >= 0 && static_cast<size_t>(idx) < __nexa_threads.size() && __nexa_threads[idx].joinable()) {\n";
            out += "    __nexa_threads[idx].join();\n";
            out += "  }\n";
            out += "}\n";
        }
        if (hasDll() && usage.dll) {
            out += "#include <vector>\n";
#ifdef _WIN32
            out += "#include <windows.h>\n";
#else
            out += "#include <dlfcn.h>\n";
#endif
            out += "static std::vector<void*> __nexa_dll_handles;\n";
        }
        std::set<std::string> seenIncludeLines;
        std::ostringstream filtered;
        std::istringstream in(out);
        std::string line;
        while (std::getline(in, line)) {
            std::string key = line;
            while (!key.empty() && key.back() == '\r') key.pop_back();
            if (key.rfind("#include <", 0) == 0) {
                if (!seenIncludeLines.insert(key).second) continue;
            }
            filtered << key << "\n";
        }
        return filtered.str();
    }

    std::string getCppIncludes() const {
        CppUsage all;
        all.ioPrint = all.ioReadln = all.ioGetline = all.ioToInt = all.ioFlush = true;
        all.osSystem = all.osGetenv = all.osPlatform = all.osExeDir = true;
        all.osGetProcessId = true;
        all.osWindowControl = all.osMessageBox = all.osGrepKeys = all.osKeyPressed = true;
        all.file = all.random = all.math = all.time = all.thread = all.dll = true;
        return getCppIncludes(all);
    }

private:
    std::set<std::string> enabled_;
};

}  // namespace nexa
