#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

namespace nexa {
namespace upgrade {

namespace fs = std::filesystem;

static const char* kRepoApi = "https://api.github.com/repos/Bobism-2009/Nexa-Lang/releases/latest";

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

static bool endsWithI(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    return iequals(s.substr(s.size() - suf.size()), suf);
}

static bool containsI(const std::string& s, const std::string& needle) {
    if (needle.empty()) return true;
    std::string a = s, b = needle;
    for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return a.find(b) != std::string::npos;
}

static std::string jsonStringField(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t i = json.find(needle);
    if (i == std::string::npos) return "";
    i += needle.size();
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r' || json[i] == ':')) i++;
    if (i >= json.size() || json[i] != '"') return "";
    i++;
    std::string out;
    while (i < json.size() && json[i] != '"') {
        if (json[i] == '\\' && i + 1 < json.size()) {
            out += json[i + 1];
            i += 2;
        } else {
            out += json[i++];
        }
    }
    return out;
}

struct SemVer {
    int a = 0, b = 0, c = 0;
};

static SemVer parseVer(std::string s) {
    s = trim(s);
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s = s.substr(1);
    const std::string prefix = "NexaC ";
    if (s.size() > prefix.size() && iequals(s.substr(0, prefix.size()), prefix)) s = s.substr(prefix.size());
    SemVer v;
    int* parts[3] = { &v.a, &v.b, &v.c };
    size_t i = 0;
    for (int p = 0; p < 3 && i < s.size(); p++) {
        while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        int n = 0;
        bool any = false;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            any = true;
            n = n * 10 + (s[i] - '0');
            i++;
        }
        if (any) *parts[p] = n;
    }
    return v;
}

static int cmpVer(SemVer x, SemVer y) {
    if (x.a != y.a) return x.a < y.a ? -1 : 1;
    if (x.b != y.b) return x.b < y.b ? -1 : 1;
    if (x.c != y.c) return x.c < y.c ? -1 : 1;
    return 0;
}

static std::string verStr(SemVer v) {
    return std::to_string(v.a) + "." + std::to_string(v.b) + "." + std::to_string(v.c);
}

static int runCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static bool cmdExists(const std::string& name) {
#ifdef _WIN32
    return runCmd("where " + name + " >nul 2>&1") == 0;
#else
    return runCmd("command -v " + name + " >/dev/null 2>&1") == 0;
#endif
}

static std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else o += c;
    }
    o += "\"";
    return o;
}

static bool downloadUrl(const std::string& url, const fs::path& dest, std::string& err) {
    fs::create_directories(dest.parent_path());
    const std::string out = dest.string();
#ifdef _WIN32
    if (cmdExists("curl")) {
        std::string cmd = "curl.exe -fsSL --retry 3 --retry-delay 1 -A NexaC-upgrade -o " + q(out) + " " + q(url);
        if (runCmd(cmd) == 0 && fs::exists(dest) && fs::file_size(dest) > 0) return true;
    }
    std::string ps =
        "powershell -NoProfile -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
        "Invoke-WebRequest -UseBasicParsing -UserAgent 'NexaC-upgrade' -Uri " + q(url) + " -OutFile " + q(out) + "\"";
    if (runCmd(ps) == 0 && fs::exists(dest) && fs::file_size(dest) > 0) return true;
    err = "download failed (curl / PowerShell)";
    return false;
#else
    if (cmdExists("curl")) {
        std::string cmd = "curl -fsSL --retry 3 --retry-delay 1 -A NexaC-upgrade -o " + q(out) + " " + q(url);
        if (runCmd(cmd) == 0 && fs::exists(dest) && fs::file_size(dest) > 0) return true;
    }
    if (cmdExists("wget")) {
        std::string cmd = "wget -q -O " + q(out) + " " + q(url);
        if (runCmd(cmd) == 0 && fs::exists(dest) && fs::file_size(dest) > 0) return true;
    }
    err = "download failed (need curl or wget)";
    return false;
#endif
}

static bool extractArchive(const fs::path& archive, const fs::path& dest, std::string& err) {
    fs::create_directories(dest);
    const std::string a = archive.string();
    const std::string d = dest.string();
#ifdef _WIN32
    if (cmdExists("tar")) {
        std::string cmd = "tar -xf " + q(a) + " -C " + q(d);
        if (runCmd(cmd) == 0) return true;
    }
    std::string ps = "powershell -NoProfile -Command \"Expand-Archive -Force -LiteralPath " + q(a) + " -DestinationPath " + q(d) + "\"";
    if (runCmd(ps) == 0) return true;
    err = "could not extract archive (tar / Expand-Archive)";
    return false;
#else
    if (endsWithI(a, ".zip") && cmdExists("unzip")) {
        if (runCmd("unzip -o -q " + q(a) + " -d " + q(d)) == 0) return true;
    }
    if (cmdExists("tar")) {
        if (runCmd("tar -xf " + q(a) + " -C " + q(d)) == 0) return true;
    }
    if (cmdExists("python3")) {
        std::string py = "python3 -c \"import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])\" "
            + q(a) + " " + q(d);
        if (runCmd(py) == 0) return true;
    }
    err = "could not extract archive (unzip / tar)";
    return false;
#endif
}

#ifdef _WIN32
static const char* kBinName = "NexaC.exe";
#else
static const char* kBinName = "NexaC";
#endif

static std::string findNexaCBinary(const fs::path& root) {
    if (!fs::exists(root)) return "";
    std::string found;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file()) continue;
        std::string name = e.path().filename().string();
        if (iequals(name, kBinName)) return e.path().string();
        if (found.empty() && containsI(name, "nexac") && !containsI(name, "installer")
#ifdef _WIN32
            && endsWithI(name, ".exe")
#endif
        ) {
            found = e.path().string();
        }
    }
    return found;
}

#ifdef _WIN32
static bool isAdmin() {
    BOOL admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID group = nullptr;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(nullptr, group, &admin);
        FreeSid(group);
    }
    return admin == TRUE;
}
#else
static bool isAdmin() {
    return geteuid() == 0;
}
#endif

static bool pathNeedsAdmin(const fs::path& p) {
    std::string s = fs::absolute(p).string();
    for (char& c : s) {
#ifdef _WIN32
        if (c == '/') c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
    }
#ifdef _WIN32
    return s.rfind("c:\\program files", 0) == 0 || s.rfind("c:\\windows", 0) == 0;
#else
    return s.rfind("/usr/", 0) == 0 || s.rfind("/bin/", 0) == 0 || s.rfind("/sbin/", 0) == 0
        || s == "/usr/local/bin/NexaC" || s.rfind("/opt/", 0) == 0;
#endif
}

static fs::path systemInstallDir() {
#ifdef _WIN32
    return fs::path("C:\\Program Files\\NexaC");
#else
    return fs::path("/usr/local/bin");
#endif
}

static bool copyFileOverwrite(const fs::path& src, const fs::path& dest, std::string& err) {
    try {
        fs::create_directories(dest.parent_path());
        fs::path old = dest;
        old += ".old";
        std::error_code ec;
        fs::remove(old, ec);
        if (fs::exists(dest)) {
            fs::rename(dest, old, ec);
            if (ec) {
                fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    err = "cannot write " + dest.string() + ": " + ec.message();
                    return false;
                }
                fs::remove(old, ec);
                return true;
            }
        }
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err = "cannot write " + dest.string() + ": " + ec.message();
            if (fs::exists(old)) fs::rename(old, dest, ec);
            return false;
        }
        fs::remove(old, ec);
#ifndef _WIN32
        fs::permissions(dest, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec
            | fs::perms::others_read | fs::perms::others_exec, fs::perm_options::replace, ec);
#endif
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

#ifdef _WIN32
static void copySiblingAliases(const fs::path& srcExe, const fs::path& destDir) {
    std::string err;
    copyFileOverwrite(srcExe, destDir / "NexaC.exe", err);
    copyFileOverwrite(srcExe, destDir / "nexac.exe", err);
    copyFileOverwrite(srcExe, destDir / "nexapkg.exe", err);
}
#else
static void copySiblingAliases(const fs::path& srcExe, const fs::path& destDir) {
    std::string err;
    fs::path dest = destDir / "NexaC";
    copyFileOverwrite(srcExe, dest, err);
    std::error_code ec;
    fs::remove(destDir / "nexac", ec);
    fs::remove(destDir / "nexapkg", ec);
    fs::create_symlink("NexaC", destDir / "nexac", ec);
    fs::create_symlink("NexaC", destDir / "nexapkg", ec);
}
#endif

static bool replaceCurrent(const fs::path& newBin, const fs::path& currentExe, std::string& err) {
    if (!copyFileOverwrite(newBin, currentExe, err)) return false;
    fs::path dir = currentExe.parent_path();
#ifdef _WIN32
    std::error_code ec;
    if (fs::exists(dir / "nexac.exe")) copyFileOverwrite(newBin, dir / "nexac.exe", err);
    if (fs::exists(dir / "nexapkg.exe")) copyFileOverwrite(newBin, dir / "nexapkg.exe", err);
    (void)ec;
#else
    std::error_code ec;
    if (fs::is_symlink(dir / "nexac") || fs::exists(dir / "nexac")) {
        fs::remove(dir / "nexac", ec);
        fs::create_symlink(currentExe.filename(), dir / "nexac", ec);
    }
    if (fs::is_symlink(dir / "nexapkg") || fs::exists(dir / "nexapkg")) {
        fs::remove(dir / "nexapkg", ec);
        fs::create_symlink(currentExe.filename(), dir / "nexapkg", ec);
    }
#endif
    return true;
}

static bool stdinIsTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(0) != 0;
#endif
}

static bool askYes(const std::string& prompt, bool defaultNo) {
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    line = trim(line);
    if (line.empty()) return !defaultNo ? true : false;
    char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
    return c == 'y';
}

struct Latest {
    std::string tag;
    SemVer ver;
    std::string assetName;
    std::string url;
};

static int assetScore(const std::string& name) {
#ifdef _WIN32
    if (containsI(name, "linux") || containsI(name, "darwin") || containsI(name, "macos")) return -1;
    int s = 0;
    if (endsWithI(name, ".zip")) s += 10;
    if (containsI(name, "installer")) s += 5;
    if (containsI(name, "windows") || containsI(name, "win64") || containsI(name, "win32")) s += 4;
    if (iequals(name, "NexaC.exe")) s += 8;
    return s;
#elif defined(__APPLE__)
    if (containsI(name, "windows") || containsI(name, "win64") || containsI(name, "installer")) return -1;
    if (containsI(name, "linux")) return -1;
    int s = 0;
    if (containsI(name, "darwin") || containsI(name, "macos") || containsI(name, "osx")) s += 10;
    if (endsWithI(name, ".zip") || endsWithI(name, ".tar.gz")) s += 3;
    return s > 0 ? s : -1;
#else
    if (containsI(name, "windows") || containsI(name, "win64") || containsI(name, "installer")) return -1;
    if (containsI(name, "darwin") || containsI(name, "macos")) return -1;
    int s = 0;
    if (containsI(name, "linux")) s += 10;
    if (endsWithI(name, ".tar.gz") || endsWithI(name, ".zip")) s += 3;
    return s > 0 ? s : -1;
#endif
}

static bool fetchLatest(Latest& out, std::string& err) {
    fs::path tmp = fs::temp_directory_path() / "nexa-latest.json";
    if (!downloadUrl(kRepoApi, tmp, err)) return false;
    std::ifstream in(tmp);
    std::stringstream buf;
    buf << in.rdbuf();
    in.close();
    std::error_code ec;
    fs::remove(tmp, ec);
    std::string json = buf.str();
    if (json.empty()) {
        err = "empty response from GitHub";
        return false;
    }
    out.tag = jsonStringField(json, "tag_name");
    if (out.tag.empty()) {
        err = "could not read latest release tag";
        return false;
    }
    out.ver = parseVer(out.tag);

    std::string bestName, bestUrl;
    int best = -1;
    size_t pos = 0;
    for (;;) {
        size_t u = json.find("\"browser_download_url\"", pos);
        if (u == std::string::npos) break;
        std::string url = jsonStringField(json.substr(u), "browser_download_url");
        std::string name = url;
        size_t sl = name.find_last_of('/');
        if (sl != std::string::npos) name = name.substr(sl + 1);
        int sc = assetScore(name);
        if (sc > best) {
            best = sc;
            bestName = name;
            bestUrl = url;
        }
        pos = u + 1;
    }
    if (best < 0 || bestUrl.empty()) {
#ifdef _WIN32
        err = "latest release has no Windows NexaC download";
#else
        err = "latest release has no NexaC build for this OS";
#endif
        return false;
    }
    out.assetName = bestName;
    out.url = bestUrl;
    return true;
}

static int relaunchElevated(const std::string& selfExe, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string argList;
    for (size_t i = 0; i < args.size(); i++) {
        if (i) argList += ",";
        std::string a = args[i];
        for (char& c : a) if (c == '\'') c = ' ';
        argList += "'" + a + "'";
    }
    std::string ps = "powershell -NoProfile -Command \"Start-Process -FilePath '" + selfExe
        + "' -ArgumentList @(" + argList + ") -Verb RunAs -Wait\"";
    std::cout << "[Nexa] Windows will ask for administrator permission (UAC)...\n";
    int rc = runCmd(ps);
    return rc == 0 ? 0 : 1;
#else
    std::string cmd = "sudo -p \"[Nexa] administrator password: \" ";
    cmd += q(selfExe);
    for (const std::string& a : args) {
        cmd += " ";
        cmd += q(a);
    }
    return runCmd(cmd) == 0 ? 0 : 1;
#endif
}

static int applyFromDir(const fs::path& extractDir, const fs::path& replaceExe, bool allUsers) {
    std::string newBin = findNexaCBinary(extractDir);
    if (newBin.empty()) {
        std::cerr << "[Nexa] Error: extracted archive has no " << kBinName << "\n";
        return 1;
    }
    std::string err;
    if (!replaceExe.empty()) {
        std::cout << "[Nexa] Updating " << replaceExe.string() << "\n";
        if (!replaceCurrent(newBin, replaceExe, err)) {
            std::cerr << "[Nexa] Error: " << err << "\n";
            return 1;
        }
    }
    if (allUsers) {
        fs::path sysDir = systemInstallDir();
        std::cout << "[Nexa] Installing for all users: " << sysDir.string() << "\n";
        copySiblingAliases(newBin, sysDir);
#ifdef _WIN32
        std::cout << "[Nexa] System NexaC is " << (sysDir / "NexaC.exe").string() << "\n";
#else
        std::cout << "[Nexa] System NexaC is " << (sysDir / "NexaC").string() << "\n";
#endif
    }
    std::cout << "[Nexa] Upgrade complete. Restart the terminal, then run: NexaC --version\n";
    return 0;
}

}  // namespace upgrade

static int doUpgrade(int argc, char* argv[], const std::string& selfExe, const std::string& currentVer) {
    using namespace upgrade;
    bool checkOnly = false;
    bool assumeYes = false;
    bool askAllUsers = true;
    bool allUsers = false;
    bool finish = false;
    std::string fromDir;
    std::string replacePath = selfExe;

    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: NexaC upgrade [--check] [--yes] [--user | --all-users]\n";
            std::cout << "  Checks GitHub for a newer NexaC, asks before installing, and can\n";
            std::cout << "  update all users (Windows UAC / sudo).\n";
            return 0;
        } else if (a == "--check") checkOnly = true;
        else if (a == "--yes" || a == "-y") assumeYes = true;
        else if (a == "--user") { askAllUsers = false; allUsers = false; }
        else if (a == "--all-users" || a == "--system") { askAllUsers = false; allUsers = true; }
        else if (a == "--finish" || a == "--apply") finish = true;
        else if (a == "--from") {
            if (i + 1 >= argc) {
                std::cerr << "[Nexa] Error: --from requires a directory\n";
                return 1;
            }
            fromDir = argv[++i];
        } else if (a == "--replace") {
            if (i + 1 >= argc) {
                std::cerr << "[Nexa] Error: --replace requires a path\n";
                return 1;
            }
            replacePath = argv[++i];
        } else {
            std::cerr << "[Nexa] Error: unknown upgrade option '" << a << "'\n";
            std::cerr << "Usage: NexaC upgrade [--check] [--yes] [--user | --all-users]\n";
            return 1;
        }
    }

    if (finish) {
        if (fromDir.empty()) {
            std::cerr << "[Nexa] Error: upgrade --finish needs --from <dir>\n";
            return 1;
        }
        if ((allUsers || pathNeedsAdmin(replacePath)) && !isAdmin()) {
            std::cerr << "[Nexa] Error: administrator permission is required for this install location\n";
            return 1;
        }
        return applyFromDir(fromDir, replacePath, allUsers);
    }

    std::cout << "[Nexa] Checking for a newer NexaC...\n";
    std::cout.flush();
    Latest latest;
    std::string err;
    if (!fetchLatest(latest, err)) {
        std::cerr << "[Nexa] Error: " << err << "\n";
        return 1;
    }
    SemVer cur = parseVer(currentVer);
    int cmp = cmpVer(latest.ver, cur);
    std::cout << "[Nexa] Running " << verStr(cur) << "; latest release is " << verStr(latest.ver)
              << " (" << latest.tag << ")\n";
    if (cmp <= 0) {
        if (cmp == 0) std::cout << "[Nexa] Already up to date.\n";
        else std::cout << "[Nexa] You already have a newer build than the latest release. No upgrade needed.\n";
        return 0;
    }
    if (checkOnly) {
        std::cout << "[Nexa] " << latest.assetName << "\n";
        std::cout << "[Nexa] " << latest.url << "\n";
        return 0;
    }

    if (!assumeYes) {
        if (!stdinIsTty()) {
            std::cerr << "[Nexa] Error: not a terminal. Re-run with --yes to install "
                      << verStr(latest.ver) << ".\n";
            return 1;
        }
        if (!askYes("Install NexaC " + verStr(latest.ver) + "? [y/N] ", true)) {
            std::cout << "[Nexa] Upgrade cancelled.\n";
            return 0;
        }
    } else {
        std::cout << "[Nexa] Installing NexaC " << verStr(latest.ver) << "\n";
    }

    if (askAllUsers) {
        if (stdinIsTty()) {
            allUsers = askYes("Update for all users? This asks for administrator permission. [y/N] ", true);
        } else {
            allUsers = false;
        }
    }

    fs::path work = fs::temp_directory_path() / ("nexa-upgrade-" + verStr(latest.ver));
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work);
    fs::path zipPath = work / latest.assetName;
    fs::path extractDir = work / "extract";

    std::cout << "[Nexa] Downloading " << latest.assetName << "...\n";
    if (!downloadUrl(latest.url, zipPath, err)) {
        std::cerr << "[Nexa] Error: " << err << "\n";
        fs::remove_all(work, ec);
        return 1;
    }
    std::cout << "[Nexa] Extracting...\n";
    if (!extractArchive(zipPath, extractDir, err)) {
        std::cerr << "[Nexa] Error: " << err << "\n";
        fs::remove_all(work, ec);
        return 1;
    }

    const bool needAdmin = allUsers || pathNeedsAdmin(replacePath);
    if (needAdmin && !isAdmin()) {
        std::vector<std::string> args = {
            "upgrade", "--finish", "--from", extractDir.string(), "--replace", replacePath
        };
        if (allUsers) args.push_back("--all-users");
        int rc = relaunchElevated(selfExe, args);
        fs::remove_all(work, ec);
        if (rc != 0) {
            std::cerr << "[Nexa] Error: administrator upgrade did not finish (UAC cancelled or failed)\n";
            return 1;
        }
        std::cout << "[Nexa] Upgrade complete. Restart the terminal, then run: NexaC --version\n";
        return 0;
    }

    int rc = applyFromDir(extractDir, replacePath, allUsers);
    fs::remove_all(work, ec);
    return rc;
}

}  // namespace nexa
