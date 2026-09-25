#pragma once

// --target: building for a platform that is not built into NexaC.
//
// The four NexaC was born with -- Windows, Linux, macOS and the browser -- are
// not touched by any of this. They build exactly as they always have, and
// --target refuses their names. Everything else is a TARGET PACKAGE, installed
// with `nexapkg target install <name>` into ~/.nexa/targets/<name>/.
//
// A target package is source and nothing else. No compiler, no prebuilt
// library, no binary of any kind: a C library, a C++ library, compiler
// builtins, whatever the platform needs, all as source text, plus target.json
// saying how to build it. Everything is compiled here, after the NexaC call,
// by the clang already on the machine -- which is a cross-compiler for every
// CPU LLVM knows whether anyone asked it to be one or not. So a package can be
// read end to end, and nothing in it is a black box.
//
// That makes the first build for a target slow and every later one fast. The
// first time a program is built for arm64-linux, the platform's runtime --
// libc, libc++, builtins -- is compiled from the package's source into
// ~/.nexa/cache/targets/<name>/, once. After that a build is one compile of
// the program and one link.
//
// What the program itself becomes is decided the usual way. target.json names
// which of NexaC's platform slices to emit ("os": "linux"), and the program is
// transpiled exactly as it would be for that platform -- then compiled for the
// target's CPU against the target's libraries instead of the host's.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include "nexapkg.hpp"

namespace nexa {
namespace target {

namespace fs = std::filesystem;

// The platforms NexaC builds for without a package. --target is for the ones
// it does not, so naming one of these is a mistake worth a clear answer rather
// than a search of ~/.nexa/targets that finds nothing.
inline bool isBuiltinName(const std::string& n) {
    static const char* names[] = {"windows", "win", "win32", "win64", "linux", "macos", "mac",
                                  "darwin", "osx", "wasm", "wasm32", "web", "browser", "native",
                                  "host", nullptr};
    std::string l;
    for (char c : n) l.push_back((char)std::tolower((unsigned char)c));
    for (const char** p = names; *p; p++) if (l == *p) return true;
    return false;
}

inline fs::path targetsDir() {
    return fs::path(pkg::getHome()) / ".nexa" / "targets";
}

inline fs::path cacheRoot() {
    return fs::path(pkg::getHome()) / ".nexa" / "cache" / "targets";
}

// --- a small JSON reader -----------------------------------------------------
//
// target.json needs objects, arrays, strings, numbers and booleans, and nothing
// in the compiler already reads all five (nexapkg's reader only pulls strings
// out of nexapkg.json). This is the whole grammar, strictly: a target file
// that is not valid JSON is refused with a line number, not half-read.

struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    const Json* get(const std::string& key) const {
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

class JsonReader {
public:
    explicit JsonReader(const std::string& text) : s_(text) {}

    Json parse() {
        // A byte-order mark is not JSON, but Windows editors -- Notepad, and
        // PowerShell's Set-Content -Encoding utf8 -- put one on every file
        // they save. A target somebody edited by hand is not damaged for it.
        if (s_.compare(0, 3, "\xEF\xBB\xBF") == 0) i_ = 3;
        Json v = value();
        ws();
        if (i_ != s_.size()) fail("unexpected text after the end of the document");
        return v;
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    [[noreturn]] void fail(const std::string& what) {
        size_t line = 1;
        for (size_t k = 0; k < i_ && k < s_.size(); k++) if (s_[k] == '\n') line++;
        throw std::runtime_error("line " + std::to_string(line) + ": " + what);
    }

    void ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) i_++;
    }

    bool lit(const char* word) {
        size_t n = std::strlen(word);
        if (s_.compare(i_, n, word) == 0) { i_ += n; return true; }
        return false;
    }

    Json value() {
        ws();
        if (i_ >= s_.size()) fail("unexpected end of file");
        char c = s_[i_];
        Json v;
        if (c == '{') {
            v.kind = Json::Object;
            i_++;
            ws();
            if (i_ < s_.size() && s_[i_] == '}') { i_++; return v; }
            for (;;) {
                ws();
                if (i_ >= s_.size() || s_[i_] != '"') fail("expected a quoted key");
                std::string k = string();
                ws();
                if (i_ >= s_.size() || s_[i_] != ':') fail("expected ':' after \"" + k + "\"");
                i_++;
                v.obj.emplace_back(k, value());
                ws();
                if (i_ < s_.size() && s_[i_] == ',') { i_++; continue; }
                if (i_ < s_.size() && s_[i_] == '}') { i_++; return v; }
                fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.kind = Json::Array;
            i_++;
            ws();
            if (i_ < s_.size() && s_[i_] == ']') { i_++; return v; }
            for (;;) {
                v.arr.push_back(value());
                ws();
                if (i_ < s_.size() && s_[i_] == ',') { i_++; continue; }
                if (i_ < s_.size() && s_[i_] == ']') { i_++; return v; }
                fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.kind = Json::String; v.str = string(); return v; }
        if (lit("true")) { v.kind = Json::Bool; v.b = true; return v; }
        if (lit("false")) { v.kind = Json::Bool; v.b = false; return v; }
        if (lit("null")) return v;
        if (c == '-' || (c >= '0' && c <= '9')) {
            // JSON's own number grammar, and no looser: -?int(.digits)?(e[+-]?digits)?
            // A version written 1.0.0 is a string that lost its quotes, and
            // reading it as the number 1.0 would hide that until much later.
            size_t start = i_;
            auto digits = [&]() {
                size_t d = i_;
                while (i_ < s_.size() && std::isdigit((unsigned char)s_[i_])) i_++;
                if (i_ == d) fail("malformed number");
            };
            if (s_[i_] == '-') i_++;
            digits();
            if (i_ < s_.size() && s_[i_] == '.') { i_++; digits(); }
            if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
                i_++;
                if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) i_++;
                digits();
            }
            v.kind = Json::Number;
            v.num = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
            return v;
        }
        fail(std::string("unexpected '") + c + "'");
    }

    std::string string() {
        i_++;  // opening quote
        std::string out;
        while (i_ < s_.size() && s_[i_] != '"') {
            char c = s_[i_++];
            if (c != '\\') { out.push_back(c); continue; }
            if (i_ >= s_.size()) break;
            char e = s_[i_++];
            switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    // ASCII is all a target file has any use for; anything
                    // wider is kept as '?' rather than decoded half-way.
                    unsigned cp = 0;
                    for (int k = 0; k < 4 && i_ < s_.size(); k++, i_++) {
                        char h = s_[i_];
                        cp = cp * 16 + (unsigned)(std::isdigit((unsigned char)h) ? h - '0'
                                                 : (std::tolower((unsigned char)h) - 'a' + 10));
                    }
                    out.push_back(cp < 128 ? (char)cp : '?');
                    break;
                }
                default: out.push_back(e); break;
            }
        }
        if (i_ >= s_.size()) fail("unterminated string");
        i_++;  // closing quote
        return out;
    }
};

// --- the target description --------------------------------------------------

struct Library {
    std::string name;                 // "c" -> libc.a in the cache
    fs::path root;                    // where its sources live, inside the package
    std::vector<std::string> files;   // relative to root, from the list file
    std::vector<std::string> flags;   // placeholders still in them
    // Built and linked only for a program that includes one of these modules
    // ("when": ["std/inline"]); empty means always. Lets a target keep the
    // bulk of a library -- all of libc++, say -- off every other program's
    // first build.
    std::vector<std::string> when;
};

struct Spec {
    fs::path dir;                     // the installed package
    std::string name;
    std::string version;
    std::string description;
    std::string os;                   // which of NexaC's platform slices to emit
    std::string triple;               // what clang is told to build for
    int clangMajor = 0;               // oldest clang the package's sources accept
    bool exceptions = false;          // whether its C++ runtime can throw
    std::vector<std::string> modules; // std modules a program may include
    std::vector<Library> libraries;
    std::string startLib;             // whose flags the start/end files compile with
    std::vector<std::string> startFiles;
    std::vector<std::string> endFiles;
    std::vector<std::string> programFlags;
    std::vector<std::string> linkFlags;
};

inline std::vector<std::string> strings(const Json* j, const std::string& what) {
    std::vector<std::string> out;
    if (!j) return out;
    if (j->kind != Json::Array) throw std::runtime_error(what + " must be a list of strings");
    for (const Json& e : j->arr) {
        if (e.kind != Json::String) throw std::runtime_error(what + " must be a list of strings");
        out.push_back(e.str);
    }
    return out;
}

inline std::string text(const Json& root, const std::string& key, bool required) {
    const Json* j = root.get(key);
    if (!j) {
        if (required) throw std::runtime_error("missing \"" + key + "\"");
        return std::string();
    }
    if (j->kind != Json::String) throw std::runtime_error("\"" + key + "\" must be a string");
    return j->str;
}

// A list file: one source path per line, '#' starts a comment. Kept separate
// from target.json because a C library is well over a thousand files, and a
// thousand-line JSON array is a worse thing to review than a thousand lines.
inline std::vector<std::string> readList(const fs::path& p) {
    std::ifstream in(p);
    if (!in) throw std::runtime_error("cannot read " + p.string());
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t a = 0;
        while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) a++;
        if (a < line.size()) out.push_back(line.substr(a));
    }
    return out;
}

inline bool isInstalled(const std::string& name) {
    return fs::exists(targetsDir() / name / "target.json");
}

inline Spec load(const std::string& name) {
    fs::path dir = targetsDir() / name;
    fs::path file = dir / "target.json";
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + file.string());
    std::stringstream ss;
    ss << in.rdbuf();
    Json root;
    try {
        root = JsonReader(ss.str()).parse();
    } catch (const std::exception& e) {
        throw std::runtime_error(file.string() + ": " + e.what());
    }
    if (root.kind != Json::Object) throw std::runtime_error(file.string() + ": not a JSON object");

    Spec s;
    s.dir = dir;
    try {
        s.name = text(root, "name", true);
        s.version = text(root, "version", true);
        s.description = text(root, "description", false);
        s.os = text(root, "os", true);
        s.triple = text(root, "triple", true);
        if (const Json* c = root.get("clang")) {
            if (c->kind != Json::Number) throw std::runtime_error("\"clang\" must be a number");
            s.clangMajor = (int)c->num;
        }
        if (const Json* e = root.get("exceptions")) {
            if (e->kind != Json::Bool) throw std::runtime_error("\"exceptions\" must be true or false");
            s.exceptions = e->b;
        }
        s.modules = strings(root.get("modules"), "\"modules\"");
        const Json* libs = root.get("libraries");
        if (!libs || libs->kind != Json::Array) throw std::runtime_error("\"libraries\" must be a list");
        for (const Json& l : libs->arr) {
            if (l.kind != Json::Object) throw std::runtime_error("each library must be an object");
            Library lib;
            lib.name = text(l, "name", true);
            lib.root = dir / text(l, "root", true);
            lib.files = readList(dir / text(l, "files", true));
            lib.flags = strings(l.get("flags"), "library \"" + lib.name + "\" flags");
            lib.when = strings(l.get("when"), "library \"" + lib.name + "\" when");
            s.libraries.push_back(lib);
        }
        if (const Json* st = root.get("startfiles")) {
            s.startLib = text(*st, "library", true);
            s.startFiles = strings(st->get("before"), "\"startfiles\".before");
            s.endFiles = strings(st->get("after"), "\"startfiles\".after");
        }
        if (const Json* p = root.get("program")) s.programFlags = strings(p->get("flags"), "\"program\".flags");
        s.linkFlags = strings(root.get("link"), "\"link\"");
    } catch (const std::exception& e) {
        throw std::runtime_error(file.string() + ": " + e.what());
    }
    if (s.name != name) {
        throw std::runtime_error(file.string() + ": says it is \"" + s.name + "\" but is installed as \"" + name + "\"");
    }
    if (!s.startLib.empty()) {
        bool found = false;
        for (const Library& l : s.libraries) {
            if (l.name != s.startLib) continue;
            found = true;
            if (!l.when.empty()) {
                throw std::runtime_error(file.string() + ": startfiles come from library \"" + s.startLib +
                                         "\", which has a \"when\" -- every program needs its start files");
            }
        }
        if (!found) throw std::runtime_error(file.string() + ": startfiles name library \"" + s.startLib + "\", which is not listed");
    }
    return s;
}

inline std::vector<std::string> installedNames() {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(targetsDir(), ec)) return out;
    for (const auto& e : fs::directory_iterator(targetsDir(), ec)) {
        if (e.is_directory() && fs::exists(e.path() / "target.json")) out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// --- running processes ---------------------------------------------------------
//
// Not std::system. That goes through cmd.exe on Windows, which rewrites the
// quoting of any command line that starts with a quoted path -- and clang
// lives in "C:\Program Files\LLVM" -- and it cannot be run from several
// threads at once with each child's output kept apart. A runtime is well over
// a thousand compiles, so they run in parallel, each writing to its own log.

#ifdef _WIN32
// The quoting CreateProcess's children parse argv back out of: quote anything
// with a space, tab or quote in it, double the backslashes that precede a
// quote, escape the quote.
inline std::string quoteArg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string out = "\"";
    size_t slashes = 0;
    for (char c : a) {
        if (c == '\\') { slashes++; continue; }
        if (c == '"') { out.append(slashes * 2 + 1, '\\'); out.push_back('"'); slashes = 0; continue; }
        out.append(slashes, '\\');
        slashes = 0;
        out.push_back(c);
    }
    out.append(slashes * 2, '\\');
    out.push_back('"');
    return out;
}

#endif

// Runs argv to completion with stdout and stderr both going to log. Returns
// the exit code, or -1 if it could not be started at all.
inline int run(const std::vector<std::string>& argv, const fs::path& log) {
#ifdef _WIN32
    std::string cmd;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i) cmd.push_back(' ');
        cmd += quoteArg(argv[i]);
    }
    // The child inherits exactly one handle, its log, named in a handle list.
    // Without the list, every inheritable handle in the process goes to every
    // child -- including the log another thread has just opened for its own --
    // and the only guard was a lock that let one process start at a time: with
    // a thousand compiles to start, that queue was the build's bottleneck.
    PROCESS_INFORMATION pi{};
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE h = CreateFileA(log.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<char> attrBuf(attrSize);
    auto attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
    HANDLE inherit[1] = {h};
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize)) {
        CloseHandle(h);
        return -1;
    }
    BOOL ok = UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit),
                                        nullptr, nullptr);
    if (ok) {
        STARTUPINFOEXA si{};
        si.StartupInfo.cb = sizeof(si);
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput = nullptr;
        si.StartupInfo.hStdOutput = h;
        si.StartupInfo.hStdError = h;
        si.lpAttributeList = attrs;
        std::vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');
        ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                            &si.StartupInfo, &pi);
    }
    DeleteProcThreadAttributeList(attrs);
    CloseHandle(h);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 1, log.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&fa, 1, 2);
    std::vector<char*> args;
    for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    pid_t pid = 0;
    int rc = posix_spawnp(&pid, args[0], &fa, nullptr, args.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0) return -1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

inline std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::string capture(const std::vector<std::string>& argv, const fs::path& scratch) {
    fs::path log = scratch / "capture.log";
    if (run(argv, log) != 0) return std::string();
    std::string out = slurp(log);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

// --- the toolchain ---------------------------------------------------------------

struct Toolchain {
    std::string cxx;       // clang++
    std::string cc;        // clang, for .c, .s and .S
    std::string lld;       // ld.lld
    std::string ar;        // llvm-ar
    std::string resource;  // clang's own headers: stddef.h, stdarg.h, arm_neon.h
    int major = 0;
};

// Clang, and the three tools that ship beside it. Found by asking clang itself
// where its siblings are, which is right wherever LLVM was installed. Only
// clang will do: g++ is built for one target, and cannot be asked for another.
inline Toolchain findToolchain(const fs::path& scratch) {
    std::vector<std::string> tries;
    if (const char* e = std::getenv("NEXA_TARGET_CLANG")) tries.push_back(e);
    tries.push_back("clang++");
#ifdef _WIN32
    tries.push_back("C:\\Program Files\\LLVM\\bin\\clang++.exe");
#else
    tries.push_back("/usr/bin/clang++");
    tries.push_back("/opt/homebrew/opt/llvm/bin/clang++");
    tries.push_back("/usr/local/opt/llvm/bin/clang++");
#endif
    Toolchain t;
    std::string ver;
    for (const std::string& c : tries) {
        ver = capture({c, "--version"}, scratch);
        if (ver.find("clang version") != std::string::npos) { t.cxx = c; break; }
    }
    if (t.cxx.empty()) {
        throw std::runtime_error("--target builds with clang (LLVM), and none was found. Install LLVM, "
                                 "or set NEXA_TARGET_CLANG to a clang++ binary");
    }
    size_t at = ver.find("clang version ");
    t.major = std::atoi(ver.c_str() + at + 14);
    t.resource = capture({t.cxx, "-print-resource-dir"}, scratch);
    if (!t.resource.empty()) t.resource = (fs::path(t.resource) / "include").generic_string();
    t.lld = capture({t.cxx, "-print-prog-name=ld.lld"}, scratch);
    t.ar = capture({t.cxx, "-print-prog-name=llvm-ar"}, scratch);
    t.cc = capture({t.cxx, "-print-prog-name=clang"}, scratch);
    if (t.lld.empty()) t.lld = "ld.lld";
    if (t.ar.empty()) t.ar = "llvm-ar";
    if (t.cc.empty()) t.cc = "clang";
    return t;
}

// --- building ------------------------------------------------------------------------

inline std::string expand(std::string s, const std::vector<std::pair<std::string, std::string>>& vars) {
    for (const auto& kv : vars) {
        size_t p;
        while ((p = s.find(kv.first)) != std::string::npos) s.replace(p, kv.first.size(), kv.second);
    }
    return s;
}

inline bool isCxxSource(const std::string& f) {
    auto ends = [&](const char* e) {
        size_t n = std::strlen(e);
        return f.size() >= n && f.compare(f.size() - n, n, e) == 0;
    };
    return ends(".cpp") || ends(".cc") || ends(".cxx");
}

struct Job {
    std::vector<std::string> argv;
    std::string label;
};

// Runs jobs on every core. Stops handing out new ones at the first failure and
// reports that one with its compiler output, since it is the only one anybody
// needs to read.
inline void runAll(const std::vector<Job>& jobs, const fs::path& logDir, const std::string& what) {
    std::error_code ec;
    fs::create_directories(logDir, ec);
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::mutex m;
    std::string failLabel, failLog;
    unsigned n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;
#ifdef _WIN32
    // Two jobs per hardware thread on Windows: part of every compile there is
    // spent not computing -- creating the process, tearing it down, the file
    // scans behind each open -- and a second job keeps the core busy through
    // it. On a 4-core, 8-thread i7-7700 the arm64-linux runtime took 29-31 s
    // at 8 jobs and 28 s at 16, and no less past that. Linux compiles are
    // CPU-bound throughout, and more jobs than threads only add switching.
    n *= 2;
#endif
    if (n > jobs.size()) n = (unsigned)jobs.size();
    std::vector<std::thread> pool;
    for (unsigned w = 0; w < n; w++) {
        pool.emplace_back([&, w]() {
            fs::path log = logDir / ("job" + std::to_string(w) + ".log");
            for (;;) {
                if (failed) return;
                size_t i = next++;
                if (i >= jobs.size()) return;
                if (run(jobs[i].argv, log) != 0) {
                    std::lock_guard<std::mutex> g(m);
                    if (!failed) {
                        failed = true;
                        failLabel = jobs[i].label;
                        failLog = slurp(log);
                    }
                    return;
                }
            }
        });
    }
    for (std::thread& t : pool) t.join();
    if (failed) {
        throw std::runtime_error(what + ": " + failLabel + " did not compile:\n" + failLog);
    }
}

// Object files get flat names: the path inside the package with its slashes
// turned into underscores, which keeps two sources of the same name in
// different directories from landing on each other.
inline std::string objectName(const std::string& rel) {
    std::string o;
    for (char c : rel) o.push_back((c == '/' || c == '\\') ? '_' : c);
    return o + ".o";
}

inline Job compileJob(const Toolchain& tc, const Spec& s, const Library& lib, const std::string& rel,
                      const fs::path& out, const std::vector<std::pair<std::string, std::string>>& vars) {
    Job j;
    j.label = lib.name + ": " + rel;
    j.argv.push_back(isCxxSource(rel) ? tc.cxx : tc.cc);
    j.argv.push_back("--target=" + s.triple);
    for (const std::string& f : lib.flags) j.argv.push_back(expand(f, vars));
    j.argv.push_back("-c");
    j.argv.push_back((lib.root / rel).generic_string());
    j.argv.push_back("-o");
    j.argv.push_back(out.generic_string());
    return j;
}

// Where this target's compiled runtime lives. Keyed by the package's version
// and by clang's major version, so updating either builds it again rather
// than linking objects made for something else.
inline fs::path runtimeDir(const Spec& s, const Toolchain& tc) {
    return cacheRoot() / s.name / (s.version + "-clang" + std::to_string(tc.major));
}

inline std::vector<std::pair<std::string, std::string>> varsFor(const Spec& s, const Toolchain& tc,
                                                               const Library* lib) {
    std::vector<std::pair<std::string, std::string>> v;
    if (lib) v.push_back({"{root}", lib->root.generic_string()});
    v.push_back({"{target}", s.dir.generic_string()});
    v.push_back({"{resource}", tc.resource});
    return v;
}

// Whether a program that includes `modules` needs this library.
inline bool wanted(const Library& l, const std::vector<std::string>& modules) {
    if (l.when.empty()) return true;
    for (const std::string& w : l.when) {
        for (const std::string& m : modules) {
            if (m == w) return true;
        }
    }
    return false;
}

// Compiles the target's libraries into the cache: those every program needs
// on the first build, and a conditional one ("when") the first time a program
// needs it. A stamp written last says each part is whole -- "complete" for the
// libraries every program links and the start files, "complete-<name>" for a
// conditional library -- so one interrupted half-way is simply built again.
//
// Every file still to compile goes into one queue, the biggest C++ sources
// first. A C++ file takes many times longer than a C one; started last, one
// core would still be on locale.cpp while the others sat idle, and one
// library at a time did that at the end of every library.
inline fs::path ensureRuntime(const Spec& s, const Toolchain& tc, const std::vector<std::string>& modules) {
    fs::path dir = runtimeDir(s, tc);
    fs::path core = dir / "complete";
    auto stampFor = [&](const Library& l) { return l.when.empty() ? core : dir / ("complete-" + l.name); };

    std::error_code ec;
    const bool firstBuild = !fs::exists(core);
    if (firstBuild) fs::remove_all(dir, ec);  // what an interrupted first build left
    std::vector<const Library*> todo;
    for (const Library& l : s.libraries) {
        if (wanted(l, modules) && !fs::exists(stampFor(l))) todo.push_back(&l);
    }
    if (todo.empty()) return dir;
    fs::create_directories(dir, ec);

    struct Item {
        Job job;
        uintmax_t weight;
    };
    std::vector<Item> items;
    auto add = [&](const Library& lib, const std::string& rel, const fs::path& out,
                   const std::vector<std::pair<std::string, std::string>>& vars) {
        uintmax_t size = fs::file_size(lib.root / rel, ec);
        if (ec) size = 0;
        items.push_back({compileJob(tc, s, lib, rel, out, vars), isCxxSource(rel) ? size * 16 : size});
    };
    size_t total = 0;
    for (const Library* lib : todo) {
        auto vars = varsFor(s, tc, lib);
        fs::create_directories(dir / "obj" / lib->name, ec);
        for (const std::string& rel : lib->files) add(*lib, rel, dir / "obj" / lib->name / objectName(rel), vars);
        total += lib->files.size();
    }
    // The start and end files are objects of their own rather than archive
    // members, because they have to be linked, and in a fixed place, whether
    // or not anything refers to them. They are part of the first build.
    if (firstBuild && !s.startLib.empty()) {
        const Library* lib = nullptr;
        for (const Library& l : s.libraries) if (l.name == s.startLib) lib = &l;
        auto vars = varsFor(s, tc, lib);
        std::vector<std::string> all = s.startFiles;
        all.insert(all.end(), s.endFiles.begin(), s.endFiles.end());
        for (const std::string& rel : all) add(*lib, rel, dir / objectName(rel), vars);
    }

    if (firstBuild) {
        std::cout << "[Nexa] " << s.name << ": first build for this target -- compiling its runtime from source ("
                  << total << " files). This happens once; later builds reuse it.\n";
    } else {
        std::string why;
        for (const Library* lib : todo) {
            for (const std::string& w : lib->when) {
                if (std::find(modules.begin(), modules.end(), w) != modules.end() &&
                    why.find(w) == std::string::npos) {
                    why += (why.empty() ? "" : ", ") + w;
                }
            }
        }
        std::cout << "[Nexa] " << s.name << ": first program with " << why
                  << " -- compiling the part of the runtime it needs (" << total
                  << " files). This happens once.\n";
    }
    for (const Library* lib : todo) std::cout << "[Nexa]   " << lib->name << ": " << lib->files.size() << " files\n";
    auto t0 = std::chrono::steady_clock::now();

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.weight > b.weight; });
    std::vector<Job> jobs;
    for (Item& it : items) jobs.push_back(std::move(it.job));
    runAll(jobs, dir / "logs", s.name + " runtime");

    // An archive per library, so the linker takes only the objects a program
    // uses. The object list goes in a response file: a thousand paths is far
    // past the length Windows allows a command line.
    for (const Library* lib : todo) {
        fs::path rsp = dir / (lib->name + ".rsp");
        {
            std::ofstream r(rsp);
            for (const std::string& rel : lib->files) {
                r << '"' << (dir / "obj" / lib->name / objectName(rel)).generic_string() << "\"\n";
            }
        }
        fs::path archive = dir / ("lib" + lib->name + ".a");
        fs::remove(archive, ec);
        fs::path log = dir / "logs" / ("ar-" + lib->name + ".log");
        if (run({tc.ar, "rcs", archive.generic_string(), "@" + rsp.generic_string()}, log) != 0) {
            throw std::runtime_error(s.name + " runtime: could not archive " + lib->name + ":\n" + slurp(log));
        }
        if (!lib->when.empty()) std::ofstream(stampFor(*lib)) << lib->name << "\n";
    }

    auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
    if (firstBuild) std::ofstream(core) << s.name << " " << s.version << " clang " << tc.major << "\n";
    std::cout << "[Nexa] " << s.name << ": runtime built in " << secs << "s\n";
    return dir;
}

struct BuildOptions {
    std::string opt = "-O2";  // or -Os for --small, -O0 for --debug
    bool debug = false;
    bool strip = true;
    // Whether this program throws or asks what type something is. Decided
    // the way a native build decides it, so a program that does neither is
    // compiled without the tables for either -- against a runtime that has
    // them, which is fine: code without exceptions links against code with.
    bool exceptions = false;
    bool rtti = false;
    // The std modules the program includes: a library with a "when" is built
    // and linked only if one of its modules is here.
    std::vector<std::string> modules;
};

// The program: one compile against the target's headers, one static link
// against its runtime. Returns normally or throws with the tool's output.
inline void buildProgram(const Spec& s, const std::string& cppPath, const std::string& outPath,
                         const BuildOptions& o) {
    fs::path scratch = fs::temp_directory_path() / ("nexa_target_" + s.name);
    std::error_code ec;
    fs::create_directories(scratch, ec);
    Toolchain tc = findToolchain(scratch);
    if (s.clangMajor > 0 && tc.major < s.clangMajor) {
        throw std::runtime_error(s.name + " needs clang " + std::to_string(s.clangMajor) +
                                 " or newer -- its C++ library is written for it -- and this is clang " +
                                 std::to_string(tc.major) + ". Update LLVM, or point NEXA_TARGET_CLANG at a newer one");
    }
    fs::path rt = ensureRuntime(s, tc, o.modules);

    auto vars = varsFor(s, tc, nullptr);
    fs::path obj = scratch / "program.o";
    std::vector<std::string> cc = {tc.cxx, "--target=" + s.triple};
    for (const std::string& f : s.programFlags) cc.push_back(expand(f, vars));
    cc.push_back(o.opt);
    if (o.debug) cc.push_back("-g");
    if (!o.exceptions) cc.push_back("-fno-exceptions");
    if (!o.rtti) cc.push_back("-fno-rtti");
    cc.push_back("-ffunction-sections");
    cc.push_back("-fdata-sections");
    cc.push_back("-c");
    cc.push_back(cppPath);
    cc.push_back("-o");
    cc.push_back(obj.generic_string());
    fs::path log = scratch / "compile.log";
    if (run(cc, log) != 0) throw std::runtime_error("compiling for " + s.name + " failed:\n" + slurp(log));

    std::vector<std::string> ld = {tc.lld, "-static", "--gc-sections", "-o", outPath};
    if (o.strip && !o.debug) ld.push_back("--strip-all");
    for (const std::string& f : s.startFiles) ld.push_back((rt / objectName(f)).generic_string());
    ld.push_back(obj.generic_string());
    ld.push_back("--start-group");
    for (const Library& l : s.libraries) {
        if (wanted(l, o.modules)) ld.push_back((rt / ("lib" + l.name + ".a")).generic_string());
    }
    ld.push_back("--end-group");
    for (const std::string& f : s.endFiles) ld.push_back((rt / objectName(f)).generic_string());
    for (const std::string& f : s.linkFlags) ld.push_back(expand(f, vars));
    log = scratch / "link.log";
    if (run(ld, log) != 0) throw std::runtime_error("linking for " + s.name + " failed:\n" + slurp(log));
}

}  // namespace target
}  // namespace nexa
