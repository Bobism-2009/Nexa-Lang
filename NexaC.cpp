#include "include/Lexer.hpp"
#include "include/Parser.hpp"
#include "include/Transpiler.hpp"
#include "include/Modules.hpp"
#include "include/nexapkg.hpp"
#include "include/NexaUpgrade.hpp"
#include "include/Help.hpp"
#include "include/Target.hpp"

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

#define NEXAC_VERSION "0.1.15"

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

#ifdef _WIN32
static void nexaRefreshWindowsCompilerPath();
#endif
static bool nexaHasPython();

// On Windows native: find clang, g++, or gcc (MinGW/MSYS2). Returns compiler name or empty.
static std::string findWindowsCxxNative() {
#ifdef _WIN32
    nexaRefreshWindowsCompilerPath();
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

// On Linux/macOS: pick the native C++ compiler. NEXA_CXX overrides (a name on PATH or a
// full path); otherwise prefer clang++, then g++ (same order as the Makefile). Returns
// compiler name or empty.
static std::string findUnixCxxNative() {
#ifdef _WIN32
    return "";
#else
    if (const char* env = std::getenv("NEXA_CXX")) {
        if (*env) return env;
    }
    const char* candidates[] = {"clang++", "g++"};
    for (const char* cxx : candidates) {
        std::string cmd = "command -v ";
        cmd += cxx;
        cmd += " >/dev/null 2>&1";
        if (std::system(cmd.c_str()) == 0)
            return cxx;
    }
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

static std::string nexaPopenAll(const std::string& cmd) {
#ifdef _WIN32
    FILE* f = _popen(cmd.c_str(), "r");
#else
    FILE* f = popen(cmd.c_str(), "r");
#endif
    if (!f) return "";
    std::string s;
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) s += buf;
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    return s;
}

static void nexaPrependProcessPath(const std::string& dir) {
    if (dir.empty() || !nexaPathExists(dir)) return;
    const char* cur = std::getenv("PATH");
    std::string path = cur ? cur : "";
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::string token = dir;
    if (path.find(token) != std::string::npos) return;
    std::string next = token + sep + path;
#ifdef _WIN32
    SetEnvironmentVariableA("PATH", next.c_str());
    _putenv_s("PATH", next.c_str());
#else
    setenv("PATH", next.c_str(), 1);
#endif
}

#ifdef _WIN32
static void nexaAddDirIfExe(const std::filesystem::path& dir, const char* exe) {
    std::error_code ec;
    if (std::filesystem::exists(dir / exe, ec)) {
        nexaPrependProcessPath(dir.string());
    }
}

static void nexaScanForExe(const std::filesystem::path& root, const char* exe, int depth) {
    std::error_code ec;
    if (depth < 0 || !std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) return;
    nexaAddDirIfExe(root, exe);
    if (depth == 0) return;
    for (std::filesystem::directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (it->is_directory(ec)) nexaScanForExe(it->path(), exe, depth - 1);
    }
}

// Pick up winget/LLVM/MinGW locations that `where` misses (legacy C:\mingw64 vs WinGet Packages).
// Cheap on the common path: WinGet Links + known dirs. Recurse Packages only if still missing.
static void nexaRefreshWindowsCompilerPath() {
    const char* la = std::getenv("LOCALAPPDATA");
    const char* pf = std::getenv("ProgramFiles");
    const char* pfx = std::getenv("ProgramFiles(x86)");
    if (la && la[0]) {
        std::filesystem::path local(la);
        nexaPrependProcessPath((local / "Microsoft" / "WinGet" / "Links").string());
        nexaAddDirIfExe(local / "Programs" / "LLVM" / "bin", "clang++.exe");
        nexaAddDirIfExe(local / "Programs" / "WinLibs" / "mingw64" / "bin", "g++.exe");
    }
    if (pf && pf[0]) {
        nexaAddDirIfExe(std::filesystem::path(pf) / "LLVM" / "bin", "clang++.exe");
    }
    if (pfx && pfx[0]) {
        nexaAddDirIfExe(std::filesystem::path(pfx) / "LLVM" / "bin", "clang++.exe");
    }
    nexaAddDirIfExe("C:\\mingw64\\bin", "g++.exe");
    nexaAddDirIfExe("C:\\WinLibs\\mingw64\\bin", "g++.exe");
    nexaAddDirIfExe("C:\\tools\\winlibs\\mingw64\\bin", "g++.exe");
    nexaAddDirIfExe("C:\\msys64\\ucrt64\\bin", "g++.exe");
    nexaAddDirIfExe("C:\\msys64\\mingw64\\bin", "g++.exe");
    if (la && la[0] && !nexaCmdExists("g++") && !nexaCmdExists("clang++") && !nexaCmdExists("g++.exe")) {
        std::filesystem::path pkgs = std::filesystem::path(la) / "Microsoft" / "WinGet" / "Packages";
        nexaScanForExe(pkgs, "g++.exe", 5);
        if (!nexaCmdExists("g++") && !nexaCmdExists("g++.exe")) nexaScanForExe(pkgs, "clang++.exe", 5);
    }
}

static void nexaRefreshWindowsToolPath() {
    nexaRefreshWindowsCompilerPath();
    const char* la = std::getenv("LOCALAPPDATA");
    const char* pf = std::getenv("ProgramFiles");
    if (la && la[0]) {
        std::filesystem::path local(la);
        nexaAddDirIfExe(local / "Programs" / "Git" / "cmd", "git.exe");
        nexaAddDirIfExe(local / "Programs" / "Python" / "Python312", "python.exe");
        nexaAddDirIfExe(local / "Programs" / "Python" / "Python313", "python.exe");
        nexaAddDirIfExe(local / "Programs" / "Python" / "Python314", "python.exe");
        nexaAddDirIfExe(local / "Programs" / "Python" / "Launcher", "py.exe");
        nexaScanForExe(local / "Programs" / "Python", "python.exe", 2);
        nexaAddDirIfExe(local / "Programs" / "nodejs", "node.exe");
    }
    if (pf && pf[0]) {
        std::filesystem::path prog(pf);
        nexaAddDirIfExe(prog / "Git" / "cmd", "git.exe");
        nexaAddDirIfExe(prog / "nodejs", "node.exe");
        nexaScanForExe(prog / "Python312", "python.exe", 1);
        nexaScanForExe(prog / "Python313", "python.exe", 1);
    }
    if (la && la[0]) {
        std::filesystem::path pkgs = std::filesystem::path(la) / "Microsoft" / "WinGet" / "Packages";
        if (!nexaCmdExists("git")) nexaScanForExe(pkgs, "git.exe", 5);
        if (!nexaHasPython()) nexaScanForExe(pkgs, "python.exe", 5);
        if (!nexaCmdExists("node")) nexaScanForExe(pkgs, "node.exe", 5);
    }
}

static bool nexaWingetHasId(const std::string& id) {
    if (!nexaCmdExists("winget")) return false;
    std::string out = nexaPopenAll("winget list -e --id " + id + " --disable-interactivity 2>nul");
    if (out.find("No installed package found") != std::string::npos) return false;
    return out.find(id) != std::string::npos;
}

// Install only if the package is not already present. Never upgrades.
static bool nexaWingetInstallMissing(const std::string& id) {
    if (nexaWingetHasId(id)) return true;
    if (!nexaCmdExists("winget")) return false;
    std::cout << "[Nexa] Installing " << id << " (missing; will not upgrade existing tools)...\n";
    std::cout.flush();
    std::string cmd = "winget install --id " + id +
        " -e --accept-package-agreements --accept-source-agreements --disable-interactivity --no-upgrade --scope user";
    if (std::system(cmd.c_str()) == 0) {
        nexaRefreshWindowsToolPath();
        return true;
    }
    cmd = "winget install --id " + id +
        " -e --accept-package-agreements --accept-source-agreements --disable-interactivity --no-upgrade";
    int rc = std::system(cmd.c_str());
    nexaRefreshWindowsToolPath();
    return rc == 0;
}
#endif

// `where python` hits the Windows Store stub and is not a real interpreter.
static bool nexaHasPython() {
#ifdef _WIN32
    if (std::system("python -c \"import sys\" >nul 2>&1") == 0) return true;
    if (std::system("py -3 -c \"import sys\" >nul 2>&1") == 0) return true;
    if (std::system("python3 -c \"import sys\" >nul 2>&1") == 0) return true;
    return false;
#else
    return std::system("python3 -c \"import sys\" >/dev/null 2>&1") == 0
        || std::system("python -c \"import sys\" >/dev/null 2>&1") == 0;
#endif
}

// When the user asked NexaC to fetch a toolchain, install missing host tools.
// Skip anything already on PATH or already registered with winget.
static bool nexaEnsureHostTool(const std::string& cmd, const char* wingetId, const char* aptPkg) {
#ifdef _WIN32
    nexaRefreshWindowsToolPath();
#endif
    if (cmd == "python" || cmd == "python3") {
        if (nexaHasPython()) return true;
    } else if (nexaCmdExists(cmd)) {
        return true;
    }
#ifdef _WIN32
    if (wingetId && wingetId[0]) {
        if (!nexaWingetInstallMissing(wingetId)) return false;
        nexaRefreshWindowsToolPath();
        if (cmd == "python" || cmd == "python3") return nexaHasPython();
        return nexaCmdExists(cmd);
    }
    return false;
#else
    (void)wingetId;
    if (!aptPkg || !aptPkg[0]) return false;
#if defined(__APPLE__)
    if (nexaCmdExists("brew")) {
        std::cout << "[Nexa] Installing " << aptPkg << " with Homebrew...\n";
        std::cout.flush();
        return std::system((std::string("brew list ") + aptPkg + " >/dev/null 2>&1 || brew install " + aptPkg).c_str()) == 0
            && nexaCmdExists(cmd);
    }
    return false;
#else
    if (nexaCmdExists("apt-get")) {
        std::cout << "[Nexa] Installing " << aptPkg << "...\n";
        std::cout.flush();
        return std::system((std::string("sudo apt-get install -y --no-upgrade ") + aptPkg).c_str()) == 0
            && nexaCmdExists(cmd);
    }
    if (nexaCmdExists("dnf")) {
        std::string check = std::string("rpm -q ") + aptPkg + " >/dev/null 2>&1";
        if (std::system(check.c_str()) == 0) return nexaCmdExists(cmd);
        return std::system((std::string("sudo dnf install -y ") + aptPkg).c_str()) == 0
            && nexaCmdExists(cmd);
    }
    return false;
#endif
#endif
}

#if !defined(_WIN32) && !defined(__APPLE__)
static std::string nexaPopenTrim(const std::string& cmd) {
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return "";
    char buf[1024];
    std::string s;
    if (fgets(buf, sizeof(buf), f)) s = buf;
    pclose(f);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    return s;
}

static void nexaLinuxCollectLibDirs(std::vector<std::string>& dirs) {
    auto add = [&](const std::string& d) {
        if (d.empty()) return;
        for (const auto& e : dirs) {
            if (e == d) return;
        }
        if (nexaPathExists(d)) dirs.push_back(d);
    };
    std::string ma = nexaPopenTrim("gcc -print-multiarch 2>/dev/null");
    if (ma.empty()) ma = nexaPopenTrim("clang -print-multiarch 2>/dev/null");
    if (!ma.empty()) {
        add("/usr/lib/" + ma);
        add("/lib/" + ma);
    }
    add(nexaPopenTrim("pkg-config --variable=libdir x11 2>/dev/null"));
    add("/usr/lib/x86_64-linux-gnu");
    add("/usr/lib/aarch64-linux-gnu");
    add("/usr/lib/arm-linux-gnueabihf");
    add("/usr/lib64");
    add("/usr/lib");
    add("/usr/local/lib");
}

static std::string nexaFindStaticLib(const std::string& name, const std::vector<std::string>& dirs) {
    const std::string fn = "lib" + name + ".a";
    for (const auto& d : dirs) {
        std::string p = d + "/" + fn;
        if (nexaPathExists(p)) return p;
    }
    return "";
}

// Embed X11 into a Linux ELF executable (portable; no libX11.so on the target).
// libc/libpthread/libdl stay dynamic — those are on every Linux.
static std::string nexaLinuxGfxEmbedFlags() {
    std::vector<std::string> dirs;
    nexaLinuxCollectLibDirs(dirs);
    const char* names[] = {"X11", "xcb", "Xau", "Xdmcp"};
    std::string out;
    bool allFound = true;
    for (const char* n : names) {
        std::string p = nexaFindStaticLib(n, dirs);
        if (p.empty()) {
            allFound = false;
            break;
        }
        out += " \"";
        out += p;
        out += "\"";
    }
    if (!allFound) {
        // Linker still prefers .a when -Bstatic is set, if the archives are on -L paths.
        out = " -Wl,-Bstatic -lX11 -lxcb -lXau -lXdmcp -Wl,-Bdynamic";
    }
    out += " -lpthread -ldl";
    return out;
}
#endif

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

static bool nexaEmsdkHasToolchain(const std::string& dest) {
    std::filesystem::path em = std::filesystem::path(dest) / "upstream" / "emscripten";
#ifdef _WIN32
    return nexaPathExists((em / "em++.bat").string()) || nexaPathExists((em / "em++.exe").string())
        || nexaPathExists((em / "em++").string());
#else
    return nexaPathExists((em / "em++").string());
#endif
}

static void nexaPrependEmsdkRuntime(const std::string& dest) {
    if (dest.empty() || !nexaPathExists(dest)) return;
#ifdef _WIN32
    SetEnvironmentVariableA("EMSDK", dest.c_str());
    _putenv_s("EMSDK", dest.c_str());
#else
    setenv("EMSDK", dest.c_str(), 1);
#endif
    std::filesystem::path root(dest);
    nexaPrependProcessPath((root / "upstream" / "emscripten").string());
    std::error_code ec;
    std::filesystem::path nodeRoot = root / "node";
    if (std::filesystem::exists(nodeRoot, ec) && std::filesystem::is_directory(nodeRoot, ec)) {
        for (std::filesystem::directory_iterator it(nodeRoot, ec), end; it != end && !ec; it.increment(ec)) {
            if (!it->is_directory(ec)) continue;
#ifdef _WIN32
            nexaAddDirIfExe(it->path(), "node.exe");
            nexaAddDirIfExe(it->path() / "bin", "node.exe");
#else
            nexaPrependProcessPath((it->path() / "bin").string());
#endif
        }
    }
}

static bool nexaInstallEmsdk(const std::string& dest) {
    if (dest.empty()) {
        nexaAlert("NexaC", "Cannot install Emscripten: no user home directory.");
        return false;
    }
#ifdef _WIN32
    nexaRefreshWindowsToolPath();
#endif

#ifdef _WIN32
    const std::string launcher = (std::filesystem::path(dest) / "emsdk.bat").string();
#else
    const std::string launcher = (std::filesystem::path(dest) / "emsdk").string();
#endif

    // Already have a working SDK in ~/emsdk — reuse it (do not pull "latest").
    if (nexaEmsdkHasToolchain(dest)) {
        std::cout << "[Nexa] Using existing Emscripten in " << dest << " (not upgrading).\n";
        std::cout.flush();
        nexaPrependEmsdkRuntime(dest);
        return true;
    }

    if (!nexaEnsureHostTool("git", "Git.Git", "git")) {
        nexaAlert("NexaC",
            "Git is required to install the WASM toolchain, and NexaC could not install it.\n"
            "Install Git, then retry --wasm.");
        return false;
    }
    if (!nexaEnsureHostTool("python", "Python.Python.3.12", "python3") &&
        !nexaHasPython()) {
        nexaAlert("NexaC",
            "Python is required by emsdk, and NexaC could not install it.\n"
            "Install Python 3, then retry --wasm.");
        return false;
    }

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

    if (nexaEmsdkHasToolchain(dest)) {
        nexaPrependEmsdkRuntime(dest);
        return true;
    }

    std::cout << "[Nexa] Installing Emscripten (first-time download; later --wasm reuses this)...\n";
    std::cout.flush();
    std::string install = "\"" + launcher + "\" install latest";
    std::string activate = "\"" + launcher + "\" activate latest";
    if (std::system(install.c_str()) != 0) {
        nexaAlert("NexaC", "emsdk install failed. See the console output for details.");
        return false;
    }
    if (std::system(activate.c_str()) != 0) {
        nexaAlert("NexaC", "emsdk activate failed. See the console output for details.");
        return false;
    }
    nexaPrependEmsdkRuntime(dest);
    return true;
}

static bool nexaEnsureWasmTool(WasmTool& tool) {
#ifdef _WIN32
    nexaRefreshWindowsToolPath();
#endif
    {
        const std::string dest = nexaDefaultEmsdkDir();
        if (!dest.empty()) nexaPrependEmsdkRuntime(dest);
    }
    tool = findWasmCxx();
    if (tool.kind != WasmKind::None) {
        nexaPrependEmsdkRuntime(nexaDefaultEmsdkDir());
        return true;
    }

    const std::string dest = nexaDefaultEmsdkDir();
    std::string ask =
        "NexaC needs the Emscripten WASM toolchain (em++) to compile --wasm.\n\n"
        "Install it now? Missing Git/Python will be installed too.\n"
        "An existing emsdk checkout is reused (not upgraded).\n\n  ";
    ask += dest.empty() ? std::string("(your home folder)/emsdk") : dest;

    std::cout << "[Nexa] WASM toolchain not found.\n";
    std::cout.flush();
    if (!nexaAskYesNo("NexaC - Install WASM toolchain?", ask.c_str())) {
        nexaPrintWasmInstallHelp();
        return false;
    }
    if (!nexaInstallEmsdk(dest)) return false;

    nexaPrependEmsdkRuntime(dest);
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

// The page that loads an Emscripten build.
//
// Every --wasm build gets one, because a .js loader on its own is not
// something a person can open -- it is something a page includes. Two shells,
// picked by what the program draws with: a gfx program wants a canvas and
// nothing else in the way, and a console program wants somewhere for
// io.println to land, since a browser has no stdout.
//
// `jsName` is the loader's file name, not its path: the page and the loader
// are written side by side, so a relative src is what keeps the pair movable.
static bool nexaWriteWasmHtml(const std::filesystem::path& htmlPath,
                              const std::string& jsName,
                              bool canvas) {
    std::ofstream html(htmlPath);
    if (!html) return false;

    html << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>Nexa</title>\n";
    if (canvas) {
        html << "<style>html,body{margin:0;height:100%;background:#111;color:#ccc;";
        html << "display:flex;flex-direction:column;align-items:center;justify-content:center;";
        html << "font:14px sans-serif}</style>\n";
        html << "</head><body>\n<canvas id=\"canvas\" oncontextmenu=\"event.preventDefault()\"></canvas>\n";
        html << "<div id=\"nexa-status\">Loading\xE2\x80\xA6</div>\n";
        html << "<script>\n";
        html << "var nexaStatus=document.getElementById('nexa-status');\n";
        html << "var Module={\n";
        html << "  canvas:document.getElementById('canvas'),\n";
        html << "  printErr:function(t){if(nexaStatus)nexaStatus.textContent=t;},\n";
        html << "  onAbort:function(r){if(nexaStatus)nexaStatus.textContent=String(r);},\n";
        html << "  onRuntimeInitialized:function(){if(nexaStatus)nexaStatus.remove();}\n";
        html << "};\n";
        html << "</script>\n";
    } else {
        // No canvas, so the page is the terminal the program does not have.
        // print and printErr both land in the same block, in the order they
        // were written, which is what a console program's output looks like.
        html << "<style>html,body{margin:0;min-height:100%;background:#111;color:#ccc;";
        html << "font:14px ui-monospace,SFMono-Regular,Consolas,monospace}";
        html << "#nexa-out{margin:0;padding:16px;white-space:pre-wrap;word-break:break-word}";
        html << "#nexa-out.waiting{color:#777}</style>\n";
        html << "</head><body>\n<pre id=\"nexa-out\" class=\"waiting\">Loading\xE2\x80\xA6</pre>\n";
        html << "<script>\n";
        html << "var nexaOut=document.getElementById('nexa-out');\n";
        html << "var nexaStarted=false;\n";
        html << "function nexaWrite(t){\n";
        html << "  if(!nexaStarted){nexaOut.textContent='';nexaOut.className='';nexaStarted=true;}\n";
        html << "  nexaOut.textContent+=t+'\\n';\n";
        html << "}\n";
        html << "var Module={\n";
        html << "  print:nexaWrite,\n";
        html << "  printErr:nexaWrite,\n";
        html << "  onAbort:function(r){nexaWrite(String(r));}\n";
        html << "};\n";
        html << "</script>\n";
    }
    html << "<script src=\"" << jsName << "\"></script>\n</body></html>\n";
    html.close();
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
    bool linkGfx,
    bool linkGfx3d,
    bool singleFile,
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
        if (linkHttp) cmd += " -sFETCH=1";
        // gfx3d.present yields with emscripten_sleep so the page can
        // composite, exactly as std/gfx does.
        if (linkHttp || linkGfx || linkGfx3d) cmd += " -sASYNCIFY";
        if (linkThread) cmd += " -pthread -sPTHREAD_POOL_SIZE=4";
        // FORCE_FILESYSTEM is gfx's: the file picker and the drop path need it.
        if (linkGfx) cmd += " -sFORCE_FILESYSTEM=1";
        // Baking the .wasm into the .js is the default because a separate
        // .wasm cannot be fetched over file:// -- a browser refuses the
        // cross-origin request for it, so a three-file build only runs off a
        // server. One file opens by double-clicking the page. --wasm-split
        // is for when you are serving it anyway and want the .wasm cacheable
        // on its own.
        if (singleFile) cmd += " -sSINGLE_FILE=1";
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
#ifdef _WIN32
        nexaRefreshWindowsToolPath();
#endif
        nexaPrependEmsdkRuntime(nexaDefaultEmsdkDir());
        if (!nexaCmdExists("node") && !nexaEnsureHostTool("node", "OpenJS.NodeJS.LTS", "nodejs")) {
            std::cerr << "[Nexa] Error: --run --wasm needs node. NexaC could not find or install it.\n";
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

// --debug runtime checks: what the backend compiler is asked to bake into the executable.
// Ordered strongest-first; nexaProbeSanitizer() walks down the list until one links, so a
// toolchain that ships no sanitizer runtime still produces a debuggable (-g) binary.
enum class NexaSanitizer {
    AddressUndefined,  // ASan + UBSan: needs libclang_rt.asan and libclang_rt.ubsan
    Undefined,         // UBSan through the toolchain's runtime: needs libclang_rt.ubsan
    UndefinedTrap,     // UBSan in trap mode: no runtime library; UB aborts on a trap instruction
    None,              // -g only: symbols for a debugger, no added checks
};

// Compiler flags for one level. Never includes -g: every --debug build gets that anyway.
static std::string nexaSanitizerFlags(NexaSanitizer s) {
    switch (s) {
        case NexaSanitizer::AddressUndefined: return "-fsanitize=address,undefined";
        case NexaSanitizer::Undefined:        return "-fsanitize=undefined";
        // -fsanitize-trap turns each check into a trap instruction instead of a call into the
        // UBSan runtime, so this level links on toolchains that ship no sanitizer libraries.
        case NexaSanitizer::UndefinedTrap:    return "-fsanitize=undefined -fsanitize-trap=undefined";
        case NexaSanitizer::None:             break;
    }
    return "";
}

// Named by flag rather than by promise: how a caught error is reported (a printed diagnostic or
// a bare trap) depends on which sanitizer runtime the toolchain ships. Some, like the one bundled
// with zig's clang, abort on SIGILL without printing anything.
static const char* nexaSanitizerLabel(NexaSanitizer s) {
    switch (s) {
        case NexaSanitizer::AddressUndefined: return "address + undefined (-fsanitize=address,undefined)";
        case NexaSanitizer::Undefined:        return "undefined behavior (-fsanitize=undefined)";
        case NexaSanitizer::UndefinedTrap:    return "undefined behavior, trap mode (-fsanitize-trap=undefined)";
        case NexaSanitizer::None:             break;
    }
    return "none (this C++ toolchain ships no linkable sanitizer runtime)";
}

// Pick the strongest sanitizer level this toolchain can actually link, by compiling a 3-line
// throwaway program once per level. Probing beats retrying the real build: a program that fails
// to compile for the user's own reasons then reports that error once instead of four times.
static NexaSanitizer nexaProbeSanitizer(const std::string& cxx, const std::string& targetFlags) {
    namespace fs = std::filesystem;
    std::string pidStr;
#ifdef _WIN32
    pidStr = std::to_string(GetCurrentProcessId());
    const std::string exeSuffix = ".exe";
    const std::string quiet = " >nul 2>&1";
#else
    pidStr = std::to_string(getpid());
    const std::string exeSuffix = "";
    const std::string quiet = " >/dev/null 2>&1";
#endif
    std::error_code ec;
    fs::path src = fs::temp_directory_path(ec) / ("neaxc_sanprobe_" + pidStr + ".cpp");
    fs::path bin = fs::temp_directory_path(ec) / ("neaxc_sanprobe_" + pidStr + exeSuffix);
    if (ec) return NexaSanitizer::None;
    {
        std::ofstream probe(src);
        if (!probe) return NexaSanitizer::None;
        probe << "int main() { return 0; }\n";
    }
    const NexaSanitizer levels[] = {
        NexaSanitizer::AddressUndefined,
        NexaSanitizer::Undefined,
        NexaSanitizer::UndefinedTrap,
    };
    NexaSanitizer chosen = NexaSanitizer::None;
    for (NexaSanitizer level : levels) {
#ifdef _WIN32
        std::string cmd = cxx + targetFlags;
#else
        std::string cmd = "\"" + cxx + "\"" + targetFlags;
#endif
        cmd += " -std=c++17 -g " + nexaSanitizerFlags(level);
        cmd += " \"" + src.string() + "\" -o \"" + bin.string() + "\"" + quiet;
        int ret = std::system(cmd.c_str());
        std::remove(bin.string().c_str());
        if (ret == 0) {
            chosen = level;
            break;
        }
    }
    std::remove(src.string().c_str());
    return chosen;
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
    bool linkSockets,
    bool linkGfx,
    bool linkGfx3d,
    bool noExceptions,
    bool noRtti,
    bool debugBuild,
    const std::vector<std::string>& linkInputs
) {
    // Windows cmd.exe: do not wrap the compiler name in quotes unless it contains spaces;
    // "clang" combined with other quoted paths can confuse cmd's parser (error 123).
#ifdef _WIN32
    std::string cmd = cxx + targetFlags;
#else
    std::string cmd = "\"" + cxx + "\"" + targetFlags;
#endif
#ifdef __APPLE__
    // Cocoa backend is Objective-C++; -x must precede the generated .cpp path.
    // std/gfx3d opens its window the same way, so it needs the same treatment.
    if (linkGfx || linkGfx3d) cmd += " -x objective-c++ -fobjc-arc";
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
#if defined(__aarch64__) || defined(__arm__)
    // ARM Linux makes a C char unsigned; a Nexa char is signed on every platform, so
    // '\x80' is -128 on a Raspberry Pi too. (Windows, macOS and x86 are signed already.)
    cmd += " -fsigned-char";
#endif
    // Size/perf: drop machinery the generated code provably never uses. RTTI is never emitted
    // by the transpiler; exceptions/unwind tables are only needed for try/catch, throw, std::stoi,
    // or inline_cpp. Stripping them removes .eh_frame and RTTI metadata (smaller, no perf cost).
    if (noRtti) {
        cmd += " -fno-rtti";
    }
    if (noExceptions) {
        cmd += " -fno-exceptions";
        // Unwind tables are what a debugger walks to produce a backtrace, so --debug keeps
        // them even when the program itself can never throw.
        if (!debugBuild) cmd += " -fno-unwind-tables -fno-asynchronous-unwind-tables";
    }
    if (debugBuild) {
        // Frame pointers make backtraces reliable even in frames the DWARF CFI does not cover.
        cmd += " -fno-omit-frame-pointer";
    } else {
        // Trim non-essential metadata from the object/binary (no runtime effect).
        // Skipped for --debug: constant merging folds distinct source-level objects together.
        cmd += " -fno-ident -fmerge-all-constants";
    }
    if (const char* nexaCxx = std::getenv("NEXA_CXXFLAGS")) {
        cmd += " ";
        cmd += nexaCxx;
    }
#ifdef _WIN32
    // Section splitting exists only to let the linker garbage-collect; --debug keeps every
    // section (and every symbol) so the debugger can map addresses back to source.
    if (!debugBuild) cmd += " -ffunction-sections -fdata-sections";
    if (buildDll || buildShared) {
        cmd += " -shared";
        // Match Linux: garbage-collect unused sections + strip symbols. MSVC-target clang
        // builds are uncommon here; if linking fails, use a MinGW/LLVM-MinGW toolchain.
        if (!debugBuild) cmd += " -Wl,--gc-sections -s";
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
        if (!debugBuild) cmd += " -Wl,--gc-sections -s";
    }
#elif defined(__APPLE__)
    // ld64 uses dead_strip instead of GNU ld's --gc-sections. Apple platforms
    // provide libc++ as a system library, so static libgcc/libstdc++ flags are invalid.
    if (!debugBuild) cmd += " -Wl,-dead_strip";
    if (buildDll || buildShared) {
        cmd += " -dynamiclib -fPIC";
    }
#else
    // Release: strip the symbol table and garbage-collect unreferenced sections.
    // --debug keeps both: `-s` would delete the very symbols gdb needs, and section GC
    // can drop code the debugger still has line entries for.
    if (!debugBuild) cmd += " -s -ffunction-sections -fdata-sections -Wl,--gc-sections";
    // Native ELF output (Linux exe or .so), not mingw-cross (PE) builds.
    const bool elfTarget = !buildWin && !buildDll;
    if (elfTarget) {
        // No -z max-page-size: the linker's default is the largest page size the CPU's kernels
        // use (64KB on aarch64), and a smaller one makes a binary that only loads on kernels
        // with that page size. The Raspberry Pi 5 kernel uses 16KB pages; a binary linked for
        // 4KB segfaulted there before main.
        // Release drops the build-id; debug asks for one outright. Merely not passing
        // --build-id=none is not enough: lld (and a plainly-configured GNU ld) emits no
        // build-id unless asked, and the build-id is how a debugger or symbol server pairs
        // a binary with separated debug info.
        cmd += debugBuild ? " -Wl,--build-id=sha1" : " -Wl,--build-id=none";
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
#ifndef _WIN32
    // Cross-built Windows targets (mingw). The import libraries the host-Windows
    // branch above spells out are not linked by default here either, and the
    // Windows slice of std/network calls straight into them.
    if (buildWin) {
        if (linkHttp) cmd += " -lwinhttp";
        if (linkHttp || linkSockets) cmd += " -lws2_32";
    }
#endif
#ifdef _WIN32
    if (linkUser32 || linkGfx) {
        // std/os (MessageBoxA, GetConsoleWindow, …) and some inline_cpp; lld does not always pull it implicitly.
        cmd += " -luser32";
        // std/os audio (os.set_volume/get_volume/mute) uses the Core Audio COM API.
        cmd += " -lole32";
        // std/os open uses ShellExecuteA.
        cmd += " -lshell32";
    }
    if (linkGfx3d) {
        // The window is Win32 and the pixel format and buffer swap are GDI.
        // opengl32 is not linked: the runtime LoadLibrary's it, so a build
        // needs no import library and no SDK.
        cmd += " -luser32";
        cmd += " -lgdi32";
        // gfx3d.play and gfx3d.sound reach the same waveOut mixer std/gfx uses,
        // so the same library answers for it. Named unconditionally the way it
        // is for gfx: an unreferenced import library contributes no import, so
        // a program that draws a cube in silence pays nothing for it.
        cmd += " -lwinmm";
    }
    if (linkGfx) {
        cmd += " -lgdi32";
        cmd += " -lwindowscodecs";
        cmd += " -lcomdlg32";
        cmd += " -lwinmm";
    }
    if (linkHttp) {
        // http.* uses WinHTTP to call out (OS API; HTTPS via Schannel) and
        // Winsock to listen (http.localhost).
        cmd += " -lwinhttp";
    }
    if (linkHttp || linkSockets) {
        // Winsock: http.localhost's listening socket, and all of tcp.* / udp.*.
        cmd += " -lws2_32";
    }
#elif defined(__APPLE__)
    if (linkHttp) {
        cmd += " -framework CoreFoundation -framework CFNetwork";
    }
    if (linkGfx3d && !linkGfx) {
        // The window, the view and NSOpenGLPixelFormat are all AppKit. The GL
        // itself is dlopened out of the framework, so it is not linked.
        cmd += " -framework Cocoa";
        // And AudioQueue, for gfx3d.play -- the same mixer gfx.play uses. The
        // !linkGfx guard above is what keeps this from being named twice: the
        // gfx branch below already asks for it.
        cmd += " -framework AudioToolbox";
    }
    if (linkGfx) {
        // AudioToolbox is gfx.audio (AudioQueue); the rest is the window, the
        // dialogs and the image codecs.
        cmd += " -framework Cocoa -framework ApplicationServices -framework ImageIO";
        cmd += " -framework AudioToolbox";
    }
#else
    // http.* HTTPS dlopens system libssl and std/gfx audio dlopens libasound;
    // dlopen lives in libdl (a stub in glibc 2.34 and later, still needed by
    // older ones and by musl).
    if (linkHttp || linkGfx || linkGfx3d) {
        cmd += " -ldl";
    }
    if (linkGfx3d && !linkGfx) {
        // Same X11 as std/gfx, for the same reason: the window. GLX and GL
        // itself are dlopened, so libGL is never a build dependency.
        const bool elfExe = !buildWin && !buildDll && !buildShared;
        if (elfExe) cmd += nexaLinuxGfxEmbedFlags();
        else cmd += " -lX11";
    }
    if (linkGfx) {
        const bool elfExe = !buildWin && !buildDll && !buildShared;
        if (elfExe) {
            // Bake X11 into the binary so the program runs without libX11.so.
            cmd += nexaLinuxGfxEmbedFlags();
        } else {
            cmd += " -lX11";
        }
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
    bool noRtti,
    bool debugBuild
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
    if (noExceptions) {
        cmd += " -fno-exceptions";
        if (!debugBuild) cmd += " -fno-unwind-tables -fno-asynchronous-unwind-tables";
    }
    if (debugBuild) cmd += " -fno-omit-frame-pointer";
    else cmd += " -fno-ident -fmerge-all-constants";
    cmd += " -ffunction-sections -fdata-sections -fPIC -c";
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

// Is this binary being run as the package manager? NexaC and nexapkg are the
// same executable under two names -- a symlink on Unix, a second copy on
// Windows -- and which one it is is decided by the name it was invoked by.
//
// This used to require twelve characters before it would consider the .exe
// form. "nexapkg.exe" is eleven. So on Windows the alias never dispatched
// once: typing nexapkg ran the compiler, which answered every package
// command by printing its own help, and `NexaC nexapkg <cmd>` was the only
// route that worked. Unix was unaffected -- the symlink has no extension and
// matched the first test.
static bool nexaInvokedAsNexapkg(std::string name) {
#ifdef _WIN32
    // Windows resolves a path case-insensitively, so the spelling that
    // reaches argv[0] is whatever the user typed: NEXAPKG.EXE is this
    // program too.
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const std::string ext = ".exe";
    if (name.size() > ext.size() &&
            name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
        name.resize(name.size() - ext.size());
    }
#endif
    return name == "nexapkg";
}

int main(int argc, char* argv[]) {
    // nexapkg: invoked as "nexapkg" or "nexapkg.exe" or "NexaC nexapkg <cmd>"
    std::string exe = argc >= 1 ? argv[0] : "";
    size_t lastSlash = exe.find_last_of("/\\");
    std::string exeName = (lastSlash != std::string::npos) ? exe.substr(lastSlash + 1) : exe;
    bool exeIsNexapkg = nexaInvokedAsNexapkg(exeName);
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
    if (argc >= 2 && std::string(argv[1]) == "upgrade") {
        return nexa::doUpgrade(argc, argv, getExePath(), NEXAC_VERSION);
    }

    std::string inputPath;
    std::string outputExe;
    std::string sourceCpp;
    bool sourceOnly = false;
    bool preserveNames = false;
    bool optimizeSize = false;
    bool debugBuild = false;  // -g -O0, no stripping, implies --preserve-names
    bool buildDll = false;   // mingw -> .dll
    bool buildShared = false;  // clang++ -> .so
    bool buildStaticLib = false;  // -> .a (Linux) / .lib (Windows) static archive
    bool buildWin = false;   // mingw -> .exe (cross-compile from Linux)
    bool buildWasm = false;  // em++ / WASI -> .js+.wasm or .wasm
    bool wasmSplit = false;  // --wasm-split: .html + .js + .wasm, not one baked .js
    bool noConsole = false;  // Windows GUI subsystem
    bool runAfterBuild = false;
    bool pendingSourceOut = false;
    std::string targetName;  // --target <name>: a platform installed with nexapkg
    std::vector<std::string> linkInputs;  // extra objects/archives/libs to link into the exe (--link)

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            // One optional topic follows: NexaC --help std/gfx. Anything
            // after it is ignored, because --help answers and exits.
            std::string topic;
            if (i + 1 < argc) topic = argv[i + 1];
            return nexa::help::run(topic, NEXAC_VERSION);
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
        } else if (arg == "--debug" || arg == "--g" || arg == "-g") {
            debugBuild = true;
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
        } else if (arg == "--wasm-split" || arg == "--split") {
            wasmSplit = true;
        } else if (arg == "--no-console") {
            noConsole = true;
        } else if (arg == "--run" || arg == "-r") {
            runAfterBuild = true;
        } else if (arg == "--target" || arg.rfind("--target=", 0) == 0) {
            if (arg == "--target") {
                if (i + 1 >= argc) {
                    std::cerr << "[Nexa] Error: --target needs a platform name (see: nexapkg target list)\n";
                    return 1;
                }
                targetName = argv[++i];
            } else {
                targetName = arg.substr(9);
            }
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

    // --debug wants the user's own function names in the symbol table; mangled names would make
    // `break my_fn` fail in the debugger. Checked before the flag-conflict errors below so those
    // messages describe what the user typed, not what --debug implied.
    if (debugBuild && optimizeSize) {
        std::cerr << "[Nexa] Error: --debug and --small are contradictory (--debug builds -g -O0 and keeps every symbol; --small optimizes for size and strips)\n";
        std::cerr << "[Nexa] Tip: pick one, or build twice: once with --debug to debug, once with --small to ship\n";
        return 1;
    }
    if (debugBuild) preserveNames = true;

    if (inputPath.empty()) {
        if (runAfterBuild) {
            std::string entry = findEntryFile(std::filesystem::current_path());
            if (!entry.empty()) inputPath = entry;
        }
        if (inputPath.empty()) {
            std::cerr << "Usage: NexaC init [dir]  |  NexaC build [dir]  |  NexaC upgrade\n";
            std::cerr << "       NexaC <file.nxa> [-o <exe>]  |  NexaC <file.nxa> --source <output.cpp>\n";
            std::cerr << "       NexaC <file.nxa> --run  |  NexaC --run (in project dir)\n";
            std::cerr << "Example: NexaC init  |  NexaC build  |  NexaC upgrade  |  NexaC --run\n";
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
    if (buildWasm && debugBuild) {
        // Deliberate: WASM debugging is a browser/DWARF-extension workflow (source maps, the
        // Chrome C/C++ DevTools extension), not the native gdb/lldb workflow --debug promises.
        // Rejecting is clearer than handing back a -g .wasm that no local debugger can attach to.
        std::cerr << "[Nexa] Error: --debug is not supported with --wasm (NexaC debug builds target native gdb/lldb)\n";
        std::cerr << "[Nexa] Tip: debug the same program natively (NexaC file.nxa --debug --run), then build --wasm to ship.\n";
        std::cerr << "[Nexa] Tip: for browser-side DWARF (em++ builds only), pass it through: NEXA_CXXFLAGS=\"-g\" NexaC file.nxa --wasm --p\n";
        return 1;
    }
    if (wasmSplit && !buildWasm) {
        std::cerr << "[Nexa] Error: --wasm-split only means anything with --wasm (it splits the WebAssembly output into .html + .js + .wasm)\n";
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

    // --target is for platforms installed as packages. Everything about it that
    // can be wrong is found here, before a line of the program is read.
    const bool buildTarget = !targetName.empty();
    nexa::target::Spec targetSpec;
    if (buildTarget) {
        if (nexa::target::isBuiltinName(targetName)) {
            const bool web = targetName.rfind("wasm", 0) == 0 || targetName == "web" || targetName == "browser";
            std::cerr << "[Nexa] Error: " << targetName << " is built into NexaC, so it needs no --target\n";
            std::cerr << "[Nexa] Tip: --target is only for platforms installed with nexapkg (nexapkg target list)."
                      << (web ? " For the browser, use --wasm." : "") << "\n";
            return 1;
        }
        if (!nexa::target::isInstalled(targetName)) {
            std::cerr << "[Nexa] Error: no target named '" << targetName << "' is installed\n";
            std::cerr << "[Nexa] Tip: nexapkg target install " << targetName << "\n";
            std::vector<std::string> have = nexa::target::installedNames();
            if (!have.empty()) {
                std::cerr << "[Nexa] Installed:";
                for (const std::string& n : have) std::cerr << " " << n;
                std::cerr << "\n";
            }
            return 1;
        }
        try {
            targetSpec = nexa::target::load(targetName);
        } catch (const std::exception& e) {
            std::cerr << "[Nexa] Error: the " << targetName << " target is damaged: " << e.what() << "\n";
            std::cerr << "[Nexa] Tip: reinstall it with: nexapkg target install " << targetName << " --force\n";
            return 1;
        }
        if (buildWasm || buildWin || buildDll || buildShared || buildStaticLib || noConsole || !linkInputs.empty()) {
            std::cerr << "[Nexa] Error: --target builds a program for " << targetName
                      << " and cannot be combined with --wasm, --win, --dll, --shared, --static-lib, --no-console or --link\n";
            return 1;
        }
        if (runAfterBuild) {
            std::cerr << "[Nexa] Error: --run cannot run a program built for " << targetName << " on this machine\n";
            std::cerr << "[Nexa] Tip: build it (NexaC file.nxa --target " << targetName
                      << "), then copy the result to the machine it is for\n";
            return 1;
        }
        // Linux is the only platform slice a target can ask for today. A
        // target for some other kind of OS needs this NexaC to learn to emit
        // one, which is a compiler change and not a package.
        if (targetSpec.os != "linux") {
            std::cerr << "[Nexa] Error: the " << targetName << " target wants code for '" << targetSpec.os
                      << "', and this NexaC can only build targets for 'linux'\n";
            return 1;
        }
    }

    WasmTool wasmTool;
    if (buildWasm && !sourceOnly) {
        if (!nexaEnsureWasmTool(wasmTool)) return 1;
    }

    std::string cppPath;
    std::string exePath;
    // --run normally builds to a temp file and deletes it afterwards. A debug build exists to be
    // inspected, so --debug --run keeps the binary at its normal output path instead.
    bool useTempExe = runAfterBuild && !debugBuild;

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
            else if (!buildDll && !buildShared && !buildWasm && !buildTarget && (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".exe")) {
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
        if (debugBuild) {
            // DWARF line entries name the file that was compiled, so a debugger can only show
            // source if that file still exists. Release builds transpile into a temp .cpp and
            // delete it; debug builds put it next to the binary and keep it.
            cppPath = exePath + ".debug.cpp";
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

        std::string absInputPath = std::filesystem::absolute(std::filesystem::path(inputPath)).string();

        nexa::Lexer lexer(source, absInputPath);
        std::vector<nexa::Token> tokens = lexer.tokenize();

        nexa::Modules modules;
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
        if (!isLib) {
            bool hasMain = false;
            for (const nexa::AstNode& n : ast) {
                if (n.type == nexa::AstNode::Type::MainFunction) { hasMain = true; break; }
            }
            if (!hasMain) {
                throw std::runtime_error("No fn main() found: an executable needs exactly one fn main() (library builds --dll/--shared/--static-lib do not)");
            }
        }
        nexa::CppTarget cppTarget = nexa::hostCppTarget();
        if (buildWasm) cppTarget = nexa::CppTarget::Wasm;
        else if (buildWin) cppTarget = nexa::CppTarget::Windows;
        else if (buildTarget) cppTarget = nexa::CppTarget::Linux;
        // Debug builds map the generated C++ back to the .nxa source with `#line` directives, so a
        // debugger steps through what the user wrote. The generated file is named absolutely in the
        // snap-back directives for the same reason absInputPath is: a debugger launched from another
        // directory still resolves both.
        std::string absCppPath = cppPath.empty()
            ? cppPath
            : std::filesystem::absolute(std::filesystem::path(cppPath)).string();
        nexa::Transpiler transpiler(ast, modules, preserveNames || isLib, isLib, cppTarget,
                                    debugBuild, absCppPath);  // library: preserve + export C names
        // Name the source file the way parse errors do -- a transpile-stage
        // complaint is still about the program the user handed us.
        std::string cpp;
        try {
            cpp = transpiler.transpile();
        } catch (const std::runtime_error& e) {
            const std::string msg = e.what();
            if (msg.find(absInputPath) == std::string::npos) {
                throw std::runtime_error(absInputPath + ": " + msg);
            }
            throw;
        }

        // Decide which C++ machinery the generated code can safely omit. Exceptions/unwind tables
        // are only needed for try/catch, throw, Result.value(), std::stoi (io.to_int), or inline_cpp.
        // RTTI is never emitted by the transpiler, so it is dropped unless inline_cpp is present.
        // usage.result matters even without an explicit .value() call: the Result runtime header
        // itself contains `throw`, which -fno-exceptions rejects outright.
        const nexa::Modules::CppUsage& usage = transpiler.cppUsage();
        const bool noExceptions = !usage.exceptions && !usage.result && !usage.ioToInt && !modules.hasInlineCpp();
        const bool noRtti = !modules.hasInlineCpp();

        // What the target can and cannot do is the package's to say, and a
        // program that asks for more is refused by name here -- not left to
        // fail at link time against a library that was never built.
        if (buildTarget) {
            std::string missing;
            for (const std::string& m : modules.enabledModules()) {
                if (m.rfind("std/", 0) != 0) continue;
                if (std::find(targetSpec.modules.begin(), targetSpec.modules.end(), m) == targetSpec.modules.end()) {
                    missing += (missing.empty() ? "" : ", ") + m;
                }
            }
            if (!missing.empty()) {
                std::string have;
                for (const std::string& m : targetSpec.modules) have += (have.empty() ? "" : ", ") + m;
                throw std::runtime_error(absInputPath + ": the " + targetSpec.name + " target does not support " +
                                         missing + " (it supports: " + (have.empty() ? "no std modules" : have) + ")");
            }
            if (!noExceptions && !targetSpec.exceptions) {
                throw std::runtime_error(absInputPath + ": this program needs C++ exceptions -- io.to_int, Result, "
                                         "try/catch and inline_cpp all use them -- and the " + targetSpec.name +
                                         " target is built without them");
            }
        }

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

        if (buildTarget) {
            std::cout << "[Nexa] Compiling for " << targetSpec.name << " with clang...\n";
            nexa::target::BuildOptions bo;
            bo.opt = debugBuild ? "-O0" : (optimizeSize ? "-Os" : "-O2");
            bo.debug = debugBuild;
            bo.exceptions = !noExceptions;
            bo.rtti = !noRtti;
            for (const std::string& m : modules.enabledModules()) bo.modules.push_back(m);
            try {
                nexa::target::buildProgram(targetSpec, cppPath, exePath, bo);
            } catch (...) {
                if (!debugBuild) std::remove(cppPath.c_str());
                throw;
            }
            if (!debugBuild) std::remove(cppPath.c_str());
            std::cout << "[Nexa] Build successful! " << exePath << " (for " << targetSpec.name << ")\n";
            return 0;
        }

        std::string wasmOut = exePath;
        if (buildWasm && wasmTool.kind == WasmKind::Emscripten && nexaHasExt(exePath, ".wasm")) {
            wasmOut = std::filesystem::path(exePath).replace_extension(".js").string();
        }
        // A page has no datagram socket -- there is no browser API that opens
        // one, so unlike http.* there is nothing to route udp.* through. Say so
        // here, where the program can still be changed, rather than handing
        // back a .wasm whose every send silently returns 0.
        if (buildWasm && modules.hasUdp() && usage.udp) {
            std::remove(cppPath.c_str());
            std::cerr << "[Nexa] Error: udp.* is not available on WASM (a page has no datagram socket).\n";
            std::cerr << "[Nexa] Tip: http.* works on wasm through the browser's fetch; tcp.* and udp.* do not.\n";
            return 1;
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
            if (modules.hasGfx() && usage.gfx) {
                std::remove(cppPath.c_str());
                std::cerr << "[Nexa] Error: std/gfx on WASM requires Emscripten (em++). Install the emsdk and retry --wasm.\n";
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
#ifdef _WIN32
            cxx = "clang++";
#else
            cxx = findUnixCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found (clang++ or g++). Install one (e.g. `make install-deps`) or set NEXA_CXX.\n";
                return 1;
            }
#endif
            std::cout << "[Nexa] Compiling with " << cxx << " (shared library)...\n";
        } else if (buildStaticLib) {
#ifdef _WIN32
            cxx = findWindowsCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found. Install clang (LLVM) or MinGW (g++/gcc).\n";
                return 1;
            }
#else
            cxx = findUnixCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found (clang++ or g++). Install one (e.g. `make install-deps`) or set NEXA_CXX.\n";
                return 1;
            }
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
            cxx = findUnixCxxNative();
            if (cxx.empty()) {
                std::cerr << "[Nexa] Error: No C++ compiler found (clang++ or g++). Install one (e.g. `make install-deps`) or set NEXA_CXX.\n";
                return 1;
            }
            std::cout << "[Nexa] Compiling with " << cxx << "...\n";
#endif
        }

        std::cout.flush();  // ensure [Nexa] lines appear before child compiler output (e.g. when stdout is redirected)
        // Sanitizers instrument the whole program and need their runtime in the final link, so
        // they are for executables only: an instrumented .so/.dll/.a would force every host
        // process that loads it to be instrumented too.
        const bool sanitizeThisBuild = debugBuild && !buildDll && !buildShared && !buildStaticLib;
        NexaSanitizer sanitizer = NexaSanitizer::None;
        if (sanitizeThisBuild) {
            sanitizer = nexaProbeSanitizer(cxx, targetFlags);
            if (sanitizer != NexaSanitizer::AddressUndefined) {
                std::cout << "[Nexa] Note: " << cxx << " cannot link AddressSanitizer here; falling back to "
                          << nexaSanitizerLabel(sanitizer) << ".\n";
            }
        }
        // Shared libraries (.dll / .so): default to -Os; use --small for exes, or NEXA_CXXFLAGS=-O2
        // if you need speed on a specific library.
        std::string opt = (optimizeSize || buildDll || buildShared) ? "-Os" : "-O2";
        if (debugBuild) {
            opt = "-g -O0";
            std::string san = nexaSanitizerFlags(sanitizer);
            if (!san.empty()) opt += " " + san;
            std::cout << "[Nexa] Debug build: -g -O0, symbols kept, checks: "
                      << (sanitizeThisBuild ? nexaSanitizerLabel(sanitizer)
                                            : "none (library builds are not instrumented)") << "\n";
            std::cout.flush();
        }
        const bool linkUser32 = modules.hasOs() || modules.hasInlineCpp();
        // std/network is one include for three protocols, so what a program
        // links has to follow what it calls rather than what it included: a
        // program that only speaks udp has no business pulling in WinHTTP or
        // CFNetwork. The runtime for a protocol nobody calls is not emitted
        // either, so there was never anything for those libraries to satisfy.
        const bool linkHttp = modules.hasHttp() && usage.http;
        const bool linkSockets = (modules.hasTcp() && usage.tcp) || (modules.hasUdp() && usage.udp);
        const bool linkGfx = modules.hasGfx() && usage.gfx;
        const bool linkGfx3d = modules.hasGfx3d() && usage.gfx3d;

        if (buildWasm) {
            // WASI has no .js loader and no page: it emits a bare module for a
            // wasmtime-style host. There is nothing there to split, so asking
            // is a mistake worth naming rather than a flag to ignore.
            if (wasmSplit && wasmTool.kind != WasmKind::Emscripten) {
                std::cerr << "[Nexa] Error: --wasm-split needs Emscripten; the WASI toolchain emits a bare .wasm with no .js loader to split\n";
                std::cerr << "[Nexa] Tip: install Emscripten (NexaC offers to), or set NEXA_WASM_CXX to an em++.\n";
                return 1;
            }
            const bool singleFile = (wasmTool.kind == WasmKind::Emscripten) && !wasmSplit;
            std::string cmd = nexaWasmCompileCmd(wasmTool, cppPath, wasmOut, opt, noExceptions, noRtti,
                modules.hasHttp() && usage.http, modules.hasThread() && usage.thread,
                modules.hasGfx() && usage.gfx, linkGfx3d, singleFile, linkInputs);
            int ret = std::system(cmd.c_str());
            std::remove(cppPath.c_str());
            if (ret != 0) {
                std::cerr << "[Nexa] Compilation failed.\n";
                return 1;
            }
            std::cout << "[Nexa] Build successful!\n";
            std::filesystem::path wasmHtmlPath;
            if (wasmTool.kind == WasmKind::Emscripten) {
                // A .js loader is something a page includes, not something a
                // person opens, so every Emscripten build gets a page. Which
                // shell depends on what the program draws with.
                std::filesystem::path jsPath(wasmOut);
                wasmHtmlPath = jsPath;
                wasmHtmlPath.replace_extension(".html");
                // Both drawing modules render into a canvas; everything else
                // gets the console page.
                const bool wantsCanvas = (modules.hasGfx() && usage.gfx)
                                      || (modules.hasGfx3d() && usage.gfx3d);
                if (!nexaWriteWasmHtml(wasmHtmlPath, jsPath.filename().string(), wantsCanvas)) {
                    std::cerr << "[Nexa] Error: Cannot write " << wasmHtmlPath.string() << "\n";
                    return 1;
                }
                std::cout << "[Nexa] Page: " << wasmHtmlPath.string() << "\n";
                std::cout << "[Nexa] Loader: " << wasmOut << "\n";
                if (singleFile) {
                    std::cout << "[Nexa] Open the .html in a browser (the .js embeds the .wasm, so file:// works).\n";
                } else {
                    std::cout << "[Nexa] Module: "
                              << std::filesystem::path(wasmOut).replace_extension(".wasm").string() << "\n";
                    std::cout << "[Nexa] Serve all three from one directory: a separate .wasm is fetched, and a\n";
                    std::cout << "[Nexa] browser will not fetch it over file:// -- use http:// (python -m http.server).\n";
                }
                std::cout << "[Nexa] Or run the loader directly: node \"" << wasmOut << "\"\n";
            } else {
                std::cout << "[Nexa] Module: " << wasmOut << "\n";
                std::cout << "[Nexa] Run: wasmtime \"" << wasmOut << "\"\n";
            }
            std::cout.flush();
            if (runAfterBuild) {
                int runRet = runWasmOutput(wasmOut, wasmTool.kind);
                std::remove(wasmOut.c_str());
                if (wasmTool.kind == WasmKind::Emscripten) {
                    // The page always exists; the sibling .wasm only when the
                    // build was split. Removing one that is not there is a
                    // no-op, so both are named unconditionally.
                    if (!wasmHtmlPath.empty()) std::remove(wasmHtmlPath.string().c_str());
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
            std::string cmd = nexaStaticLibCmd(cxx, cppPath, objPath, exePath, opt, noExceptions, noRtti, debugBuild);
            int ret = std::system(cmd.c_str());
            if (!debugBuild) std::remove(cppPath.c_str());  // debug: the archive's DWARF points at it
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

        std::string cmd = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll(), noConsole, linkUser32, linkHttp, linkSockets, linkGfx, linkGfx3d, noExceptions, noRtti, debugBuild, linkInputs);
        int ret = std::system(cmd.c_str());

        if (ret != 0 && sanitizeThisBuild && sanitizer != NexaSanitizer::None) {
            // The probe links a 3-line program; the real link adds -static-libstdc++, gfx/http
            // system libraries and --link inputs, any of which can be incompatible with the
            // sanitizer runtime. A debuggable binary is worth more than the instrumentation.
            std::cout << "[Nexa] Sanitizer flags (" << nexaSanitizerLabel(sanitizer)
                      << ") failed to link this program; retrying with plain -g.\n";
            std::cout.flush();
            sanitizer = NexaSanitizer::None;
            opt = "-g -O0";
            std::string cmdNoSan = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll(), noConsole, linkUser32, linkHttp, linkSockets, linkGfx, linkGfx3d, noExceptions, noRtti, debugBuild, linkInputs);
            ret = std::system(cmdNoSan.c_str());
        }

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
                std::string cmd2 = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll(), noConsole, linkUser32, linkHttp, linkSockets, linkGfx, linkGfx3d, noExceptions, noRtti, debugBuild, linkInputs);
                ret = std::system(cmd2.c_str());
            }
        }

        // Debug builds keep the generated C++: the binary's DWARF refers to it by path, so
        // deleting it would leave the debugger with line numbers and no source to show.
        if (!debugBuild) std::remove(cppPath.c_str());

        if (ret != 0) {
            std::cerr << "[Nexa] Compilation failed.\n";
            if (debugBuild) std::cerr << "[Nexa] Generated C++ kept for inspection: " << cppPath << "\n";
            return 1;
        }

        std::cout << "[Nexa] Build successful!\n";
        if (debugBuild) {
            std::cout << "[Nexa] Debug binary: " << exePath << "\n";
            std::cout << "[Nexa] Debug source: your .nxa (the debugger steps through it directly)\n";
            std::cout << "[Nexa] Generated C++: " << cppPath << " (kept; line info names it for runtime helpers)\n";
            if (!buildDll && !buildShared) {
                std::cout << "[Nexa] Debug it with: gdb \"" << exePath << "\"  (break <nexa fn name>, run)\n";
            }
            std::cout.flush();
        }

        if (runAfterBuild) {
            // A temp-file build is an absolute path, but a debug build runs the real output
            // ("myprog"), and no shell searches the current directory for a bare name.
            std::string runTarget = exePath;
            if (runTarget.find('/') == std::string::npos && runTarget.find('\\') == std::string::npos) {
#ifdef _WIN32
                runTarget = ".\\" + runTarget;
#else
                runTarget = "./" + runTarget;
#endif
            }
            int runRet = std::system(("\"" + runTarget + "\"").c_str());
            // --debug --run keeps the binary so it can be re-run under a debugger.
            if (!debugBuild) std::remove(exePath.c_str());
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
            if (runAfterBuild && !debugBuild && !exePath.empty()) std::remove(exePath.c_str());
        }
        std::cerr << "[Nexa] Error: " << e.what() << "\n";
        return 1;
    }
}
