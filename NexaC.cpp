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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#define NEXAC_VERSION "0.1.3"

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

static int doBuild(const std::string& dir) {
    namespace fs = std::filesystem;
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    std::string entry = findEntryFile(base);
    if (entry.empty()) {
        std::cerr << "[Nexa] Error: No .nxa file with fn main() or fn __init__() in " << base.string() << "\n";
        std::cerr << "Run 'NexaC init' to create a project.\n";
        return 1;
    }
    fs::path entryPath(entry);
    std::string outBase = (base / entryPath.stem()).string();
    std::string exePath = getExePath();
    std::string cmd = exePath.empty() ? "NexaC" : ("\"" + exePath + "\"");
    cmd += " \"" + entry + "\" -o \"" + outBase + "\"";
    int ret = std::system(cmd.c_str());
    return (ret == 0) ? 0 : 1;
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
    std::string nexapkgJson = "{\n  \"name\": \"myapp\",\n  \"dependencies\": {}\n}\n";
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
    const char* candidates[] = {"clang", "clang++", "g++", "gcc"};
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

// Build the shell command for clang/g++/gcc. Windows (MSVC-target clang) cannot use -fPIC on DLLs
// and needs an explicit subsystem for console executables; GNU flags (-Wl,--gc-sections, -s) are skipped there.
static std::string nexaBuildCompileCmd(
    const std::string& cxx,
    const std::string& targetFlags,
    const std::string& cppPath,
    const std::string& exePath,
    const std::string& opt,
    bool buildDll,
    bool buildShared,
    bool buildWin,
    bool modulesHasDll
) {
    // Windows cmd.exe: do not wrap the compiler name in quotes unless it contains spaces;
    // "clang" combined with other quoted paths can confuse cmd's parser (error 123).
#ifdef _WIN32
    std::string cmd = cxx + targetFlags;
#else
    std::string cmd = "\"" + cxx + "\"" + targetFlags;
#endif
    cmd += " \"" + cppPath + "\" " + opt;
#ifdef _WIN32
    cmd += " -ffunction-sections -fdata-sections";
    if (buildDll || buildShared) {
        cmd += " -shared";
    } else {
        // Quoted for cmd.exe: unquoted /SUBSYSTEM is parsed as multiple invalid paths (error 123).
        cmd += " -Xlinker \"/SUBSYSTEM:CONSOLE\"";
    }
#else
    cmd += " -s -ffunction-sections -fdata-sections -Wl,--gc-sections";
    if (buildDll || buildShared) {
        cmd += " -shared -fPIC";
    }
#endif
    if (buildWin && !buildDll && !buildShared) {
#ifndef _WIN32
        cmd += " -static -static-libgcc -static-libstdc++";
#endif
    }
#ifdef _WIN32
    // user32: std/os (MessageBoxA, GetConsoleWindow, …) and inline_cpp + windows.h. lld-link does not pull it implicitly.
    cmd += " -luser32";
#endif
    cmd += " -o \"" + exePath + "\"";
#ifndef _WIN32
    if (modulesHasDll && buildShared) cmd += " -ldl";
    cmd += " 2>&1";
#endif
    return cmd;
}

// Resolve topic/page string to page number (1-8). 0 = unknown.
static int helpPageFromArg(const std::string& arg) {
    if (arg.empty() || arg == "1" || arg == "page1") return 1;
    if (arg == "2" || arg == "page2" || arg == "core" || arg == "lang" || arg == "language") return 2;
    if (arg == "3" || arg == "page3" || arg == "std/io") return 3;
    if (arg == "4" || arg == "page4" || arg == "std/os") return 4;
    if (arg == "5" || arg == "page5" || arg == "std/dll") return 5;
    if (arg == "6" || arg == "page6" || arg == "std/file") return 6;
    if (arg == "7" || arg == "page7" || arg == "std/random") return 7;
    if (arg == "8" || arg == "page8" || arg == "std/inline" || arg == "inline" || arg == "inline_cpp") return 8;
    if (arg.size() >= 6 && arg.substr(0, 6) == "--page") {
        int n = 0;
        for (size_t i = 6; i < arg.size() && std::isdigit(arg[i]); i++)
            n = n * 10 + (arg[i] - '0');
        if (n >= 1 && n <= 8) return n;
    }
    return 0;
}

static int printHelp(int page = 1) {
    if (page == 2) {
        std::cout << "NexaC - Core language (page 2/8)\n\n";
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
        std::cout << "  #include <std/io>   - print, println, readln\n";
        std::cout << "  #include <std/os>   - system, platform, getenv\n";
        std::cout << "  #include <std/dll>  - load, call (dynamic libraries)\n";
        std::cout << "  #include <std/file> - read, write, append, exists\n";
        std::cout << "  #include <std/random> - int, seed\n";
        std::cout << "  #include <std/inline> - inline_cpp! { ... } (embed C++)\n\n";
        std::cout << "  #include \"file.nxa\"  - include another .nxa file (path relative to current file)\n";
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
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page3  NexaC --help std/io\n";
        return 0;
    }
    if (page == 3) {
        std::cout << "NexaC - std/io module (page 3/8)\n\n";
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
        std::cout << "Variables used:\n";
        std::cout << "  - string: from \"literal\" or io.readln()\n";
        std::cout << "  - int: from 42, -10, etc.\n\n";
        std::cout << "Example:\n";
        std::cout << "  io.print(\"Name: \");\n";
        std::cout << "  let name = io.readln();\n";
        std::cout << "  io.println(name);\n";
        std::cout << "  let x = 42;\n";
        std::cout << "  io.println(x);\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page4  NexaC --help std/os\n";
        return 0;
    }
    if (page == 4) {
        std::cout << "NexaC - std/os module (page 4/8)\n\n";
        std::cout << "OS and system calls. Include with: #include <std/os>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  os.system(cmd)\n";
        std::cout << "    Runs a shell command. Blocks until complete.\n";
        std::cout << "    cmd: string literal \"...\" or string variable\n";
        std::cout << "    Returns: (exit code from shell, not captured in Nexa)\n\n";
        std::cout << "  os.platform()\n";
        std::cout << "    Returns OS name: \"windows\", \"linux\", \"darwin\", or \"unknown\".\n";
        std::cout << "    Use: let p = os.platform();  io.println(p);\n\n";
        std::cout << "  os.getenv(name)\n";
        std::cout << "    Returns environment variable value (string) or \"\" if unset.\n\n";
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
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page5  NexaC --help std/dll\n";
        return 0;
    }
    if (page == 5) {
        std::cout << "NexaC - std/dll module (page 5/8)\n\n";
        std::cout << "Dynamic library loading. Include with: #include <std/dll>\n\n";
        std::cout << "Calls:\n\n";
        std::cout << "  dll.load(path)\n";
        std::cout << "    Loads a .so (Linux) or .dll (Windows) file.\n";
        std::cout << "    path: string literal, e.g. \"./mylib.so\" or \"mylib.dll\"\n";
        std::cout << "    Returns: handle (int) for use with dll.call\n";
        std::cout << "    Use with: let h = dll.load(\"./mylib.so\");\n\n";
        std::cout << "  dll.call(handle, \"symbol\")\n";
        std::cout << "    Calls a void function with no args from the loaded library.\n";
        std::cout << "    handle: variable from dll.load\n";
        std::cout << "    symbol: string literal, function name to call\n\n";
        std::cout << "Example:\n";
        std::cout << "  #include <std/dll>\n";
        std::cout << "  #include <std/io>\n\n";
        std::cout << "  fn main() {\n";
        std::cout << "    let h = dll.load(\"./plugin.so\");\n";
        std::cout << "    dll.call(h, \"plugin_init\");\n";
        std::cout << "  }\n\n";
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page6  NexaC --help std/file\n";
        return 0;
    }
    if (page == 6) {
        std::cout << "NexaC - std/file module (page 6/8)\n\n";
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
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page7  NexaC --help std/random\n";
        return 0;
    }
    if (page == 7) {
        std::cout << "NexaC - std/random module (page 7/8)\n\n";
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
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page8  NexaC --help std/inline\n";
        return 0;
    }
    if (page == 8) {
        std::cout << "NexaC - std/inline (inline_cpp!) (page 8/8)\n\n";
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
        std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
        std::cout << "  NexaC --help --page1\n";
        return 0;
    }
    // Page 1 (default): usage and options
    std::cout << "NexaC - Nexa compiler (page 1/8)\n";
    std::cout << "A general purpose, high-performance programming language.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  NexaC init [dir]       Scaffold new project (current dir or dir/)\n";
    std::cout << "  NexaC build [dir]      Build .nxa with fn main() or fn __init__()\n";
    std::cout << "  NexaC <file.nxa> [-o <executable>]\n";
    std::cout << "  NexaC <file.nxa> --source <output.cpp>\n";
    std::cout << "  NexaC --help [page|module]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <name>     Output executable name\n";
    std::cout << "  --source <f>  Emit C++ only, do not compile\n";
    std::cout << "  --preserve-names, --p  Keep function names in generated C++ (default: mangle)\n";
    std::cout << "  --small       Optimize for smaller executable (-Os)\n";
    std::cout << "  --dll     Build Windows .dll (uses mingw-w64 cross-compiler)\n";
    std::cout << "  --shared  Build Linux .so (uses clang++)\n";
    std::cout << "  --win     Build Windows .exe (mingw-w64 from Linux; native on Windows)\n";
    std::cout << "  --help, -h    Show this help\n";
    std::cout << "  --version, --v, -v  Show version\n";
    std::cout << "  --help <page>  Show page (2=core ... 7=std/random, 8=std/inline)\n";
    std::cout << "  --help --pageN  Same (e.g. --page2, --page3)\n\n";
    std::cout << "Standard library modules:\n";
    std::cout << "  std/io        Input/output: print, println, readln\n";
    std::cout << "  std/os        System calls: system\n";
    std::cout << "  std/dll       Dynamic libraries: load, call\n";
    std::cout << "  std/file      File I/O: read, write, append, exists\n";
    std::cout << "  std/random   Random: int, seed\n";
    std::cout << "  std/inline   inline_cpp! { ... } embed C++ (requires include)\n";
    std::cout << "  core          Core language: variables, types, fn main, functions\n\n";
    std::cout << "Example:\n";
    std::cout << "  NexaC program.nxa -o program\n";
    std::cout << "  NexaC --help --page2  NexaC --help std/io\n\n";
    std::cout << "Pages: 1=usage | 2=core | 3=std/io | 4=std/os | 5=std/dll | 6=std/file | 7=std/random | 8=std/inline\n";
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
    bool buildWin = false;   // mingw -> .exe (cross-compile from Linux)
    bool runAfterBuild = false;

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
                std::cerr << "Use 'NexaC --help' to list pages (1-8) and modules (std/io, std/os, std/dll, std/file, std/random, std/inline, core).\n";
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
            } else {
                outputExe = outPath;
            }
        } else if (arg == "--source") {
            if (i + 1 >= argc) {
                std::cerr << "[Nexa] Error: --source requires a filename\n";
                return 1;
            }
            sourceCpp = argv[++i];
            sourceOnly = true;
        } else if (arg == "--preserve-names" || arg == "--p" || arg == "-p") {
            preserveNames = true;
        } else if (arg == "--small") {
            optimizeSize = true;
        } else if (arg == "--dll") {
            buildDll = true;
        } else if (arg == "--shared") {
            buildShared = true;
        } else if (arg == "--win" || arg == "--windows") {
            buildWin = true;
        } else if (arg == "--run" || arg == "-r") {
            runAfterBuild = true;
        } else if (arg[0] != '-') {
            inputPath = arg;
        }
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

    if (runAfterBuild && (buildDll || buildShared || buildWin)) {
        std::cerr << "[Nexa] Error: --run cannot be used with --dll, --shared, or --win\n";
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
#ifdef _WIN32
            exePath += ".exe";
#endif
        } else if (outputExe.empty()) {
            exePath = inputPath;
            if (exePath.size() >= 4 && exePath.substr(exePath.size() - 4) == ".nxa") {
                exePath = exePath.substr(0, exePath.size() - 4);
            } else {
                exePath += "_out";
            }
            if (buildDll) {
                exePath += ".dll";
            } else if (buildShared) {
                exePath += ".so";
            } else if (buildWin) {
                exePath += ".exe";
            }
        } else {
            exePath = outputExe;
            if (buildWin && (exePath.size() < 4 || exePath.substr(exePath.size() - 4) != ".exe")) {
                exePath += ".exe";
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

        bool isSharedLib = buildDll || buildShared;
        nexa::Transpiler transpiler(ast, modules, preserveNames || isSharedLib, isSharedLib);  // shared lib: preserve names
        std::string cpp = transpiler.transpile();

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

        std::string cxx;
        std::string targetFlags;
        if (buildDll) {
#ifdef _WIN32
            cxx = "clang";
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
        } else if (buildWin) {
#ifdef _WIN32
            cxx = "clang";
            std::cout << "[Nexa] Compiling with clang (Windows exe)...\n";
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
        std::string opt = optimizeSize ? "-Os" : "-O2";
        std::string cmd = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll());
        int ret = std::system(cmd.c_str());

        if (ret != 0) {
            std::string fallback;
#ifdef _WIN32
            const char* next[] = {"clang", "clang++", "g++", "gcc"};
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
                std::string cmd2 = nexaBuildCompileCmd(cxx, targetFlags, cppPath, exePath, opt, buildDll, buildShared, buildWin, modules.hasDll());
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
