#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

namespace nexa {
namespace pkg {

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// Paths
// ----------------------------------------------------------------------------
static std::string getHome() {
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
    return h ? h : ".";
#else
    const char* h = std::getenv("HOME");
    return h ? h : "/tmp";
#endif
}

static std::string getPackagesDir() {
    return (fs::path(getHome()) / ".nexa" / "packages").string();
}

// ----------------------------------------------------------------------------
// Shell helpers
// ----------------------------------------------------------------------------
// Run a command and capture trimmed stdout (empty on failure).
static std::string runCapture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    FILE* p = _popen((cmd + " 2>NUL").c_str(), "r");
#else
    FILE* p = popen((cmd + " 2>/dev/null").c_str(), "r");
#endif
    if (!p) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), p)) out += buf;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

static bool gitAvailable() {
    return !runCapture("git --version").empty();
}

// ----------------------------------------------------------------------------
// Minimal JSON helpers
// ----------------------------------------------------------------------------
static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

static bool parseJsonString(const std::string& s, size_t& i, std::string& out) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    if (i >= s.size() || s[i] != '"') return false;
    i++;
    out.clear();
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\') {
            i++;
            if (i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    default: out += e; break;
                }
            }
        } else {
            out += s[i++];
        }
    }
    if (i < s.size()) i++;
    return true;
}

// Extract a top-level "key": "value" string field.
static std::string findStringField(const std::string& content, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t i = content.find(needle);
    if (i == std::string::npos) return "";
    i += needle.size();
    while (i < content.size() && (content[i] == ' ' || content[i] == '\t' || content[i] == ':')) i++;
    std::string val;
    if (i < content.size() && content[i] == '"') {
        parseJsonString(content, i, val);
    }
    return val;
}

// ----------------------------------------------------------------------------
// Manifest (nexapkg.json)
// ----------------------------------------------------------------------------
struct Manifest {
    std::string name = "myapp";
    std::string version;
    std::string entry;   // .nxa entry file for NexaC build (e.g. main.nxa)
    std::string output;  // executable base name or path for NexaC build (-o)
    std::string dll;       // .nxa to compile as a shared library (.dll / .so / .dylib)
    std::string dllOutput; // library base name or path (default: stem of dll)
    // Ordered include-path -> source (source may carry a trailing @ref).
    std::vector<std::pair<std::string, std::string>> deps;

    int indexOf(const std::string& includePath) const {
        for (size_t k = 0; k < deps.size(); k++) if (deps[k].first == includePath) return (int)k;
        return -1;
    }
    void set(const std::string& includePath, const std::string& source) {
        int k = indexOf(includePath);
        if (k >= 0) deps[(size_t)k].second = source;
        else deps.emplace_back(includePath, source);
    }
    bool erase(const std::string& includePath) {
        int k = indexOf(includePath);
        if (k < 0) return false;
        deps.erase(deps.begin() + k);
        return true;
    }
};

static bool readManifest(const fs::path& path, Manifest& m) {
    std::ifstream f(path);
    if (!f) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    std::string nm = findStringField(content, "name");
    if (!nm.empty()) m.name = nm;
    m.version = findStringField(content, "version");
    m.entry = findStringField(content, "entry");
    m.output = findStringField(content, "output");
    m.dll = findStringField(content, "dll");
    m.dllOutput = findStringField(content, "dllOutput");

    m.deps.clear();
    size_t i = content.find("\"dependencies\"");
    if (i != std::string::npos) {
        i = content.find('{', i);
        if (i != std::string::npos) {
            i++;
            while (i < content.size()) {
                while (i < content.size() && (content[i] == ' ' || content[i] == '\t' || content[i] == '\n' ||
                                              content[i] == '\r' || content[i] == ':' || content[i] == ',')) i++;
                if (i >= content.size() || content[i] == '}') break;
                std::string key, val;
                if (!parseJsonString(content, i, key)) break;
                while (i < content.size() && (content[i] == ' ' || content[i] == '\t' || content[i] == ':' ||
                                              content[i] == ',' || content[i] == '\n' || content[i] == '\r')) i++;
                if (!parseJsonString(content, i, val)) break;
                m.set(key, val);
            }
        }
    }
    return true;
}

static void writeManifest(const fs::path& path, const Manifest& m) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"name\": \"" << jsonEscape(m.name) << "\",\n";
    if (!m.version.empty()) out << "  \"version\": \"" << jsonEscape(m.version) << "\",\n";
    if (!m.entry.empty()) out << "  \"entry\": \"" << jsonEscape(m.entry) << "\",\n";
    if (!m.output.empty()) out << "  \"output\": \"" << jsonEscape(m.output) << "\",\n";
    if (!m.dll.empty()) out << "  \"dll\": \"" << jsonEscape(m.dll) << "\",\n";
    if (!m.dllOutput.empty()) out << "  \"dllOutput\": \"" << jsonEscape(m.dllOutput) << "\",\n";
    out << "  \"dependencies\": {";
    for (size_t k = 0; k < m.deps.size(); k++) {
        out << (k == 0 ? "\n" : ",\n");
        out << "    \"" << jsonEscape(m.deps[k].first) << "\": \"" << jsonEscape(m.deps[k].second) << "\"";
    }
    out << (m.deps.empty() ? "}\n" : "\n  }\n");
    out << "}\n";
    std::ofstream(path) << out.str();
}

// ----------------------------------------------------------------------------
// Lockfile (nexapkg.lock)
// ----------------------------------------------------------------------------
struct LockEntry {
    std::string source;  // resolved source (with @ref if any)
    std::string commit;  // resolved git commit SHA (empty for local deps)
};

struct Lock {
    std::vector<std::pair<std::string, LockEntry>> entries;  // ordered by include path

    LockEntry* find(const std::string& includePath) {
        for (auto& e : entries) if (e.first == includePath) return &e.second;
        return nullptr;
    }
    void set(const std::string& includePath, const LockEntry& le) {
        for (auto& e : entries) {
            if (e.first == includePath) { e.second = le; return; }
        }
        entries.emplace_back(includePath, le);
    }
    void erase(const std::string& includePath) {
        for (size_t k = 0; k < entries.size(); k++) {
            if (entries[k].first == includePath) { entries.erase(entries.begin() + k); return; }
        }
    }
};

static void readLock(const fs::path& path, Lock& lk) {
    std::ifstream f(path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    size_t i = content.find('{');
    if (i == std::string::npos) return;
    i++;
    while (i < content.size()) {
        while (i < content.size() && (content[i] == ' ' || content[i] == '\t' || content[i] == '\n' ||
                                      content[i] == '\r' || content[i] == ',')) i++;
        if (i >= content.size() || content[i] == '}') break;
        std::string key;
        if (!parseJsonString(content, i, key)) break;
        // value is an object { "source": "...", "commit": "..." }
        size_t objStart = content.find('{', i);
        size_t objEnd = content.find('}', objStart);
        if (objStart == std::string::npos || objEnd == std::string::npos) break;
        std::string obj = content.substr(objStart, objEnd - objStart + 1);
        LockEntry le;
        le.source = findStringField(obj, "source");
        le.commit = findStringField(obj, "commit");
        lk.set(key, le);
        i = objEnd + 1;
    }
}

static void writeLock(const fs::path& path, const Lock& lk) {
    if (lk.entries.empty()) {
        std::error_code ec;
        fs::remove(path, ec);
        return;
    }
    std::ostringstream out;
    out << "{\n";
    for (size_t k = 0; k < lk.entries.size(); k++) {
        out << "  \"" << jsonEscape(lk.entries[k].first) << "\": { \"source\": \""
            << jsonEscape(lk.entries[k].second.source) << "\"";
        if (!lk.entries[k].second.commit.empty()) {
            out << ", \"commit\": \"" << jsonEscape(lk.entries[k].second.commit) << "\"";
        }
        out << " }" << (k + 1 < lk.entries.size() ? "," : "") << "\n";
    }
    out << "}\n";
    std::ofstream(path) << out.str();
}

// ----------------------------------------------------------------------------
// Source spec parsing
// ----------------------------------------------------------------------------
struct ResolvedSpec {
    std::string includePath;  // default dependency key
    std::string source;       // stored in manifest (url@ref, file:abs, or global name)
    bool isLocal = false;
};

// Split a stored source "url@ref" into (url, ref). Local/url sources have no '@'.
static void splitSourceRef(const std::string& source, std::string& url, std::string& ref) {
    url = source;
    ref.clear();
    if (source.rfind("file:", 0) == 0) return;  // local path, never split
    size_t at = source.rfind('@');
    if (at != std::string::npos && at > 0) {
        url = source.substr(0, at);
        ref = source.substr(at + 1);
    }
}

static std::string stripNxa(std::string s) {
    if (s.size() > 4 && s.substr(s.size() - 4) == ".nxa") s = s.substr(0, s.size() - 4);
    return s;
}

// Turn a user-provided spec into a resolved (includePath, source).
// Forms: owner/repo[@ref]  https://...[@ref]  ./local/path  <global-name>
static ResolvedSpec resolveSpec(const std::string& spec, const fs::path& projBase) {
    ResolvedSpec r;
    // Local path
    if (spec.rfind("./", 0) == 0 || spec.rfind("../", 0) == 0 ||
        (!spec.empty() && (spec[0] == '/' || (spec.size() > 1 && spec[1] == ':')))) {
        fs::path localPath = fs::path(spec).is_absolute() ? fs::path(spec) : (projBase / spec).lexically_normal();
        r.includePath = stripNxa(localPath.filename().string());
        r.source = "file:" + fs::absolute(localPath).string();
        r.isLocal = true;
        return r;
    }
    // Remote: split optional @ref
    std::string base = spec, ref;
    size_t at = spec.rfind('@');
    if (at != std::string::npos && at > 0 && spec.rfind("http", 0) != 0) {
        // shorthand owner/repo@ref
        base = spec.substr(0, at);
        ref = spec.substr(at + 1);
    } else if (at != std::string::npos && spec.rfind("http", 0) == 0) {
        // a full URL: only treat @ as ref if it appears after the host path
        size_t scheme = spec.find("://");
        if (scheme != std::string::npos && at > scheme + 3) {
            base = spec.substr(0, at);
            ref = spec.substr(at + 1);
        }
    }
    std::string url;
    if (base.rfind("http", 0) == 0) {
        url = base;
    } else if (base.find('/') != std::string::npos) {
        url = "https://github.com/" + base;
    } else {
        // bare name: a global package
        url = base;
    }
    size_t slash = base.rfind('/');
    r.includePath = stripNxa(slash != std::string::npos ? base.substr(slash + 1) : base);
    r.source = ref.empty() ? url : (url + "@" + ref);
    return r;
}

// ----------------------------------------------------------------------------
// Install a single dependency into <pkgDir>/<includePath>
// Returns true on success; sets outCommit for git deps.
// ----------------------------------------------------------------------------
static bool installDep(const std::string& includePath, const std::string& source,
                       const fs::path& pkgDir, const fs::path& projBase,
                       const std::string& pinCommit, bool force, std::string& outCommit) {
    fs::path targetBase = pkgDir / includePath;
    fs::path targetParent = (includePath.find('/') != std::string::npos) ? targetBase.parent_path() : targetBase;
    std::string moduleName = stripNxa(targetBase.filename().string());
    outCommit.clear();

    if (source.rfind("file:", 0) == 0) {
        // Local dependency: always re-copy so local edits propagate.
        std::string pathStr = source.substr(5);
        fs::path path = fs::path(pathStr);
        if (path.is_relative()) path = (projBase / pathStr).lexically_normal();
        path = fs::absolute(path);
        if (!fs::exists(path)) {
            std::cerr << "[nexapkg] Path not found: " << path.string() << "\n";
            return false;
        }
        std::error_code ec;
        fs::create_directories(targetParent, ec);
        if (fs::is_directory(path)) {
            for (const auto& e : fs::directory_iterator(path)) {
                fs::path dest = targetParent / e.path().filename();
                if (e.is_directory()) fs::copy(e.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                else fs::copy_file(e.path(), dest, fs::copy_options::overwrite_existing, ec);
            }
        } else {
            std::string destName = moduleName;
            if (destName.size() < 4 || destName.substr(destName.size() - 4) != ".nxa") destName += ".nxa";
            fs::copy_file(path, targetParent / destName, fs::copy_options::overwrite_existing, ec);
        }
        std::cout << "[nexapkg] Installed " << includePath << " (local)\n";
        return true;
    }

    // Git dependency
    std::string url, ref;
    splitSourceRef(source, url, ref);
    if (url.find("http") != 0) url = "https://github.com/" + url;

    if (fs::exists(targetBase) && !fs::is_empty(targetBase) && !force) {
        std::cout << "[nexapkg] " << includePath << " already installed (use 'update' or --force)\n";
        return true;
    }
    if (!gitAvailable()) {
        std::cerr << "[nexapkg] git not found; cannot fetch " << includePath << "\n";
        return false;
    }

    std::string tmpName = "nexapkg_";
    for (char c : includePath) tmpName += (c == '/') ? '_' : c;
    fs::path tmp = fs::temp_directory_path() / tmpName;
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    int ret = -1;
    std::string commitToCheckout = pinCommit;
    if (!ref.empty()) {
        // Pin to a tag or branch.
        std::string cmd = "git clone --depth 1 --branch \"" + ref + "\" \"" + url + "\" \"" + tmp.string() + "\"";
        ret = std::system((cmd + (
#ifdef _WIN32
            " >NUL 2>NUL"
#else
            " >/dev/null 2>/dev/null"
#endif
        )).c_str());
    } else if (!commitToCheckout.empty()) {
        // Reproduce a locked commit (needs full history).
        std::string cmd = "git clone \"" + url + "\" \"" + tmp.string() + "\"";
        ret = std::system((cmd +
#ifdef _WIN32
            " >NUL 2>NUL"
#else
            " >/dev/null 2>/dev/null"
#endif
        ).c_str());
        if (ret == 0) {
            std::system(("git -C \"" + tmp.string() + "\" checkout " + commitToCheckout +
#ifdef _WIN32
                " >NUL 2>NUL"
#else
                " >/dev/null 2>/dev/null"
#endif
            ).c_str());
        }
    } else {
        std::string cmd = "git clone --depth 1 \"" + url + "\" \"" + tmp.string() + "\"";
        ret = std::system((cmd +
#ifdef _WIN32
            " >NUL 2>NUL"
#else
            " >/dev/null 2>/dev/null"
#endif
        ).c_str());
    }
    if (ret != 0) {
        std::cerr << "[nexapkg] Failed to fetch " << includePath << " from " << url
                  << (ref.empty() ? "" : ("@" + ref)) << "\n";
        fs::remove_all(tmp, ec);
        return false;
    }

    outCommit = runCapture("git -C \"" + tmp.string() + "\" rev-parse HEAD");

    fs::remove_all(targetBase, ec);
    fs::create_directories(targetParent, ec);
    for (const auto& e : fs::directory_iterator(tmp)) {
        std::string fn = e.path().filename().string();
        if (fn == ".git") continue;
        fs::path dest = targetParent / fn;
        if (e.is_directory()) fs::copy(e.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        else fs::copy_file(e.path(), dest, fs::copy_options::overwrite_existing, ec);
    }
    fs::remove_all(tmp, ec);
    std::cout << "[nexapkg] Installed " << includePath
              << (ref.empty() ? "" : (" @" + ref))
              << (outCommit.empty() ? "" : (" (" + outCommit.substr(0, 7) + ")")) << "\n";
    return true;
}

// ----------------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------------
static int cmdInit(const std::string& dir) {
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    fs::path manifest = base / "nexapkg.json";
    if (fs::exists(manifest)) {
        std::cerr << "[nexapkg] nexapkg.json already exists.\n";
        return 1;
    }
    Manifest m;
    m.name = base.filename().empty() ? "myapp" : base.filename().string();
    writeManifest(manifest, m);
    std::cout << "[nexapkg] Created nexapkg.json (name: " << m.name << ")\n";
    return 0;
}

static int cmdAdd(const std::string& spec, const std::string& dir, const std::string& asPath) {
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    fs::path manifest = base / "nexapkg.json";
    Manifest m;
    if (!fs::exists(manifest)) {
        std::cerr << "[nexapkg] Run 'nexapkg init' first.\n";
        return 1;
    }
    readManifest(manifest, m);
    ResolvedSpec r = resolveSpec(spec, base);
    std::string key = asPath.empty() ? r.includePath : asPath;
    m.set(key, r.source);
    writeManifest(manifest, m);
    std::cout << "[nexapkg] Added " << key << " <- " << r.source << "\n";
    return 0;
}

static int cmdRemove(const std::string& name, const std::string& dir) {
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    fs::path manifest = base / "nexapkg.json";
    if (!fs::exists(manifest)) {
        std::cerr << "[nexapkg] nexapkg.json not found.\n";
        return 1;
    }
    Manifest m;
    readManifest(manifest, m);
    if (!m.erase(name)) {
        std::cerr << "[nexapkg] Dependency not found: " << name << "\n";
        return 1;
    }
    writeManifest(manifest, m);

    Lock lk;
    fs::path lockPath = base / "nexapkg.lock";
    readLock(lockPath, lk);
    lk.erase(name);
    writeLock(lockPath, lk);

    std::error_code ec;
    fs::remove_all(base / ".nexa" / "packages" / name, ec);
    std::cout << "[nexapkg] Removed " << name << "\n";
    return 0;
}

// Shared install/update worker. updateMode ignores the lock and re-fetches.
static int installAll(const std::string& dir, bool global, bool force, bool updateMode,
                      const std::string& onlyName) {
    fs::path projBase = dir.empty() ? fs::current_path() : fs::path(dir);
    fs::path manifest = projBase / "nexapkg.json";
    if (!fs::exists(manifest)) {
        std::cerr << "[nexapkg] nexapkg.json not found. Run 'nexapkg init' first.\n";
        return 1;
    }
    Manifest m;
    readManifest(manifest, m);

    fs::path pkgDir = global ? fs::path(getPackagesDir()) : (projBase / ".nexa" / "packages");
    std::error_code ec;
    fs::create_directories(pkgDir, ec);

    fs::path lockPath = projBase / "nexapkg.lock";
    Lock lk;
    readLock(lockPath, lk);

    int failures = 0, done = 0;
    for (const auto& [includePath, source] : m.deps) {
        if (!onlyName.empty() && includePath != onlyName) continue;
        std::string pinCommit;
        if (!updateMode) {
            if (LockEntry* le = lk.find(includePath)) {
                if (le->source == source) pinCommit = le->commit;  // reproduce locked commit
            }
        }
        std::string commit;
        if (installDep(includePath, source, pkgDir, projBase, pinCommit, force || updateMode, commit)) {
            done++;
            if (source.rfind("file:", 0) != 0) {
                LockEntry le;
                le.source = source;
                // Keep prior commit if nothing new was resolved (e.g. skipped).
                if (commit.empty()) {
                    if (LockEntry* prev = lk.find(includePath)) le.commit = prev->commit;
                    if (!pinCommit.empty() && le.commit.empty()) le.commit = pinCommit;
                } else {
                    le.commit = commit;
                }
                lk.set(includePath, le);
            }
        } else {
            failures++;
        }
    }
    if (!global) writeLock(lockPath, lk);

    if (!onlyName.empty() && done == 0 && failures == 0) {
        std::cerr << "[nexapkg] Dependency not found in manifest: " << onlyName << "\n";
        return 1;
    }
    return failures == 0 ? 0 : 1;
}

static int cmdList(const std::string& dir) {
    fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
    fs::path manifest = base / "nexapkg.json";
    if (!fs::exists(manifest)) {
        std::cerr << "[nexapkg] nexapkg.json not found.\n";
        return 1;
    }
    Manifest m;
    readManifest(manifest, m);
    Lock lk;
    readLock(base / "nexapkg.lock", lk);
    fs::path pkgDir = base / ".nexa" / "packages";

    std::cout << m.name << (m.version.empty() ? "" : (" " + m.version)) << "\n";
    if (m.deps.empty()) {
        std::cout << "  (no dependencies)\n";
        return 0;
    }
    for (const auto& [k, v] : m.deps) {
        bool installed = fs::exists(pkgDir / k) && !fs::is_empty(pkgDir / k);
        std::cout << "  " << k << " <- " << v;
        if (LockEntry* le = lk.find(k)) {
            if (!le->commit.empty()) std::cout << " [" << le->commit.substr(0, 7) << "]";
        }
        std::cout << (installed ? "" : "  (not installed)") << "\n";
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Targets: platforms installed for NexaC --target
// ----------------------------------------------------------------------------
//
// A target package is a directory holding target.json and the source it names,
// and it is installed whole into ~/.nexa/targets/<name>/. They come out of one
// repository with a directory per platform, and a sparse checkout fetches only
// the one being installed -- so a repository that grows more platforms does not
// make installing any one of them slower.

static const char* kDefaultTargetRepo = "Bobism-2009/Nexa-Targets";

static fs::path getTargetsDir() {
    return fs::path(getHome()) / ".nexa" / "targets";
}

static fs::path getTargetCacheDir(const std::string& name) {
    return fs::path(getHome()) / ".nexa" / "cache" / "targets" / name;
}

static const char* quiet() {
#ifdef _WIN32
    return " >NUL 2>NUL";
#else
    return " >/dev/null 2>/dev/null";
#endif
}

static std::string readTargetField(const fs::path& dir, const std::string& key) {
    std::ifstream in(dir / "target.json", std::ios::binary);
    if (!in) return std::string();
    std::stringstream ss;
    ss << in.rdbuf();
    return findStringField(ss.str(), key);
}

// Replaces whatever is installed under this name with src. The compiled
// runtime in the cache goes too: it was built from the sources being replaced.
static bool placeTarget(const fs::path& src, const std::string& name) {
    std::error_code ec;
    fs::path dest = getTargetsDir() / name;
    fs::remove_all(dest, ec);
    fs::remove_all(getTargetCacheDir(name), ec);
    fs::create_directories(dest.parent_path(), ec);
    fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec || !fs::exists(dest / "target.json")) {
        std::cerr << "[nexapkg] Could not install into " << dest.string()
                  << (ec ? (": " + ec.message()) : std::string()) << "\n";
        return false;
    }
    std::string ver = readTargetField(dest, "version");
    std::string desc = readTargetField(dest, "description");
    std::cout << "[nexapkg] Installed target " << name << (ver.empty() ? "" : (" " + ver)) << "\n";
    if (!desc.empty()) std::cout << "[nexapkg]   " << desc << "\n";
    std::cout << "[nexapkg]   Build with: NexaC file.nxa --target " << name << "\n";
    std::cout << "[nexapkg]   The first build compiles the target's runtime from source, once.\n";
    return true;
}

static int cmdTargetInstall(const std::string& name, const std::string& from, bool force) {
    if (name.empty() || name.find_first_of("/\\.") != std::string::npos) {
        std::cerr << "[nexapkg] target install: '" << name << "' is not a target name\n";
        return 1;
    }
    if (fs::exists(getTargetsDir() / name / "target.json") && !force) {
        std::cout << "[nexapkg] Target " << name << " is already installed (use 'nexapkg target update "
                  << name << "' or --force)\n";
        return 0;
    }

    // A directory on disk: for working on a package before it is published.
    // Either the target's own directory, or a repository holding it.
    if (!from.empty() && fs::is_directory(from)) {
        fs::path base = fs::absolute(from);
        fs::path src = fs::exists(base / "target.json") ? base : base / name;
        if (!fs::exists(src / "target.json")) {
            std::cerr << "[nexapkg] No target.json in " << src.string() << "\n";
            return 1;
        }
        return placeTarget(src, name) ? 0 : 1;
    }

    std::string repo = from.empty() ? kDefaultTargetRepo : from;
    std::string url, ref;
    splitSourceRef(repo, url, ref);
    if (url.find("http") != 0) url = "https://github.com/" + url;
    if (!gitAvailable()) {
        std::cerr << "[nexapkg] git not found; cannot fetch targets from " << url << "\n";
        return 1;
    }

    fs::path tmp = fs::temp_directory_path() / ("nexapkg_target_" + name);
    std::error_code ec;
    fs::remove_all(tmp, ec);
    std::string branch = ref.empty() ? "" : (" --branch \"" + ref + "\"");
    std::cout << "[nexapkg] Fetching target " << name << " from " << url << (ref.empty() ? "" : ("@" + ref)) << "...\n";
    // Only this target's directory is checked out. git older than 2.25 has no
    // sparse checkout, and gets the whole repository instead.
    bool sparse = std::system(("git clone --depth 1 --filter=blob:none --sparse" + branch + " \"" + url +
                               "\" \"" + tmp.string() + "\"" + quiet()).c_str()) == 0;
    if (sparse) {
        sparse = std::system(("git -C \"" + tmp.string() + "\" sparse-checkout set \"" + name + "\"" +
                              quiet()).c_str()) == 0;
    }
    if (!sparse) {
        fs::remove_all(tmp, ec);
        if (std::system(("git clone --depth 1" + branch + " \"" + url + "\" \"" + tmp.string() + "\"" +
                         quiet()).c_str()) != 0) {
            std::cerr << "[nexapkg] Failed to fetch " << url << "\n";
            return 1;
        }
    }

    fs::path src = tmp / name;
    if (!fs::exists(src / "target.json")) {
        std::cerr << "[nexapkg] " << url << " has no target named '" << name << "'\n";
        // The tree is known even where its files were not checked out.
        std::string all = runCapture("git -C \"" + tmp.string() + "\" ls-tree --name-only HEAD");
        std::stringstream ls(all);
        std::string line, avail;
        while (std::getline(ls, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (runCapture("git -C \"" + tmp.string() + "\" ls-tree --name-only HEAD \"" + line + "/target.json\"").empty()) continue;
            avail += " " + line;
        }
        if (!avail.empty()) std::cerr << "[nexapkg] Available:" << avail << "\n";
        fs::remove_all(tmp, ec);
        return 1;
    }
    bool ok = placeTarget(src, name);
    fs::remove_all(tmp, ec);
    return ok ? 0 : 1;
}

static int cmdTargetList() {
    std::error_code ec;
    std::vector<std::string> names;
    if (fs::exists(getTargetsDir(), ec)) {
        for (const auto& e : fs::directory_iterator(getTargetsDir(), ec)) {
            if (e.is_directory() && fs::exists(e.path() / "target.json")) names.push_back(e.path().filename().string());
        }
    }
    if (names.empty()) {
        std::cout << "[nexapkg] No targets installed. Windows, Linux, macOS and --wasm are built in;\n"
                     "[nexapkg] others install with: nexapkg target install <name>   (e.g. arm64-linux)\n";
        return 0;
    }
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) {
        fs::path d = getTargetsDir() / n;
        std::string ver = readTargetField(d, "version");
        std::string desc = readTargetField(d, "description");
        bool built = fs::exists(getTargetCacheDir(n), ec);
        std::cout << "  " << n << (ver.empty() ? "" : ("  " + ver)) << (built ? "" : "  (runtime not built yet)") << "\n";
        if (!desc.empty()) std::cout << "      " << desc << "\n";
    }
    return 0;
}

static int cmdTargetRemove(const std::string& name) {
    std::error_code ec;
    fs::path d = getTargetsDir() / name;
    if (name.empty() || !fs::exists(d / "target.json")) {
        std::cerr << "[nexapkg] No target named '" << name << "' is installed\n";
        return 1;
    }
    fs::remove_all(d, ec);
    fs::remove_all(getTargetCacheDir(name), ec);
    std::cout << "[nexapkg] Removed target " << name << " and its compiled runtime\n";
    return 0;
}

static int cmdTarget(const std::vector<std::string>& pos, bool force, const std::string& from) {
    if (pos.empty() || pos[0] == "list" || pos[0] == "ls") return cmdTargetList();
    const std::string& sub = pos[0];
    std::string name = pos.size() >= 2 ? pos[1] : "";
    if (sub == "install" || sub == "add") return cmdTargetInstall(name, from, force);
    if (sub == "update") return cmdTargetInstall(name, from, true);
    if (sub == "remove" || sub == "rm" || sub == "uninstall") return cmdTargetRemove(name);
    std::cerr << "[nexapkg] Unknown target command: " << sub << " (install, update, list, remove)\n";
    return 1;
}

// ----------------------------------------------------------------------------
// Argument dispatch
// ----------------------------------------------------------------------------
static void printHelp() {
    std::cout <<
        "nexapkg - Nexa package manager\n"
        "\n"
        "Usage:\n"
        "  nexapkg init [dir]                 Create nexapkg.json (name = folder)\n"
        "  nexapkg add <spec> [dir]           Add a dependency to the manifest\n"
        "  nexapkg add as <path> <spec> [dir] Add under a custom include path\n"
        "  nexapkg remove <name> [dir]        Remove a dependency (alias: rm, uninstall)\n"
        "  nexapkg install [--global] [--force] [dir]\n"
        "                                     Install all manifest dependencies\n"
        "  nexapkg install <spec> [dir]       Add then install a single dependency\n"
        "  nexapkg update [name] [dir]        Re-fetch git dependencies, refresh lock\n"
        "  nexapkg list [dir]                 Show dependencies and lock state\n"
        "\n"
        "Targets (platforms for NexaC --target; Windows, Linux, macOS and --wasm are built in):\n"
        "  nexapkg target install <name>      Install one, e.g. arm64-linux\n"
        "  nexapkg target update <name>       Re-fetch it (its runtime is rebuilt on next use)\n"
        "  nexapkg target list                Show installed targets\n"
        "  nexapkg target remove <name>       Remove it and its compiled runtime\n"
        "    --from <owner/repo | ./dir>      Somewhere other than Bobism-2009/Nexa-Targets\n"
        "\n"
        "Specs:\n"
        "  owner/repo            GitHub repo (default branch)\n"
        "  owner/repo@v1.2.0     pin to a tag or branch\n"
        "  https://host/x.git    full git URL (optionally @ref)\n"
        "  ./path or /abs/path   local file or directory\n"
        "\n"
        "Use an installed package:  #include <name/module>\n";
}

// Collect positionals (non-flag args) and flags.
struct Args {
    std::vector<std::string> pos;
    bool global = false;
    bool force = false;
    std::string from;  // nexapkg target install --from <repo | dir>
};

static Args parseArgs(int argc, char* argv[], int start) {
    Args a;
    for (int i = start; i < argc; i++) {
        std::string s = argv[i];
        if (s == "--global") a.global = true;
        else if (s == "--force" || s == "-f") a.force = true;
        else if (s == "--from" && i + 1 < argc) a.from = argv[++i];
        else a.pos.push_back(s);
    }
    return a;
}

// A spec references a package (owner/repo, URL, ./local, name@ref); anything else is a dir.
static bool looksLikeSpec(const std::string& s) {
    if (s == "." || s == "..") return false;
    return s.find('/') != std::string::npos || s.rfind("http", 0) == 0 || s.find('@') != std::string::npos;
}

static int run(int argc, char* argv[]) {
    if (argc < 2) { printHelp(); return 0; }
    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") { printHelp(); return 0; }

    Args a = parseArgs(argc, argv, 2);

    if (cmd == "init") {
        return cmdInit(a.pos.empty() ? "" : a.pos[0]);
    }
    if (cmd == "add") {
        if (a.pos.empty()) { std::cerr << "[nexapkg] add: missing <spec>\n"; return 1; }
        // forms: add <spec> [dir]   |   add as <path> <spec> [dir]
        if (a.pos[0] == "as") {
            if (a.pos.size() < 3) { std::cerr << "[nexapkg] add as <path> <spec>\n"; return 1; }
            std::string asPath = a.pos[1], spec = a.pos[2];
            std::string dir = a.pos.size() >= 4 ? a.pos[3] : "";
            return cmdAdd(spec, dir, asPath);
        }
        std::string spec = a.pos[0];
        std::string dir = a.pos.size() >= 2 ? a.pos[1] : "";
        return cmdAdd(spec, dir, "");
    }
    if (cmd == "remove" || cmd == "rm" || cmd == "uninstall") {
        if (a.pos.empty()) { std::cerr << "[nexapkg] remove: missing <name>\n"; return 1; }
        std::string name = a.pos[0];
        std::string dir = a.pos.size() >= 2 ? a.pos[1] : "";
        return cmdRemove(name, dir);
    }
    if (cmd == "install") {
        // Optional single spec to add-then-install, plus optional dir.
        std::string spec, dir;
        for (const std::string& p : a.pos) {
            if (looksLikeSpec(p)) { if (spec.empty()) spec = p; }
            else if (dir.empty()) dir = p;
        }
        if (!spec.empty()) {
            if (a.global) {
                // Install a single spec directly into the global store (no manifest).
                fs::path projBase = dir.empty() ? fs::current_path() : fs::path(dir);
                ResolvedSpec r = resolveSpec(spec, projBase);
                std::string commit;
                bool ok = installDep(r.includePath, r.source, fs::path(getPackagesDir()), projBase,
                                     "", a.force, commit);
                return ok ? 0 : 1;
            }
            // Auto-create a manifest so 'install <spec>' works in a fresh project.
            fs::path base = dir.empty() ? fs::current_path() : fs::path(dir);
            if (!fs::exists(base / "nexapkg.json")) cmdInit(dir);
            if (cmdAdd(spec, dir, "") != 0) return 1;
        }
        return installAll(dir, a.global, a.force, false, "");
    }
    if (cmd == "update") {
        std::string name, dir;
        for (const std::string& p : a.pos) {
            if (p == "." || p == ".." || p.find('/') != std::string::npos) { if (dir.empty()) dir = p; }
            else if (name.empty()) name = p;
        }
        return installAll(dir, a.global, true, true, name);
    }
    if (cmd == "target" || cmd == "targets") {
        return cmdTarget(a.pos, a.force, a.from);
    }
    if (cmd == "list" || cmd == "ls") {
        return cmdList(a.pos.empty() ? "" : a.pos[0]);
    }
    std::cerr << "[nexapkg] Unknown command: " << cmd << "\n";
    printHelp();
    return 1;
}

}  // namespace pkg
}  // namespace nexa
