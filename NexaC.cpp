#include "include/Lexer.hpp"
#include "include/Parser.hpp"
#include "include/Transpiler.hpp"
#include "include/Modules.hpp"
#include "include/nexapkg.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <string>
#include <cstdio>
#include <filesystem>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#define NEXAC_VERSION "0.1.8"

static std::string getExePath() {
#ifdef __linux__
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = 0; return std::string(buf); }
#elif defined(_WIN32)
    char buf[4096];
    if (GetModuleFileNameA(NULL, buf, sizeof(buf))) return std::string(buf);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) return std::string(buf);
#endif
    return "";
}

// Scan dir for .nxa file containing fn main() or fn __init__(). Prefer main, then main.nxa.
static std::string findEntryFile(const std::filesystem::path& base) {
    namespace fs = std::filesystem;
    std::string foundMain, foundInit;
    for (const auto& e : fs::directory_iterator(base)) {
        if (!e.is_regular_file()) continue;
        std::string name = e.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 4) != ".nxa") continue;
        std::ifstream f(e.path());
        if (!f) continue;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
        bool hasMain = content.find("fn main(") != std::string::npos;
        bool hasInit = content.find("fn __init__(") != std::string::npos;
        if (hasMain) {
            if (name == "main.nxa") return (base / name).string();
            if (foundMain.empty()) foundMain = (base / name).string();
        }
        if (hasInit && foundInit.empty()) foundInit = (base / name).string();
    }
    return !foundMain.empty() ? foundMain : foundInit;
}

static int runNexaCChild(const std::string& extraArgs) {
    std::string exePath = getExePath();
#ifdef _WIN32
    if (exePath.empty()) exePath = "NexaC.exe";
    std::string cmdLine = "\"" + exePath + "\" " + extraArgs;
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    BOOL ok = CreateProcessA(
        NULL,
        mutableCmd.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );
    if (!ok) {
        std::cerr << "[Nexa] Error: Failed to launch build command.\n";
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0) ? 0 : 1;
#else
    std::string cmd = exePath.empty() ? "NexaC" : ("\"" + exePath + "\"");
    cmd += " " + extraArgs;
    int ret = std::system(cmd.c_str());
    return (ret == 0) ? 0 : 1;
#endif
}

static int doBuild(const std::string& dir) {
    namespace fs = std::filesystem;
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    nexa::pkg::Manifest manifest;
    fs::path manifestPath = base / "nexapkg.json";
    if (fs::exists(manifestPath)) {
        nexa::pkg::readManifest(manifestPath, manifest);
    }

    std::string entry;
    if (!manifest.entry.empty()) {
        fs::path entryPath = base / manifest.entry;
        if (!fs::exists(entryPath)) {
            std::cerr << "[Nexa] Error: Entry file not found: " << entryPath.string() << "\n";
            return 1;
        }
        entry = entryPath.string();
    } else {
        entry = findEntryFile(base);
    }
    if (entry.empty()) {
        std::cerr << "[Nexa] Error: No .nxa file with fn main() or fn __init__() in " << base.string() << "\n";
        std::cerr << "Run 'NexaC init' to create a project.\n";
        return 1;
    }
    fs::path entryPath(entry);
    std::string outBase;
    if (!manifest.output.empty()) {
        fs::path outPath(manifest.output);
        outBase = outPath.is_absolute() ? outPath.string() : (base / outPath).string();
    } else {
        outBase = (base / entryPath.stem()).string();
    }
    int exeRet = runNexaCChild("\"" + entry + "\" -o \"" + outBase + "\"");
    if (exeRet != 0) return exeRet;

    if (!manifest.dll.empty()) {
        fs::path dllSrc = base / manifest.dll;
        if (!fs::exists(dllSrc)) {
            std::cerr << "[Nexa] Error: DLL source not found: " << dllSrc.string() << "\n";
            return 1;
        }
        std::string dllOut;
        if (!manifest.dllOutput.empty()) {
            fs::path p(manifest.dllOutput);
            dllOut = p.is_absolute() ? p.string() : (base / p).string();
        } else {
            dllOut = (base / dllSrc.stem()).string();
        }
#ifdef _WIN32
        const char* libFlag = "--dll";
#else
        const char* libFlag = "--shared";
#endif
        // --preserve-names so dll.call(h, "SaveSettings") uses the Nexa function name.
        int dllRet = runNexaCChild("\"" + dllSrc.string() + "\" " + libFlag + " --preserve-names -o \"" + dllOut + "\"");
        if (dllRet != 0) return dllRet;
    }
    return 0;
}

static int doInit(const std::string& dir) {
    namespace fs = std::filesystem;
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    if (!dir.empty()) {
        if (fs::exists(base) && !fs::is_empty(base)) {
            std::cerr << "[Nexa] Error: Directory '" << dir << "' exists and is not empty.\n";
            return 1;
        }
        fs::create_directories(base);
    }
    std::string mainNxa = R"(#include <std/io>

fn main() {
    io.println("Hello, Nexa!");
}
)";
    std::string gitignore = R"(# Build output
*.exe
*.o
*.cpp
*.so
*.dll

# Packages
.nexa/

# Temp
*.tmp
)";
    std::string projName = fs::absolute(base).filename().string();
    if (projName.empty()) projName = "myapp";
    std::string nexapkgJson = "{\n  \"name\": \"" + projName + "\",\n  \"dependencies\": {}\n}\n";
    try {
        std::ofstream(base / "main.nxa") << mainNxa;
        std::ofstream(base / ".gitignore") << gitignore;
        std::ofstream(base / "nexapkg.json") << nexapkgJson;
        std::cout << "[Nexa] Initialized project in " << base.string() << "\n";
        std::cout << "  main.nxa     - Entry point\n";
        std::cout << "  nexapkg.json - Package manifest (nexapkg add, install)\n";
        std::cout << "  .gitignore   - Ignore build artifacts\n";
        std::cout << "Run: NexaC build  |  nexapkg add <pkg> && nexapkg install\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Nexa] Error: " << e.what() << "\n";
        return 1;
    }
}

// On Windows native: find clang, g++, or gcc (MinGW/MSYS2). Returns compiler name or empty.
static std::string findWindowsCxxNative() {
#ifdef _WIN32
    const char* candidates[] = {"clang++", "clang", "g++", "gcc"};
    for (const char* cxx : candidates) {
        std::string cmd = "where ";
        cmd += cxx;
        cmd += " >nul 2>&1";
        if (std::system(cmd.c_str()) == 0)
            return cxx;
    }
    return "";
#else
    return "";
#endif
}

// Return mingw-g++ path for Windows cross-compile (skip clang). Used as fallback when clang fails.
static std::string findMingwCxx() {
#ifdef _WIN32
    return "";
#else
    FILE* f = popen("which x86_64-w64-mingw32-g++ 2>/dev/null", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) && buf[0]) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            pclose(f);
            return s;
        }
        pclose(f);
    }
    f = popen("which i686-w64-mingw32-g++ 2>/dev/null", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) && buf[0]) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            pclose(f);
            return s;
        }
        pclose(f);
    }
    return "";
#endif
}

// Return C++ compiler for cross-compiling to Windows (clang with mingw target, or mingw-g++ fallback).
static std::string findWindowsCxx() {
#ifdef _WIN32
    return "";  // On Windows, use clang++ for DLL
#else
    // Prefer clang++ with mingw target
    FILE* f = popen("which clang++ 2>/dev/null", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) && buf[0]) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            pclose(f);
            return s;
        }
        pclose(f);
    }
    f = popen("which x86_64-w64-mingw32-g++ 2>/dev/null", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) && buf[0]) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            pclose(f);
            return s;
        }
        pclose(f);
    }
    f = popen("which i686-w64-mingw32-g++ 2>/dev/null", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f) && buf[0]) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            pclose(f);
            return s;
        }
        pclose(f);
    }
    return "";
#endif
}

enum class WasmKind { None, Emscripten, Wasi };

struct WasmTool {
    std::string cxx;
    WasmKind kind = WasmKind::None;
    std::string sysroot;
};

static bool nexaCmdExists(const std::string& name) {
#ifdef _WIN32
    return std::system(("where " + name + " >nul 2>&1").c_str()) == 0;
#else
    return std::system(("command -v " + name + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

static bool nexaPathExists(const std::string& p) {
    return !p.empty() && std::filesystem::exists(p);
}

static WasmTool findWasmCxx() {
    WasmTool w;
    if (const char* env = std::getenv("NEXA_WASM_CXX")) {
        if (env[0]) {
            w.cxx = env;
            std::string low = env;
            for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            w.kind = (low.find("em++") != std::string::npos || low.find("emcc") != std::string::npos)
                ? WasmKind::Emscripten : WasmKind::Wasi;
            if (const char* sr = std::getenv("WASI_SYSROOT")) w.sysroot = sr;
            else if (const char* wp = std::getenv("WASI_SDK_PATH")) {
#ifdef _WIN32
                w.sysroot = std::string(wp) + "\\share\\wasi-sysroot";
#else
                w.sysroot = std::string(wp) + "/share/wasi-sysroot";
#endif
            }
            return w;
        }
    }

    auto takeEm = [&](const std::string& p, bool onPath) -> bool {
        if (p.empty()) return false;
        if (onPath) {
            if (!nexaCmdExists(p)) return false;
        } else if (!nexaPathExists(p)) {
            return false;
        }
        w.cxx = p;
        w.kind = WasmKind::Emscripten;
        return true;
    };

    if (takeEm("em++", true)) return w;
#ifdef _WIN32
    if (takeEm("em++.exe", true)) return w;
    if (takeEm("em++.bat", true)) return w;
#endif

    auto takeEmAt = [&](const std::string& dir) -> bool {
        if (dir.empty()) return false;
#ifdef _WIN32
        if (takeEm(dir + "\\em++.exe", false)) return true;
        if (takeEm(dir + "\\em++.bat", false)) return true;
        if (takeEm(dir + "\\em++", false)) return true;
#else
        if (takeEm(dir + "/em++", false)) return true;
#endif
        return false;
    };

    if (const char* emsdk = std::getenv("EMSDK")) {
        if (emsdk[0]) {
#ifdef _WIN32
            if (takeEmAt(std::string(emsdk) + "\\upstream\\emscripten")) return w;
#else
            if (takeEmAt(std::string(emsdk) + "/upstream/emscripten")) return w;
#endif
        }
    }

    auto takeEmHome = [&](const char* home) -> bool {
        if (!home || !home[0]) return false;
#ifdef _WIN32
        return takeEmAt(std::string(home) + "\\emsdk\\upstream\\emscripten");
#else
        return takeEmAt(std::string(home) + "/emsdk/upstream/emscripten");
#endif
    };
    if (takeEmHome(std::getenv("USERPROFILE"))) return w;
    if (takeEmHome(std::getenv("HOME"))) return w;

    if (const char* wasi = std::getenv("WASI_SDK_PATH")) {
        if (wasi[0]) {
#ifdef _WIN32
            std::string cxx = std::string(wasi) + "\\bin\\clang++.exe";
            if (!nexaPathExists(cxx)) cxx = std::string(wasi) + "\\bin\\clang++";
            std::string root = std::string(wasi) + "\\share\\wasi-sysroot";
#else
            std::string cxx = std::string(wasi) + "/bin/clang++";
            std::string root = std::string(wasi) + "/share/wasi-sysroot";
#endif
            if (nexaPathExists(cxx)) {
                w.cxx = cxx;
                w.kind = WasmKind::Wasi;
                if (nexaPathExists(root)) w.sysroot = root;
                return w;
            }
        }
    }
    return w;
}

static std::string nexaUserHome() {
#ifdef _WIN32
    const char* p = std::getenv("USERPROFILE");
    if (!p || !p[0]) p = std::getenv("HOME");
#else
    const char* p = std::getenv("HOME");
    if (!p || !p[0]) p = std::getenv("USERPROFILE");
#endif
    return (p && p[0]) ? std::string(p) : std::string();
}

static std::string nexaDefaultEmsdkDir() {
    std::string home = nexaUserHome();
    if (home.empty()) return "";
#ifdef _WIN32
    return (std::filesystem::path(home) / "emsdk").string();
#else
    return (std::filesystem::path(home) / "emsdk").string();
#endif
}

// Yes/No install prompt. Windows uses a real MessageBox; macOS/Linux use the native dialog.
static bool nexaAskYesNo(const char* title, const char* text) {
#ifdef _WIN32
    using MsgBoxFn = int (WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (!user32) return false;
    auto fn = reinterpret_cast<MsgBoxFn>(GetProcAddress(user32, "MessageBoxA"));
    int r = 0;
    if (fn) {
        r = fn(NULL, text, title, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST);
    }
    FreeLibrary(user32);
    return r == IDYES;
#elif defined(__APPLE__)
    std::string script = "display dialog \"";
    for (const char* p = text; *p; ++p) {
        if (*p == '"') script += '\\';
        if (*p == '\n') script += "\\n";
        else script += *p;
    }
    script += "\" with title \"";
    script += title;
    script += "\" buttons {\"Cancel\", \"Install\"} default button \"Install\" with icon caution";
    std::string cmd = "osascript -e '";
    cmd += script;
    cmd += "' >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
#else
    auto shEscape = [](const char* s) {
        std::string o = "'";
        for (const char* p = s; *p; ++p) {
            if (*p == '\'') o += "'\\''";
            else o += *p;
        }
        o += "'";
        return o;
    };
    if (nexaCmdExists("zenity")) {
        std::string cmd = "zenity --question --title=";
        cmd += shEscape(title);
        cmd += " --text=";
        cmd += shEscape(text);
        cmd += " --ok-label=Install --cancel-label=Cancel >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    }
    if (nexaCmdExists("kdialog")) {
        std::string cmd = "kdialog --title ";
        cmd += shEscape(title);
        cmd += " --yesno ";
        cmd += shEscape(text);
        cmd += " >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    }
    if (isatty(STDIN_FILENO)) {
        std::cerr << title << "\n" << text << " [Y/n] ";
        std::cerr.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line == "y" || line == "Y" || line == "yes" || line == "Yes") return true;
        return false;
    }
    return false;
#endif
}

static void nexaAlert(const char* title, const char* text) {
#ifdef _WIN32
    using MsgBoxFn = int (WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        auto fn = reinterpret_cast<MsgBoxFn>(GetProcAddress(user32, "MessageBoxA"));
        if (fn) fn(NULL, text, title, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
        FreeLibrary(user32);
    }
#elif defined(__APPLE__)
    std::string script = "display dialog \"";
    for (const char* p = text; *p; ++p) {
        if (*p == '"') script += '\\';
        if (*p == '\n') script += "\\n";
        else script += *p;
    }
    script += "\" with title \"";
    script += title;
    script += "\" buttons {\"OK\"} default button \"OK\" with icon caution";
    std::string cmd = "osascript -e '";
    cmd += script;
    cmd += "' >/dev/null 2>&1";
    std::system(cmd.c_str());
#else
    if (nexaCmdExists("zenity")) {
        std::string cmd = "zenity --warning --title='NexaC' --text='";
        cmd += text;
        cmd += "' >/dev/null 2>&1";
        std::system(cmd.c_str());
    } else if (nexaCmdExists("kdialog")) {
        std::string cmd = "kdialog --error '";
        cmd += text;
        cmd += "' >/dev/null 2>&1";
        std::system(cmd.c_str());
    }
#endif
    std::cerr << "[Nexa] " << title << ": " << text << "\n";
}

static void nexaPrintWasmInstallHelp() {
    std::cerr << "[Nexa] Error: No WebAssembly C++ toolchain found.\n";
    std::cerr << "  Install Emscripten (em++) and put it on PATH, or set EMSDK:\n";
    std::cerr << "    https://emscripten.org/docs/getting_started/downloads.html\n";
    std::cerr << "  Or install WASI-SDK and set WASI_SDK_PATH.\n";
    std::cerr << "  Override the compiler with NEXA_WASM_CXX.\n";
}

static bool nexaInstallEmsdk(const std::string& dest) {
    if (dest.empty()) {
        nexaAlert("NexaC", "Cannot install Emscripten: no user home directory.");
        return false;
    }
    if (!nexaCmdExists("git")) {
        nexaAlert("NexaC",
            "Git is required to install the WASM toolchain.\n"
            "Install Git from https://git-scm.com/ and try --wasm again.");
        return false;
    }

#ifdef _WIN32
    const std::string launcher = (std::filesystem::path(dest) / "emsdk.bat").string();
#else
    const std::string launcher = (std::filesystem::path(dest) / "emsdk").string();
#endif
    if (!nexaPathExists(launcher)) {
        if (nexaPathExists(dest)) {
            std::error_code ec;
            if (!std::filesystem::is_empty(dest, ec)) {
                nexaAlert("NexaC",
                    ("Cannot install Emscripten: " + dest +
                     " already exists and is not an emsdk checkout.").c_str());
                return false;
            }
        }
        std::cout << "[Nexa] Cloning emsdk into " << dest << "...\n";
        std::cout.flush();
        std::string clone = "git clone --depth 1 https://github.com/emscripten-core/emsdk.git \"" + dest + "\"";
        if (std::system(clone.c_str()) != 0) {
            nexaAlert("NexaC", "Failed to clone emsdk. Check your network and Git install.");
            return false;
        }
    }

    std::cout << "[Nexa] Installing Emscripten (this can take several minutes)...\n";
    std::cout.flush();
#ifdef _WIN32
    std::string install = "\"" + launcher + "\" install latest";
    std::string activate = "\"" + launcher + "\" activate latest";
#else
    std::string install = "\"" + launcher + "\" install latest";
    std::string activate = "\"" + launcher + "\" activate latest";
#endif
    if (std::system(install.c_str()) != 0) {
        nexaAlert("NexaC", "emsdk install latest failed. See the console output for details.");
        return false;
    }
    if (std::system(activate.c_str()) != 0) {
        nexaAlert("NexaC", "emsdk activate latest failed. See the console output for details.");
        return false;
    }
    return true;
}

static bool nexaEnsureWasmTool(WasmTool& tool) {
    tool = findWasmCxx();
    if (tool.kind != WasmKind::None) return true;

    const std::string dest = nexaDefaultEmsdkDir();
    std::string ask =
        "NexaC needs the Emscripten WASM toolchain (em++) to compile --wasm.\n\n"
        "Install it now? Emscripten will be downloaded into:\n\n  ";
    ask += dest.empty() ? std::string("(your home folder)/emsdk") : dest;
    ask += "\n\nThis can take several minutes and requires Git.";

    std::cout << "[Nexa] WASM toolchain not found.\n";
    std::cout.flush();
    if (!nexaAskYesNo("NexaC - Install WASM toolchain?", ask.c_str())) {
        nexaPrintWasmInstallHelp();
        return false;
    }
    if (!nexaInstallEmsdk(dest)) return false;

    tool = findWasmCxx();
    if (tool.kind == WasmKind::None) {
        nexaAlert("NexaC",
            "Emscripten finished installing, but em++ was still not found.\n"
            "Open a new terminal, or set EMSDK to your emsdk folder, and retry --wasm.");
        nexaPrintWasmInstallHelp();
        return false;
    }
    std::cout << "[Nexa] WASM toolchain ready: " << tool.cxx << "\n";
    std::cout.flush();
    return true;
}

// Emscripten treats bare `-s` as a settings flag (not strip). Never reuse the native link line.
static std::string nexaWasmCompileCmd(
    const WasmTool& tool,
    const std::string& cppPath,
    const std::string& outPath,
    const std::string& opt,
    bool noExceptions,
    bool noRtti,
    bool linkHttp,
    bool linkThread,
    const std::vector<std::string>& linkInputs
) {
#ifdef _WIN32
    std::string cmd = (tool.cxx.find(' ') != std::string::npos) ? ("\"" + tool.cxx + "\"") : tool.cxx;
#else
    std::string cmd = "\"" + tool.cxx + "\"";
#endif
    cmd += " -std=c++17 ";
    cmd += opt;
    cmd += " -DNEXA_WASM=1";
    if (tool.kind == WasmKind::Emscripten || tool.cxx.find("clang") != std::string::npos) {
        cmd += " -Wno-parentheses-equality -Wno-return-type-c-linkage";
    }
    if (noRtti) cmd += " -fno-rtti";
    if (noExceptions) cmd += " -fno-exceptions";
    cmd += " \"" + cppPath + "\"";
    if (tool.kind == WasmKind::Emscripten) {
        cmd += " -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1";
        if (linkHttp) cmd += " -sFETCH=1 -sASYNCIFY";
        if (linkThread) cmd += " -pthread -sPTHREAD_POOL_SIZE=4";
    } else {
        cmd += " --target=wasm32-wasi";
        if (!tool.sysroot.empty()) {
            cmd += " --sysroot=\"";
            cmd += tool.sysroot;
            cmd += "\"";
        }
        cmd += " -ffunction-sections -fdata-sections -Wl,--gc-sections,--strip-all";
    }
    if (const char* nexaCxx = std::getenv("NEXA_CXXFLAGS")) {
        cmd += " ";
        cmd += nexaCxx;
    }
    for (const std::string& li : linkInputs) {
        cmd += " ";
        if (!li.empty() && li[0] == '-') cmd += li;
        else cmd += "\"" + li + "\"";
    }
    if (const char* nexaLd = std::getenv("NEXA_LDFLAGS")) {
        cmd += " ";
        cmd += nexaLd;
    }
    cmd += " -o \"" + outPath + "\"";
#ifndef _WIN32
    cmd += " 2>&1";
#endif
    return cmd;
}

static int runWasmOutput(const std::string& path, WasmKind kind) {
    if (kind == WasmKind::Emscripten) {
        if (!nexaCmdExists("node")) {
            std::cerr << "[Nexa] Error: --run --wasm needs node on PATH to execute the Emscripten loader.\n";
            return 1;
        }
        return std::system(("node \"" + path + "\"").c_str());
    }
    if (nexaCmdExists("wasmtime")) return std::system(("wasmtime \"" + path + "\"").c_str());
    if (nexaCmdExists("wasmer")) return std::system(("wasmer run \"" + path + "\"").c_str());
    std::cerr << "[Nexa] Error: --run --wasm (WASI) needs wasmtime or wasmer on PATH.\n";
    return 1;
}

static bool nexaHasExt(const std::string& path, const char* ext) {
    std::string e = std::filesystem::path(path).extension().string();
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e == ext;
}

// Build the shell command for clang/g++/gcc. Linker flags are selected per object format:
// PE/COFF on Windows, Mach-O on macOS, and ELF on Linux.
static std::string nexaBuildCompileCmd(
    const std::string& cxx,
    const std::string& targetFlags,
    const std::string& cppPath,
    const std::string& exePath,
    const std::string& opt,
    bool buildDll,
    bool buildShared,
    bool buildWin,
    bool modulesHasDll,
    bool noConsole,
    bool linkUser32,
    bool linkHttp,
    bool noExceptions,
    bool noRtti,
    const std::vector<std::string>& linkInputs
) {
    // Windows cmd.exe: do not wrap the compiler name in quotes unless it contains spaces;
    // "clang" combined with other quoted paths can confuse cmd's parser (error 123).
#ifdef _WIN32
    std::string cmd = cxx + targetFlags;
#else
    std::string cmd = "\"" + cxx + "\"" + targetFlags;
#endif
    cmd += " \"" + cppPath + "\" " + opt;
    if (cxx.find("clang") != std::string::npos) {
        // Generated C++ may intentionally carry extra grouping parentheses.
        cmd += " -Wno-parentheses-equality";
        // Forward decls for Nexa wrapper fns inherit `extern "C"` linkage but may
        // return std::string; clang emits a pedantic warning that doesn't matter
        // (the whole pipeline is clang->clang, not C interop).
        cmd += " -Wno-return-type-c-linkage";
    }
    // Size/perf: drop machinery the generated code provably never uses. RTTI is never emitted
    // by the transpiler; exceptions/unwind tables are only needed for try/catch, throw, std::stoi,
    // or inline_cpp. Stripping them removes .eh_frame and RTTI metadata (smaller, no perf cost).
    if (noRtti) {
        cmd += " -fno-rtti";
    }
    if (noExceptions) {
        cmd += " -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables";
    }
    // Trim non-essential metadata from the object/binary (no runtime effect).
    cmd += " -fno-ident -fmerge-all-constants";
    if (const char* nexaCxx = std::getenv("NEXA_CXXFLAGS")) {
        cmd += " ";
        cmd += nexaCxx;
    }
#ifdef _WIN32
    cmd += " -ffunction-sections -fdata-sections";
    if (buildDll || buildShared) {
        cmd += " -shared";
        // Match Linux: garbage-collect unused sections + strip symbols. MSVC-target clang
        // builds are uncommon here; if linking fails, use a MinGW/LLVM-MinGW toolchain.
        cmd += " -Wl,--gc-sections -s";
        if (buildDll) {
            std::filesystem::path dllOut(exePath);
            std::filesystem::path libOut = dllOut;
            libOut.replace_extension(".lib");
            cmd += " -Wl,--out-implib,\"";
            cmd += libOut.string();
            cmd += "\"";
        }
    } else {
        // Quoted for cmd.exe: unquoted /SUBSYSTEM is parsed as multiple invalid paths (error 123).
        cmd += noConsole ? " -Xlinker \"/SUBSYSTEM:WINDOWS\"" : " -Xlinker \"/SUBSYSTEM:CONSOLE\"";
        // Fully static executables: avoid libgcc/libstdc++ (or mixed libc++) DLLs on machines
        // without the compiler's bin directory on PATH.
        cmd += " -static -static-libgcc -static-libstdc++";
        // Same as Linux: drop unreferenced object code from static libc++ and strip symbols
        // (ffunction/fdata sections were enabled above; without --gc-sections, .exe stays large).
        cmd += " -Wl,--gc-sections -s";
    }
#elif defined(__APPLE__)
    // ld64 uses dead_strip instead of GNU ld's --gc-sections. Apple platforms
    // provide libc++ as a system library, so static libgcc/libstdc++ flags are invalid.
    cmd += " -Wl,-dead_strip";
    if (buildDll || buildShared) {
        cmd += " -dynamiclib -fPIC";
    }
#else
    cmd += " -s -ffunction-sections -fdata-sections -Wl,--gc-sections";
    // Native ELF output (Linux exe or .so), not mingw-cross (PE) builds.
    const bool elfTarget = !buildWin && !buildDll;
    if (elfTarget) {
        // On aarch64 (e.g. Raspberry Pi) the default max-page-size is 64KB, which pads even a
        // trivial binary to ~64KB+ of segment alignment. 4KB pages (the kernel default on Pi OS
        // and most aarch64 Linux) shrink output ~10x. On x86-64 this is already the default (no-op).
        cmd += " -Wl,-z,max-page-size=4096";
        cmd += " -Wl,--build-id=none";
    }
    if (elfTarget && !buildShared) {
        // Self-contained w.r.t. the C++ toolchain runtime: embed libstdc++ and libgcc so the
        // binary does not require those to be installed on the target. Base system libraries
        // (libc, libm, the dynamic loader) stay dynamic since they exist on every Linux by
        // default. Programs that never touch the C++ runtime (e.g. only printf/puts) pull in
        // nothing extra and stay tiny; programs using std::string/exceptions embed only the
        // parts they use. Shared libraries (.so) are excluded: they load into a host process
        // and must share its libstdc++ to avoid duplicate-runtime issues.
        cmd += " -static-libstdc++ -static-libgcc";
    }
    if (buildDll || buildShared) {
        cmd += " -shared -fPIC";
    } else if (buildWin && noConsole) {
        // mingw target: mark as GUI subsystem to suppress console window.
        cmd += " -Wl,--subsystem,windows";
    }
#endif
    if (buildWin && !buildDll && !buildShared) {
#ifndef _WIN32
        cmd += " -static -static-libgcc -static-libstdc++";
#endif
    }
#ifdef _WIN32
    if (linkUser32) {
        // std/os (MessageBoxA, GetConsoleWindow, …) and some inline_cpp; lld does not always pull it implicitly.
        cmd += " -luser32";
        // std/os audio (os.set_volume/get_volume/mute) uses the Core Audio COM API.
        cmd += " -lole32";
        // std/os open uses ShellExecuteA.
        cmd += " -lshell32";
    }
    if (linkHttp) {
        // std/http uses WinHTTP (OS API; HTTPS via Schannel).
        cmd += " -lwinhttp";
    }
#elif defined(__APPLE__)
    if (linkHttp) {
        cmd += " -framework CoreFoundation -framework CFNetwork";
    }
#endif
    // Extra link inputs (--link): static archives (.a/.lib) are baked in, objects (.o) embedded,
    // shared libs (.so/.dll) linked dynamically. Placed after the main object so archive members
    // that satisfy references from the generated code are pulled in (correct GNU ld link order).
    for (const std::string& li : linkInputs) {
        cmd += " ";
        // Pass linker-style tokens (e.g. -lpthread, -L/path) verbatim; quote file paths.
        if (!li.empty() && li[0] == '-') cmd += li;
        else cmd += "\"" + li + "\"";
    }
    if (const char* nexaLd = std::getenv("NEXA_LDFLAGS")) {
        cmd += " ";
        cmd += nexaLd;
    }
    cmd += " -o \"" + exePath + "\"";
#if !defined(_WIN32) && !defined(__APPLE__)
    if (modulesHasDll && buildShared) cmd += " -ldl";
#endif
#ifndef _WIN32
    cmd += " 2>&1";
#endif
    return cmd;
}

// Build a static archive (.a / .lib) from the generated C++: compile to a relocatable object,
// then archive it. Objects are compiled -fPIC so the archive links cleanly into PIE executables.
static std::string nexaStaticLibCmd(
    const std::string& cxx,
    const std::string& cppPath,
    const std::string& objPath,
    const std::string& archivePath,
    const std::string& opt,
    bool noExceptions,
    bool noRtti
) {
#ifdef _WIN32
    std::string cmd = cxx;
#else
    std::string cmd = "\"" + cxx + "\"";
#endif
    cmd += " \"" + cppPath + "\" " + opt;
    if (cxx.find("clang") != std::string::npos) {
        cmd += " -Wno-parentheses-equality";
    }
    if (noRtti) cmd += " -fno-rtti";
    if (noExceptions) cmd += " -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables";
    cmd += " -fno-ident -fmerge-all-constants -ffunction-sections -fdata-sections -fPIC -c";
    if (const char* nexaCxx = std::getenv("NEXA_CXXFLAGS")) {
        cmd += " ";
        cmd += nexaCxx;
    }
    cmd += " -o \"" + objPath + "\"";
#ifndef _WIN32
    cmd += " 2>&1";
#endif
    // Archive the object. llvm-ar (ships with clang) and GNU ar both accept "rcs".
#ifdef _WIN32
    std::string ar = "llvm-ar";
#else
    std::string ar = "ar";
#endif
    cmd += " && " + ar + " rcs \"" + archivePath + "\" \"" + objPath + "\"";
    return cmd;
}

// Resolve topic/page string to page number (1-9). 0 = unknown.
static int helpPageFromArg(const std::string& arg) {
    if (arg.empty() || arg == "1" || arg == "page1") return 1;
    if (arg == "2" || arg == "page2" || arg == "core" || arg == "lang" || arg == "language") return 2;
    if (arg == "3" || arg == "page3" || arg == "std/io") return 3;
    if (arg == "4" || arg == "page4" || arg == "std/os") return 4;
    if (arg == "5" || arg == "page5" || arg == "std/dll") return 5;
    if (arg == "6" || arg == "page6" || arg == "std/file") return 6;
    if (arg == "7" || arg == "page7" || arg == "std/random") return 7;
    if (arg == "8" || arg == "page8" || arg == "std/thread") return 8;
    if (arg == "9" || arg == "page9" || arg == "std/inline" || arg == "inline" || arg == "inline_cpp") return 9;
    if (arg.size() >= 6 && arg.substr(0, 6) == "--page") {
        int n = 0;
        for (size_t i = 6; i < arg.size() && std::isdigit(arg[i]); i++)
            n = n * 10 + (arg[i] - '0');
        if (n >= 1 && n <= 9) return n;
    }
    return 0;
}

static int printHelp(int page = 1) {
    if (page == 2) {
        std::cout << "NexaC - Core language (page 2/9)\n\n";
        std::cout << "Entry point:\n";
        std::cout << "  fn main() {\n";
        std::cout << "    ...\n";
        std::cout << "  }\n\n";
        std::cout << "Functions:\n";
        std::cout << "  fn name(a, b) { ... }   Define function with parameters (int)\n";
        std::cout << "  return expr;             Return value from function\n";
        std::cout << "  name(x, y);              Call function (statement)\n";
        std::cout << "  let z = name(x, y);      Call function (expression)\n";
        std::cout << "  By default, NexaC mangles function names in C++ output.\n";
        std::cout << "  Use --preserve-names to keep original names.\n\n";
        std::cout << "Variables:\n";
        std::cout << "  let name = value;\n\n";
        std::cout << "  Types:\n";
        std::cout << "    int:    let x = 42;  let n = -10;\n";
        std::cout << "    string: let s = \"hello\";  let s = io.readln();\n\n";
        std::cout << "  Initializers:\n";
        std::cout << "    - number literal: let x = 42;\n";
        std::cout << "    - string literal: let s = \"hi\";\n";
        std::cout << "    - io.readln():    let s = io.readln();\n";
        std::cout << "    - io.getline(s[, n]): get line n (default 1) from string buffer s\n";
        std::cout << "    - trim(s[, prefix]): strip whitespace; optional prefix removed from start after (built-in)\n";
        std::cout << "    - dll.load():     let h = dll.load(\"./lib.so\");  (requires std/dll)\n";
        std::cout << "    - expression:     let sum = a + b; let x = add(2, 3);\n\n";
        std::cout << "  Arithmetic: + - * / % (int only, * / % before + and -)\n";
        std::cout << "    let sum = a + b;  io.println(x * 2);\n\n";
        std::cout << "  Reassignment: x = expr; (variable must be declared first)\n";
        std::cout << "    x = x + 1;  n = 0;\n\n";
        std::cout << "  Conditionals: if, else if, else\n";
        std::cout << "    Comparisons: ==, !=, <, <=, >, >=\n";
        std::cout << "    Logical: && (and), || (or), ! (not)\n";
        std::cout << "    if (x > 0) { ... } else if (x < 0) { ... } else { ... }\n\n";
        std::cout << "  Loops: while (cond) { ... }\n";
        std::cout << "    while (i < 10) { io.println(i); i = i + 1; }\n\n";
        std::cout << "Comments:\n";
        std::cout << "  // line comment\n";
        std::cout << "  /* block comment */\n\n";
        std::cout << "Modules:\n";
        std::cout << "  #include <std/io>   - print, println, readln, getline, trim, to_int\n";
        std::cout << "  #include <std/os>   - system, platform, getenv\n";
        std::cout << "  #include <std/dll>  - load, call (dynamic libraries)\n";
        std::cout << "  #include <std/file> - read, write, append, exists\n";
        std::cout << "  #include <std/random> - int, seed\n";
        std::cout << "  #include <std/thread> - spawn, join\n";
        std::cout << "  #include <std/inline> - inline_cpp! { ... } (embed C++)\n\n";
        std::cout << "  #include \"file.nxa\"  - include another .nxa file (path relative to current file)\n";
        std::cout << "  #include \"file.h\" / .hpp / .hxx / .hh  - pass C/C++ header into generated .cpp (quoted: absolute path)\n";
        std::cout << "  #include <path.hpp>  - same for angle form when extension is .h, .hpp, .hxx, or .hh (verbatim line)\n";
        std::cout << "    \"lib.nxa\" = same dir, \"../other.nxa\" = parent dir\n\n";
        std::cout << "Full example:\n";
        std::cout << "  #include <std/io>\n\n";
        std::cout << "  fn add(a, b) {\n";
        std::cout << "    return a + b;\n";
        std::cout << "  }\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    let x = add(2, 3);\n";
        std::cout << "    io.println(x);\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page3  NexaC --help std/io\n";
        return 0;
    }
    if (page == 3) {
        std::cout << "NexaC - std/io module (page 3/9)\n\n";
        std::cout << "Input/output. Include with: #include <std/io>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  io.print(arg)\n";
        std::cout << "    Prints to stdout, no newline.\n";
        std::cout << "    arg: string literal \"...\" or variable (int or string)\n\n";
        std::cout << "  io.println(arg)\n";
        std::cout << "    Prints to stdout, adds newline.\n";
        std::cout << "    arg: string literal \"...\" or variable (int or string)\n\n";
        std::cout << "  io.readln()\n";
        std::cout << "    Reads one line from stdin.\n";
        std::cout << "    Returns: string\n";
        std::cout << "    Use with: let var = io.readln();\n\n";
        std::cout << "  io.getline(text[, lineNo])\n";
        std::cout << "    Returns line lineNo (1-based, default 1) from string buffer text.\n";
        std::cout << "    Use with: let l = io.getline(file.read(\"data.txt\"), 2);\n\n";
        std::cout << "  io.trim(s[, prefix])\n";
        std::cout << "    Same as trim(s[, prefix]): trims ASCII whitespace; if prefix is set, strips it once from the\n";
        std::cout << "    start after trimming (e.g. io.trim(io.getline(buf, \"x\"), \"x:\") for value 5 from line \"x:5\").\n\n";
        std::cout << "Variables used:\n";
        std::cout << "  - string: from \"literal\", io.readln(), io.getline(...), trim/io.trim(...)\n";
        std::cout << "  - int: from 42, -10, etc.\n\n";
        std::cout << "Example:\n";
        std::cout << "  io.print(\"Name: \");\n";
        std::cout << "  let name = io.readln();\n";
        std::cout << "  let second = io.getline(\"a\\nb\\nc\", 2);\n";
        std::cout << "  io.println(name);\n";
        std::cout << "  let x = 42;\n";
        std::cout << "  io.println(x);\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page4  NexaC --help std/os\n";
        return 0;
    }
    if (page == 4) {
        std::cout << "NexaC - std/os module (page 4/9)\n\n";
        std::cout << "OS and system calls. Include with: #include <std/os>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  os.system(cmd)\n";
        std::cout << "    Runs a shell command. Blocks until complete.\n";
        std::cout << "    cmd: string literal \"...\" or string variable\n";
        std::cout << "    Returns: (exit code from shell, not captured in Nexa)\n\n";
        std::cout << "  os.platform()\n";
        std::cout << "    Returns OS name: \"windows\", \"linux\", \"darwin\", \"wasm\", or \"unknown\".\n";
        std::cout << "    Use: let p = os.platform();  io.println(p);\n\n";
        std::cout << "  os.getenv(name)\n";
        std::cout << "    Returns environment variable value (string) or \"\" if unset.\n\n";
        std::cout << "  os.getprocessid() / os.getpid()\n";
        std::cout << "    Returns current process ID as int.\n";
        std::cout << "    Use: let pid = os.getprocessid();  io.println(pid);\n\n";
        std::cout << "  os.grepkeys() / os.getkey()\n";
        std::cout << "    Waits for a key press, returns key as string (Windows only).\n";
        std::cout << "    Use: let key = os.grepkeys();  Returns \"a\", \"Enter\", \"Escape\", \"Up\", etc.\n\n";
        std::cout << "  os.keypressed()\n";
        std::cout << "    Non-blocking: returns 1 if a key is waiting, 0 otherwise (Windows).\n";
        std::cout << "    Use: if (os.keypressed()) { let key = os.grepkeys(); }\n\n";
        std::cout << "Variables used:\n";
        std::cout << "  - string: command to run, e.g. let cmd = \"ls -la\";\n\n";
        std::cout << "Example:\n";
        std::cout << "  os.system(\"ls -la\");\n";
        std::cout << "  let cmd = \"echo hello\";\n";
        std::cout << "  os.system(cmd);\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page5  NexaC --help std/dll\n";
        return 0;
    }
    if (page == 5) {
        std::cout << "NexaC - std/dll module (page 5/9)\n\n";
        std::cout << "Dynamic library loading. Include with: #include <std/dll>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  dll.load(path)\n";
        std::cout << "    Loads a .so (Linux) or .dll (Windows) file.\n";
        std::cout << "    path: string literal, e.g. \"./mylib.so\" or \"mylib.dll\"\n";
        std::cout << "    Returns: handle (int) for use with dll.call\n";
        std::cout << "    Use with: let h = dll.load(\"./mylib.so\");\n\n";
        std::cout << "  dll.call(handle, \"symbol\", args...)\n";
        std::cout << "    Calls an exported function from the loaded library.\n";
        std::cout << "    handle: variable from dll.load\n";
        std::cout << "    symbol: string literal, function name to call\n";
        std::cout << "    args: optional values matching the function parameters\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/dll>\n";
        std::cout << "  #include <std/io>\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    let h = dll.load(\"./plugin.so\");\n";
        std::cout << "    dll.call(h, \"plugin_init\");\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page6  NexaC --help std/file\n";
        return 0;
    }
    if (page == 6) {
        std::cout << "NexaC - std/file module (page 6/9)\n\n";
        std::cout << "File I/O. Include with: #include <std/file>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  file.read(path)\n";
        std::cout << "    Reads entire file into a string.\n";
        std::cout << "    path: string literal or variable\n";
        std::cout << "    Returns: string\n";
        std::cout << "    Use with: let s = file.read(\"data.txt\");\n\n";
        std::cout << "  file.write(path, content)\n";
        std::cout << "    Writes content to file (overwrites).\n";
        std::cout << "    path, content: string or int (content)\n\n";
        std::cout << "  file.append(path, content)\n";
        std::cout << "    Appends content to file.\n\n";
        std::cout << "  file.exists(path)\n";
        std::cout << "    Returns 1 if file exists, 0 otherwise.\n";
        std::cout << "    Use: io.println(file.exists(\"x.txt\"));\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/io>\n";
        std::cout << "  #include <std/file>\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    file.write(\"out.txt\", \"hello\");\n";
        std::cout << "    let s = file.read(\"out.txt\");\n";
        std::cout << "    io.println(s);\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page7  NexaC --help std/random\n";
        return 0;
    }
    if (page == 7) {
        std::cout << "NexaC - std/random module (page 7/9)\n\n";
        std::cout << "Random numbers. Include with: #include <std/random>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  random.int(min, max)\n";
        std::cout << "    Returns random int in [min, max] inclusive.\n";
        std::cout << "    Use: let roll = random.int(1, 6);  io.println(random.int(1, 20));\n\n";
        std::cout << "  random.seed(n)\n";
        std::cout << "    Seeds the RNG for reproducible sequences.\n";
        std::cout << "    Use: random.seed(42);\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/io>\n";
        std::cout << "  #include <std/random>\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    let d6 = random.int(1, 6);\n";
        std::cout << "    io.println(d6);\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page8  NexaC --help std/thread\n";
        return 0;
    }
    if (page == 8) {
        std::cout << "NexaC - std/thread module (page 8/9)\n\n";
        std::cout << "Threading. Include with: #include <std/thread>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  thread.spawn(fn_name)\n";
        std::cout << "    Starts fn_name() on a new OS thread.\n";
        std::cout << "    Returns: thread handle (int)\n";
        std::cout << "    fn_name must be a zero-argument function.\n\n";
        std::cout << "  thread.join(handle)\n";
        std::cout << "    Waits for a previously spawned thread to finish.\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/io>\n";
        std::cout << "  #include <std/thread>\n\n";
        std::cout << "  fn worker() {\n";
        std::cout << "    io.println(\"worker running\");\n";
        std::cout << "  }\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    let t = thread.spawn(worker);\n";
        std::cout << "    thread.join(t);\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page9  NexaC --help std/inline\n";
        return 0;
    }
    if (page == 9) {
        std::cout << "NexaC - std/inline (inline_cpp!) (page 9/9)\n\n";
        std::cout << "Embeds C++ in Nexa when the language has no API for your case.\n";
        std::cout << "Enable with: #include <std/inline>\n\n";
        std::cout << "Syntax:\n";
        std::cout << "  inline_cpp! {\n";
        std::cout << "    ... C++ statements ...\n";
        std::cout << "  }\n\n";
        std::cout << "The block body is pasted into generated C++ (Clang must parse it).\n";
        std::cout << "Lines starting with #include are hoisted to the top of the .cpp (required for\n";
        std::cout << "standard headers when they appear inside a function).\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/inline>\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    inline_cpp! {\n";
        std::cout << "#include <iostream>\n";
        std::cout << "      std::cout << \"Hello\\n\";\n";
        std::cout << "    }\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
        std::cout << "  NexaC --help --page1\n";
        return 0;
    }
    // Page 1 (default): usage and options
    std::cout << "NexaC - Nexa compiler (page 1/9)\n";
    std::cout << "A general purpose, high-performance programming language.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  NexaC init [dir]       Scaffold new project (current dir or dir/)\n";
    std::cout << "  NexaC build [dir]      Build .nxa with fn main() or fn __init__()\n";
    std::cout << "  NexaC <file.nxa> [-o <executable>]\n";
    std::cout << "  NexaC <file.nxa> --source <output.cpp>\n";
    std::cout << "  NexaC --help [page|module]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <name>     Output executable name\n";
    std::cout << "  --source [opts...] <f>  Emit C++ only (e.g. --source --p Out.cpp)\n";
    std::cout << "  --preserve-names, --p  Keep function names in generated C++ (default: mangle)\n";
    std::cout << "  --small       Optimize for smaller executable (-Os)\n";
    std::cout << "  --dll     Build Windows .dll (default: -Os + strip for smaller .dll)\n";
    std::cout << "  --shared  Build .dylib (macOS) or .so (Linux; default: -Os + strip)\n";
    std::cout << "  --static-lib  Build a static archive (.a Linux / .lib Windows) from a .nxa\n";
    std::cout << "  --link <file>  Statically link an archive/object/lib into the executable\n";
    std::cout << "                 (repeatable; .a/.o are baked in, .so/.dll link dynamically)\n";
    std::cout << "  --win     Build Windows .exe (mingw-w64 from Linux; native on Windows)\n";
    std::cout << "  --wasm    Build WebAssembly (em++ → .js+.wasm, or WASI .wasm).\n";
    std::cout << "            If em++ is missing, a popup offers to install Emscripten.\n";
    std::cout << "  --no-console  Build Windows GUI .exe (no console window)\n";
    std::cout << "  --help, -h    Show this help\n";
    std::cout << "  --version, --v, -v  Show version\n";
    std::cout << "  --help <page>  Show page (2=core ... 8=std/thread, 9=std/inline)\n";
    std::cout << "  --help --pageN  Same (e.g. --page2, --page3)\n\n";
    std::cout << "Standard library modules:\n";
    std::cout << "  std/io        Input/output: print, println, readln, getline, trim, to_int\n";
    std::cout << "  std/os        System calls: system\n";
    std::cout << "  std/dll       Dynamic libraries: load, call\n";
    std::cout << "  std/file      File I/O: read, write, append, exists\n";
    std::cout << "  std/random   Random: int, seed\n";
    std::cout << "  std/math      Math: abs, min, max, pow, sqrt, floor, ceil, round, sin, cos, tan, log, exp, pi, e\n";
    std::cout << "  std/thread   Threads: spawn, join\n";
    std::cout << "  std/inline   inline_cpp! { ... } embed C++ (requires include)\n";
    std::cout << "  core          Core language: variables, types, fn main, functions\n\n";
    std::cout << "Example:\n";
    std::cout << "  NexaC program.nxa -o program\n";
    std::cout << "  NexaC --help --page2  NexaC --help std/io\n\n";
    std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/thread | 9=std/inline\n";
	return 0;
}

int main(int argc, char* argv[]) {
    // nexapkg: invoked as "nexapkg" or "nexapkg.exe" or "NexaC nexapkg <cmd>"
    std::string exe = argc >= 1 ? argv[0] : "";
    size_t lastSlash = exe.find_last_of("/\\");
    std::string exeName = (lastSlash != std::string::npos) ? exe.substr(lastSlash + 1) : exe;
    bool exeIsNexapkg = (exeName == "nexapkg" || (exeName.size() >= 12 && exeName.substr(0, 7) == "nexapkg" && exeName.substr(exeName.size() - 4) == ".exe"));
    bool argIsNexapkg = (argc >= 2 && std::string(argv[1]) == "nexapkg");
    if (exeIsNexapkg) {
        return nexa::pkg::run(argc, argv);
    }
    if (argIsNexapkg) {
        return nexa::pkg::run(argc - 1, argv + 1);
    }
    if (argc >= 2 && std::string(argv[1]) == "init") {
        std::string dir = (argc >= 3) ? argv[2] : "";
        return doInit(dir);
    }
    if (argc >= 2 && std::string(argv[1]) == "build") {
        std::string dir = (argc >= 3) ? argv[2] : "";
        return doBuild(dir);
    }

    std::string inputPath;
    std::string outputExe;
    std::string sourceCpp;
    bool sourceOnly = false;
    bool preserveNames = false;
    bool optimizeSize = false;
    bool buildDll = false;   // mingw -> .dll
    bool buildShared = false;  // clang++ -> .so
    bool buildStaticLib = false;  // -> .a (Linux) / .lib (Windows) static archive
    bool buildWin = false;   // mingw -> .exe (cross-compile from Linux)
    bool buildWasm = false;  // em++ / WASI -> .js+.wasm or .wasm
    bool noConsole = false;  // Windows GUI subsystem
    bool runAfterBuild = false;
    bool pendingSourceOut = false;
    std::vector<std::string> linkInputs;  // extra objects/archives/libs to link into the exe (--link)

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::string nextArg;
            if (i + 1 < argc) {
                nextArg = argv[i + 1];
                i++;
            }
            int page = helpPageFromArg(nextArg);
            if (page == 0 && !nextArg.empty()) {
                std::cerr << "Unknown help page/module: " << nextArg << "\n";
                std::cerr << "Use 'NexaC --help' to list pages (1-9) and modules (std/io, std/os, std/dll, std/file, std/random, std/thread, std/inline, core).\n";
                return 1;
            }
            return printHelp(page);
        } else if (arg == "--version" || arg == "--v" || arg == "-v") {
            std::cout << "NexaC " << NEXAC_VERSION << "\n";
            return 0;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "[Nexa] Error: -o requires a filename\n";
                return 1;
            }
            std::string outPath = argv[++i];
            // -o foo.cpp -> emit C++ source (same as --source foo.cpp)
            if ((outPath.size() >= 4 && outPath.substr(outPath.size() - 4) == ".cpp") ||
                    (outPath.size() >= 3 && outPath.substr(outPath.size() - 3) == ".cc") ||
                    (outPath.size() >= 5 && outPath.substr(outPath.size() - 5) == ".cxx")) {
                sourceCpp = outPath;
                sourceOnly = true;
                pendingSourceOut = false;
            } else {
                outputExe = outPath;
            }
        } else if (arg == "--source") {
            pendingSourceOut = true;
        } else if (arg == "--preserve-names" || arg == "--p" || arg == "-p") {
            preserveNames = true;
        } else if (arg == "--small") {
            optimizeSize = true;
        } else if (arg == "--dll") {
            buildDll = true;
        } else if (arg == "--shared") {
            buildShared = true;
        } else if (arg == "--static-lib" || arg == "--staticlib" || arg == "--lib" || arg == "--a") {
            buildStaticLib = true;
        } else if (arg == "--link") {
            if (i + 1 >= argc) {
                std::cerr << "[Nexa] Error: --link requires a file (e.g. --link libfoo.a)\n";
                return 1;
            }
            linkInputs.push_back(argv[++i]);
        } else if (arg == "--win" || arg == "--windows") {
            buildWin = true;
        } else if (arg == "--wasm" || arg == "--wasm32") {
            buildWasm = true;
        } else if (arg == "--no-console") {
            noConsole = true;
        } else if (arg == "--run" || arg == "-r") {
            runAfterBuild = true;
        } else if (arg[0] != '-') {
            if (pendingSourceOut) {
                sourceCpp = arg;
                sourceOnly = true;
                pendingSourceOut = false;
            } else {
                inputPath = arg;
            }
        }
    }

    if (pendingSourceOut) {
        std::cerr << "[Nexa] Error: --source requires an output .cpp path (options like --p may go before the path)\n";
        return 1;
    }

    if (inputPath.empty()) {
        if (runAfterBuild) {
            std::string entry = findEntryFile(std::filesystem::current_path());
            if (!entry.empty()) inputPath = entry;
        }
        if (inputPath.empty()) {
            std::cerr << "Usage: NexaC init [dir]  |  NexaC build [dir]  |  NexaC <file.nxa> [-o <exe>]\n";
            std::cerr << "       NexaC <file.nxa> --source <output.cpp>\n";
            std::cerr << "       NexaC <file.nxa> --run  |  NexaC --run (in project dir)\n";
            std::cerr << "Example: NexaC init  |  NexaC build  |  NexaC --run\n";
            return 1;
        }
    }

    {
        std::filesystem::path inPath(inputPath);
        std::string ext = inPath.extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".nxa") {
            std::cerr << "[Nexa] Error: Input must be a .nxa file (got: " << inputPath << ")\n";
            std::cerr << "[Nexa] Tip: Rename your source to .nxa, then run: NexaC <file.nxa> [options]\n";
            return 1;
        }
    }

    if (runAfterBuild && (buildDll || buildShared || buildWin)) {
        std::cerr << "[Nexa] Error: --run cannot be used with --dll, --shared, or --win\n";
        return 1;
    }
    if (buildWasm && (buildDll || buildShared || buildWin || buildStaticLib)) {
        std::cerr << "[Nexa] Error: --wasm cannot be combined with --dll, --shared, --win, or --static-lib\n";
        return 1;
    }
    if (buildWasm && noConsole) {
        std::cerr << "[Nexa] Error: --no-console is only valid for native Windows executables, not --wasm\n";
        return 1;
    }
    if (buildDll && buildShared) {
        std::cerr << "[Nexa] Error: --dll and --shared are mutually exclusive\n";
        return 1;
    }
    if ((buildDll || buildShared) && buildWin) {
        std::cerr << "[Nexa] Error: --win cannot be used with --dll or --shared\n";
        return 1;
    }
    if (noConsole && (buildDll || buildShared)) {
        std::cerr << "[Nexa] Error: --no-console is only valid for executables, not --dll/--shared\n";
        return 1;
    }
    if (noConsole && !buildWin) {
#ifndef _WIN32
        std::cerr << "[Nexa] Error: --no-console requires --win when compiling from non-Windows hosts\n";
        return 1;
#endif
    }
    if (buildStaticLib && (buildDll || buildShared || buildWin)) {
        std::cerr << "[Nexa] Error: --static-lib cannot be combined with --dll, --shared, or --win\n";
        return 1;
    }
    if (buildStaticLib && runAfterBuild) {
        std::cerr << "[Nexa] Error: --run cannot be used with --static-lib (a library is not executable)\n";
        return 1;
    }
    if (buildStaticLib && noConsole) {
        std::cerr << "[Nexa] Error: --no-console is only valid for executables, not --static-lib\n";
        return 1;
    }
    if (!linkInputs.empty() && buildStaticLib) {
        std::cerr << "[Nexa] Error: --link cannot be used with --static-lib (archives just bundle objects, they do not link)\n";
        return 1;
    }

    WasmTool wasmTool;
    if (buildWasm && !sourceOnly) {
        if (!nexaEnsureWasmTool(wasmTool)) return 1;
    }

    std::string cppPath;
    std::string exePath;
    bool useTempExe = runAfterBuild;

    if (sourceOnly) {
        cppPath = sourceCpp;
        exePath = "";
    } else {
        std::string pidStr;
#ifdef _WIN32
        pidStr = std::to_string(GetCurrentProcessId());
#else
        pidStr = std::to_string(getpid());
#endif
        cppPath = (std::filesystem::temp_directory_path() / ("neaxc_" + pidStr + ".cpp")).string();
        if (useTempExe) {
            exePath = (std::filesystem::temp_directory_path() / ("neaxc_" + pidStr)).string();
            if (buildWasm) {
                exePath += (wasmTool.kind == WasmKind::Emscripten) ? ".js" : ".wasm";
            } else {
#ifdef _WIN32
                exePath += ".exe";
#endif
            }
        } else if (outputExe.empty()) {
            exePath = inputPath;
            if (exePath.size() >= 4 && exePath.substr(exePath.size() - 4) == ".nxa") {
                exePath = exePath.substr(0, exePath.size() - 4);
            } else {
                exePath += "_out";
            }
            if (buildWasm) {
                exePath += (wasmTool.kind == WasmKind::Emscripten) ? ".js" : ".wasm";
            } else if (buildDll) {
                exePath += ".dll";
            } else if (buildShared) {
#ifdef __APPLE__
                exePath += ".dylib";
#else
                exePath += ".so";
#endif
            } else if (buildStaticLib) {
#ifdef _WIN32
                exePath += ".lib";
#else
                exePath += ".a";
#endif
            } else if (buildWin) {
                exePath += ".exe";
            }
        } else {
            exePath = outputExe;
            if (buildDll && (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".dll")) {
                exePath += ".dll";
            } else if (buildShared
#ifdef __APPLE__
                       && (exePath.size() < 6 || exePath.substr(exePath.size() - 6) != ".dylib")) {
                exePath += ".dylib";
#else
                       && (exePath.size() < 3 || exePath.substr(exePath.size() - 3) != ".so")) {
                exePath += ".so";
#endif
            } else if (buildStaticLib) {
#ifdef _WIN32
                if (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".lib") exePath += ".lib";
#else
                if (exePath.size() < 2 || exePath.substr(exePath.size() - 2) != ".a") exePath += ".a";
#endif
            }
            if (buildWin && (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".exe")) {
                exePath += ".exe";
            }
#ifdef _WIN32
            else if (!buildDll && !buildShared && !buildWasm && (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".exe")) {
                exePath += ".exe";
            }
#endif
            if (buildWasm) {
                std::string ext = std::filesystem::path(exePath).extension().string();
                for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (ext.empty()) {
                    exePath += (wasmTool.kind == WasmKind::Emscripten) ? ".js" : ".wasm";
                }
            }
        }
    }

    std::ifstream in(inputPath);
    if (!in) {
        std::cerr << "[Nexa] Error: Cannot open " << inputPath << "\n";
        return 1;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    std::string source = buf.str();
    in.close();

    try {
        std::cout << "[Nexa] Parsing...\n";

        nexa::Lexer lexer(source);
        std::vector<nexa::Token> tokens = lexer.tokenize();

        nexa::Modules modules;
        std::string absInputPath = std::filesystem::absolute(std::filesystem::path(inputPath)).string();
        std::set<std::string> includedFiles;
        includedFiles.insert(absInputPath);  // prevent main from being included (circular)
        std::vector<std::string> packagePaths;
        {
            namespace fs = std::filesystem;
            fs::path inputDir = fs::path(absInputPath).parent_path();
            packagePaths.push_back((inputDir / ".nexa" / "packages").string());
        }
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (home) packagePaths.push_back(std::string(home) + "/.nexa/packages");
        nexa::Parser parser(std::move(tokens), modules, absInputPath, &includedFiles, &packagePaths);
        std::vector<nexa::AstNode> ast = parser.parse();

        std::cout << "[Nexa] Transpiling...\n";

        bool isLib = buildDll || buildShared || buildStaticLib;
        nexa::Transpiler transpiler(ast, modules, preserveNames || isLib, isLib);  // library: preserve + export C names
        std::string cpp = transpiler.transpile();

        // Decide which C++ machinery the generated code can safely omit. Exceptions/unwind tables
        // are only needed for try/catch, throw, std::stoi (io.to_int), or inline_cpp (arbitrary C++).
        // RTTI is never emitted by the transpiler, so it is dropped unless inline_cpp is present.
        const nexa::Modules::CppUsage& usage = transpiler.cppUsage();
        const bool noExceptions = !usage.exceptions && !usage.ioToInt && !modules.hasInlineCpp();
        const bool noRtti = !modules.hasInlineCpp();

        std::ofstream out(cppPath);
        if (!out) {
            std::cerr << "[Nexa] Error: Cannot write " << cppPath << "\n";
            return 1;
        }
        out << cpp;
        out.close();

        if (sourceOnly) {
            std::cout << "[Nexa] Source written to " << cppPath << "\n";
            return 0;
        }

        std::string wasmOut = exePath;
        if (buildWasm && wasmTool.kind == WasmKind::Emscripten && nexaHasExt(exePath, ".wasm")) {
            wasmOut = std::filesystem::path(exePath).replace_extension(".js").string();
        }
        if (buildWasm && wasmTool.kind == WasmKind::Wasi) {
            if (modules.hasHttp() && usage.http) {
                std::remove(cppPath.c_str());
                std::cerr << "[Nexa] Error: std/http on WASM requires Emscripten (em++). Install the emsdk and retry --wasm.\n";
                return 1;
            }
            if (modules.hasThread() && usage.thread) {
                std::remove(cppPath.c_str());
                std::cerr << "[Nexa] Error: std/thread on WASM requires Emscripten (em++ -pthread). Install the emsdk and retry --wasm.\n";
                return 1;
            }
        }

        std::string cxx;
        std::string targetFlags;
        if (buildWasm) {
            const char* kindName = (wasmTool.kind == WasmKind::Emscripten) ? "Emscripten" : "WASI";
            std::cout << "[Nexa] Compiling with " << wasmTool.cxx << " (" << kindName << " WASM)...\n";
        } else if (buildDll) {
#ifdef _WIN32
            cxx = "clang++";
#else
            cxx = findWindowsCxx();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: clang++ or mingw-w64 required for --dll. Install: apt install clang mingw-w64\n";
                return 1;
            }
            if (cxx.find("clang") != std::string::npos) {
                targetFlags = " -target x86_64-w64-mingw32";
            }
#endif
            std::cout << "[Nexa] Compiling with " << cxx << " (Windows DLL)...\n";
        } else if (buildShared) {
            cxx = "clang++";
            std::cout << "[Nexa] Compiling with clang++ (shared library)...\n";
        } else if (buildStaticLib) {
#ifdef _WIN32
            cxx = findWindowsCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found. Install clang (LLVM) or MinGW (g++/gcc).\n";
                return 1;
            }
#else
            cxx = "clang++";
#endif
            std::cout << "[Nexa] Compiling with " << cxx << " (static library)...\n";
        } else if (buildWin) {
#ifdef _WIN32
            cxx = "clang++";
            std::cout << "[Nexa] Compiling with clang++ (Windows exe)...\n";
#else
            cxx = findWindowsCxx();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: clang++ or mingw-w64 required for --win. Install: apt install clang mingw-w64\n";
                return 1;
            }
            if (cxx.find("clang") != std::string::npos) {
                targetFlags = " -target x86_64-w64-mingw32";
            }
            std::cout << "[Nexa] Compiling with " << cxx << " (Windows exe)...\n";
#endif
        } else {
#ifdef _WIN32
            cxx = findWindowsCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found. Install clang (LLVM) or MinGW (g++/gcc).\n";
                std::cerr << "  - LLVM: https://releases.llvm.org/\n";
                std::cerr << "  - MinGW: https://www.mingw-w64.org/ or use MSYS2: msys2.org\n";
                return 1;
            }
            std::cout << "[Nexa] Compiling with " << cxx << "...\n";
#else
            cxx = "clang++";
            std::cout << "[Nexa] Compiling with clang++...\n";
#endif
        }

        std::cout.flush();  // ensure [Nexa] lines appear before child compiler output (e.g. when stdout is redirected)
        // Shared libraries (.dll / .so): default to -Os; use --small for exes, or NEXA_CXXFLAGS=-O2
        // if you need speed on a specific library.
        std::string opt = (optimizeSize || buildDll || buildShared) ? "-Os" : "-O2";
        const bool linkUser32 = modules.hasOs() || modules.hasInlineCpp();
        const bool linkHttp = modules.hasHttp();

        if (buildWasm) {
            std::string cmd = nexaWasmCompileCmd(wasmTool, cppPath, wasmOut, opt, noExceptions, noRtti,
                modules.hasHttp() && usage.http, modules.hasThread() && usage.thread, linkInputs);
            int ret = std::system(cmd.c_str());
            std::remove(cppPath.c_str());
            if (ret != 0) {
                std::cerr << "[Nexa] Compilation failed.\n";
                return 1;
            }
            std::cout << "[Nexa] Build successful!\n";
            if (wasmTool.kind == WasmKind::Emscripten) {
                std::string side = std::filesystem::path(wasmOut).replace_extension(".wasm").string();
                std::cout << "[Nexa] Loader: " << wasmOut << "\n";
                std::cout << "[Nexa] Module: " << side << "\n";
                std::cout << "[Nexa] Run: node \"" << wasmOut << "\"  (or include the .js from a page)\n";
            } else {
                std::cout << "[Nexa] Module: " << wasmOut << "\n";
                std::cout << "[Nexa] Run: wasmtime \"" << wasmOut << "\"\n";
            }
            std::cout.flush();
            if (runAfterBuild) {
                int runRet = runWasmOutput(wasmOut, wasmTool.kind);
                std::remove(wasmOut.c_str());
                if (wasmTool.kind == WasmKind::Emscripten) {
                    std::remove(std::filesystem::path(wasmOut).replace_extension(".wasm").string().c_str());
                }
#ifdef _WIN32
                return runRet;
#else
                return WIFEXITED(runRet) ? WEXITSTATUS(runRet) : 127;
#endif
            }
            return 0;
        }

        if (buildStaticLib) {
            std::string objPath = cppPath.substr(0, cppPath.size() - 4) + ".o";
            std::string cmd = nexaStaticLibCmd(cxx, cppPath, objPath, exePath, opt, noExceptions, noRtti);
            int ret = std::system(cmd.c_str());
            std::remove(cppPath.c_str());
            std::remove(objPath.c_str());
            if (ret != 0) {
                std::cerr << "[Nexa] Static library build failed.\n";
                return 1;
            }
            std::cout << "[Nexa] Build successful! Static library: " << exePath << "\n";
            std::cout << "[Nexa] Link it into an executable with: NexaC <main.nxa> --link \"" << exePath << "\" -o <exe>\n";
            std::cout << "[Nexa] Call its exported functions from main via inline_cpp (declare 'extern \"C\" ...').\n";
            return 0;
        }

        std::string cmd = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll(), noConsole, linkUser32, linkHttp, noExceptions, noRtti, linkInputs);
        int ret = std::system(cmd.c_str());

        if (ret != 0) {
            std::string fallback;
#ifdef _WIN32
            const char* next[] = {"clang++", "clang", "g++", "gcc"};
            for (const char* n : next) {
                if (std::string(n) != cxx) {
                    std::string cmd = "where ";
                    cmd += n;
                    cmd += " >nul 2>&1";
                    if (std::system(cmd.c_str()) == 0) {
                        fallback = n;
                        break;
                    }
                }
            }
            if (!fallback.empty()) std::cout << "[Nexa] " << cxx << " failed, retrying with " << fallback << "...\n";
#else
            if (!targetFlags.empty()) {
                fallback = findMingwCxx();
                if (!fallback.empty()) std::cout << "[Nexa] clang++ failed, retrying with mingw-g++...\n";
            }
#endif
            if (!fallback.empty()) {
                cxx = fallback;
                targetFlags = "";
                std::cout.flush();
                std::string cmd2 = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll(), noConsole, linkUser32, linkHttp, noExceptions, noRtti, linkInputs);
                ret = std::system(cmd2.c_str());
            }
        }

        std::remove(cppPath.c_str());

        if (ret != 0) {
            std::cerr << "[Nexa] Compilation failed.\n";
            return 1;
        }

        std::cout << "[Nexa] Build successful!\n";

        if (runAfterBuild) {
            int runRet = std::system(("\"" + exePath + "\"").c_str());
            std::remove(exePath.c_str());
#ifdef _WIN32
            return runRet;
#else
            return WIFEXITED(runRet) ? WEXITSTATUS(runRet) : 127;
#endif
        }
        return 0;
    } catch (const std::exception& e) {
        if (!cppPath.empty() && !sourceOnly) {
            std::remove(cppPath.c_str());
            if (runAfterBuild && !exePath.empty()) std::remove(exePath.c_str());
        }
        std::cerr << "[Nexa] Error: " << e.what() << "\n";
        return 1;
    }
}
