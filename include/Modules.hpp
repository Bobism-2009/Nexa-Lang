#pragma once

#include <string>
#include <set>

namespace nexa {

// Tracks which standard library modules are enabled by #include directives
class Modules {
public:
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

    bool hasTime() const {
        return enabled_.count("std/time") > 0;
    }

    std::string getCppIncludes() const {
        std::string out;
        if (hasIo()) {
            out += "#include <cstdio>\n";
            out += "#include <cstring>\n";
            out += "#include <cstdlib>\n";
            out += "#include <string>\n";
            out += "static int __nexa_to_int(const std::string& s) { try { return std::stoi(s); } catch(...) { return 0; } }\n";
        }
        if (hasOs()) {
            out += "#include <cstdlib>\n";
            out += "#include <cstdio>\n";
            out += "#include <string>\n";
            out += "#ifdef __linux__\n#include <unistd.h>\n#endif\n";
            out += "#ifdef _WIN32\n#include <windows.h>\n#include <conio.h>\n#endif\n";
            out += "#ifdef __APPLE__\n#include <mach-o/dyld.h>\n#endif\n";
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
            out += "static void __nexa_os_messagebox(const std::string& text, const std::string& title) {\n";
            out += "#ifdef _WIN32\n";
            out += "  MessageBoxA(NULL, text.c_str(), title.c_str(), MB_OK);\n";
            out += "#endif\n";
            out += "}\n";
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
            out += "static int __nexa_os_keypressed() {\n";
            out += "#ifdef _WIN32\n";
            out += "  return _kbhit() ? 1 : 0;\n";
            out += "#else\n";
            out += "  return 0;\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasFile()) {
            out += "#include <fstream>\n";
            out += "#include <sstream>\n";
            out += "#include <filesystem>\n";
        }
        if (hasRandom()) {
            out += "#include <random>\n";
            out += "static std::mt19937& __nexa_rng() { static std::mt19937 gen(std::random_device{}()); return gen; }\n";
            out += "static void __nexa_random_seed(int s) { __nexa_rng().seed(static_cast<unsigned>(s)); }\n";
            out += "static int __nexa_random_int(int a, int b) { return std::uniform_int_distribution<int>(a, b)(__nexa_rng()); }\n";
        }
        if (hasTime()) {
            out += "#include <thread>\n";
            out += "#include <chrono>\n";
        }
        if (hasDll()) {
            out += "#include <vector>\n";
#ifdef _WIN32
            out += "#include <windows.h>\n";
#else
            out += "#include <dlfcn.h>\n";
#endif
            out += "static std::vector<void*> __nexa_dll_handles;\n";
        }
        return out;
    }

private:
    std::set<std::string> enabled_;
};

}  // namespace nexa
