#pragma once

#include <string>
#include <set>
#include <sstream>
#include "CryptoRuntime.hpp"
#include "FileRuntime.hpp"
#include "HttpRuntime.hpp"

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
        bool osExec = false;
        bool osGetenv = false;
        bool osPlatform = false;
        bool osExeDir = false;
        bool osGetProcessId = false;
        bool osWindowControl = false;
        bool osMessageBox = false;
        bool osGrepKeys = false;
        bool osKeyPressed = false;
        bool osType = false;
        bool osLock = false;
        bool osShutdown = false;
        bool osReboot = false;
        bool osSuspend = false;
        bool osLogout = false;
        bool osAudio = false;
        bool osBrightness = false;
        bool osClipboard = false;
        bool osDesktop = false;
        bool osExit = false;
        bool osHostname = false;
        bool osUsername = false;
        bool osHome = false;
        bool osSetenv = false;
        bool file = false;
        bool random = false;
        bool math = false;
        bool crypto = false;
        bool http = false;
        bool str = false;
        bool time = false;
        bool thread = false;
        bool threadLambda = false;
        bool threadWorker = false;
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

    bool hasCrypto() const {
        return enabled_.count("std/crypto") > 0;
    }

    bool hasHttp() const {
        return enabled_.count("std/http") > 0;
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
        if (hasOs() && (usage.osSystem || usage.osExec || usage.osGetenv || usage.osLock ||
                        usage.osShutdown || usage.osReboot || usage.osSuspend ||
                        usage.osLogout || usage.osAudio || usage.osBrightness ||
                        usage.osClipboard || usage.osDesktop || usage.osExit ||
                        usage.osSetenv || usage.osHome || usage.osUsername)) {
            out += "#include <cstdlib>\n";
        }
        if (hasOs() && (usage.osSystem || usage.osExec)) {
            out += "#include <cstdio>\n";
        }
        if (hasOs() && (usage.osGetenv || usage.osExec || usage.osPlatform || usage.osExeDir || usage.osMessageBox || usage.osGrepKeys ||
                        usage.osHostname || usage.osUsername || usage.osHome || usage.osSetenv)) {
            out += "#include <string>\n";
        }
        if (hasOs() && (usage.osHostname || usage.osUsername)) {
            out += "#ifdef _WIN32\n#include <windows.h>\n#else\n#include <unistd.h>\n#endif\n";
        }
        if (hasOs() && usage.osExec) {
            out += "static std::string __nexa_os_exec(const std::string& cmd) {\n";
            out += "  std::string result; char buf[4096]; size_t n;\n";
            out += "#ifdef _WIN32\n";
            out += "  FILE* p = _popen(cmd.c_str(), \"r\");\n";
            out += "#else\n";
            out += "  FILE* p = popen(cmd.c_str(), \"r\");\n";
            out += "#endif\n";
            out += "  if (!p) return std::string();\n";
            out += "  while ((n = fread(buf, 1, sizeof(buf), p)) > 0) result.append(buf, n);\n";
            out += "#ifdef _WIN32\n";
            out += "  _pclose(p);\n";
            out += "#else\n";
            out += "  pclose(p);\n";
            out += "#endif\n";
            out += "  return result;\n";
            out += "}\n";
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
        if (hasOs() && usage.osExit) {
            out += "static void __nexa_os_exit(int code) { std::exit(code); }\n";
        }
        if (hasOs() && usage.osHostname) {
            out += "static std::string __nexa_os_hostname() {\n";
            out += "#ifdef _WIN32\n";
            out += "  char buf[256]; DWORD n = (DWORD)sizeof(buf);\n";
            out += "  if (GetComputerNameA(buf, &n)) return std::string(buf);\n";
            out += "  return std::string();\n";
            out += "#else\n";
            out += "  char buf[256];\n";
            out += "  if (gethostname(buf, sizeof(buf)) == 0) { buf[sizeof(buf)-1] = 0; return std::string(buf); }\n";
            out += "  return std::string();\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osUsername) {
            out += "static std::string __nexa_os_username() {\n";
            out += "#ifdef _WIN32\n";
            out += "  char buf[256]; DWORD n = (DWORD)sizeof(buf);\n";
            out += "  if (GetUserNameA(buf, &n)) return std::string(buf);\n";
            out += "  const char* e = std::getenv(\"USERNAME\");\n";
            out += "  return e ? std::string(e) : std::string();\n";
            out += "#else\n";
            out += "  const char* e = std::getenv(\"USER\");\n";
            out += "  if (e && e[0]) return std::string(e);\n";
            out += "  e = std::getenv(\"LOGNAME\");\n";
            out += "  if (e && e[0]) return std::string(e);\n";
            out += "  char* login = getlogin();\n";
            out += "  return login ? std::string(login) : std::string();\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osHome) {
            out += "static std::string __nexa_os_home() {\n";
            out += "#ifdef _WIN32\n";
            out += "  const char* e = std::getenv(\"USERPROFILE\");\n";
            out += "  if (e && e[0]) return std::string(e);\n";
            out += "  const char* d = std::getenv(\"HOMEDRIVE\");\n";
            out += "  const char* p = std::getenv(\"HOMEPATH\");\n";
            out += "  if (d && p) return std::string(d) + std::string(p);\n";
            out += "  return std::string();\n";
            out += "#else\n";
            out += "  const char* e = std::getenv(\"HOME\");\n";
            out += "  return e ? std::string(e) : std::string();\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osSetenv) {
            out += "static void __nexa_os_setenv(const std::string& name, const std::string& value) {\n";
            out += "#ifdef _WIN32\n";
            out += "  _putenv_s(name.c_str(), value.c_str());\n";
            out += "#else\n";
            out += "  setenv(name.c_str(), value.c_str(), 1);\n";
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
        if (hasOs() && usage.osLock) {
            out += "static void __nexa_os_lock() {\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system(\"rundll32.exe user32.dll,LockWorkStation\");\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"/System/Library/CoreServices/Menu\\\\ Extras/User.menu/Contents/Resources/CGSession -suspend\");\n";
            out += "#else\n";
            out += "  if (system(\"loginctl lock-session 2>/dev/null\") == 0) return;\n";
            out += "  if (system(\"xdg-screensaver lock 2>/dev/null\") == 0) return;\n";
            out += "  if (system(\"gnome-screensaver-command -l 2>/dev/null\") == 0) return;\n";
            out += "  if (system(\"xscreensaver-command -lock 2>/dev/null\") == 0) return;\n";
            out += "  (void)system(\"dm-tool lock 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osShutdown) {
            out += "static void __nexa_os_shutdown() {\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system(\"shutdown /s /t 0\");\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"osascript -e 'tell application \\\"System Events\\\" to shut down'\");\n";
            out += "#else\n";
            out += "  if (system(\"systemctl poweroff 2>/dev/null\") == 0) return;\n";
            out += "  (void)system(\"shutdown -h now 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osReboot) {
            out += "static void __nexa_os_reboot() {\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system(\"shutdown /r /t 0\");\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"osascript -e 'tell application \\\"System Events\\\" to restart'\");\n";
            out += "#else\n";
            out += "  if (system(\"systemctl reboot 2>/dev/null\") == 0) return;\n";
            out += "  (void)system(\"shutdown -r now 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osSuspend) {
            out += "static void __nexa_os_suspend() {\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system(\"rundll32.exe powrprof.dll,SetSuspendState 0,1,0\");\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"pmset sleepnow\");\n";
            out += "#else\n";
            out += "  (void)system(\"systemctl suspend 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osLogout) {
            out += "static void __nexa_os_logout() {\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system(\"shutdown /l\");\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"osascript -e 'tell application \\\"System Events\\\" to log out'\");\n";
            out += "#else\n";
            out += "  if (system(\"loginctl terminate-user \\\"$USER\\\" 2>/dev/null\") == 0) return;\n";
            out += "  if (system(\"gnome-session-quit --logout --no-prompt 2>/dev/null\") == 0) return;\n";
            out += "  (void)system(\"xfce4-session-logout --logout 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osAudio) {
            out += "#include <string>\n";
            out += "#include <cstdio>\n";
            out += "#ifdef _WIN32\n";
            out += "#include <windows.h>\n";
            out += "#include <mmdeviceapi.h>\n";
            out += "#include <endpointvolume.h>\n";
            out += "static IAudioEndpointVolume* __nexa_win_audio() {\n";
            out += "  if (CoInitializeEx(NULL, COINIT_MULTITHREADED) < 0) {}\n";
            out += "  IMMDeviceEnumerator* en = NULL;\n";
            out += "  if (CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en) < 0 || !en) return NULL;\n";
            out += "  IMMDevice* dev = NULL; en->GetDefaultAudioEndpoint(eRender, eConsole, &dev); en->Release();\n";
            out += "  if (!dev) return NULL;\n";
            out += "  IAudioEndpointVolume* vol = NULL;\n";
            out += "  dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&vol); dev->Release();\n";
            out += "  return vol;\n";
            out += "}\n";
            out += "#endif\n";
            out += "static void __nexa_os_set_volume(int p) {\n";
            out += "  if (p < 0) p = 0; if (p > 100) p = 100;\n";
            out += "#ifdef _WIN32\n";
            out += "  IAudioEndpointVolume* v = __nexa_win_audio();\n";
            out += "  if (v) { v->SetMasterVolumeLevelScalar((float)p / 100.0f, NULL); v->Release(); }\n";
            out += "  CoUninitialize();\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  std::string c = \"osascript -e 'set volume output volume \" + std::to_string(p) + \"'\"; (void)system(c.c_str());\n";
            out += "#else\n";
            out += "  std::string s = std::to_string(p);\n";
            out += "  if (system((\"wpctl set-volume @DEFAULT_AUDIO_SINK@ \" + s + \"% 2>/dev/null\").c_str()) == 0) return;\n";
            out += "  if (system((\"pactl set-sink-volume @DEFAULT_SINK@ \" + s + \"% 2>/dev/null\").c_str()) == 0) return;\n";
            out += "  (void)system((\"amixer set Master \" + s + \"% 2>/dev/null\").c_str());\n";
            out += "#endif\n";
            out += "}\n";
            out += "static int __nexa_os_get_volume() {\n";
            out += "#ifdef _WIN32\n";
            out += "  IAudioEndpointVolume* v = __nexa_win_audio(); if (!v) { CoUninitialize(); return -1; }\n";
            out += "  float f = 0.0f; v->GetMasterVolumeLevelScalar(&f); v->Release(); CoUninitialize();\n";
            out += "  return (int)(f * 100.0f + 0.5f);\n";
            out += "#else\n";
            out += "  char buf[256]; int vol = -1;\n";
            out += "#ifdef __APPLE__\n";
            out += "  FILE* p = popen(\"osascript -e 'output volume of (get volume settings)' 2>/dev/null\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) vol = atoi(buf); pclose(p); }\n";
            out += "  return vol;\n";
            out += "#else\n";
            out += "  FILE* p = popen(\"wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) { double d = 0; if (sscanf(buf, \"Volume: %lf\", &d) == 1) vol = (int)(d * 100.0 + 0.5); } pclose(p); }\n";
            out += "  if (vol >= 0) return vol;\n";
            out += "  p = popen(\"amixer get Master 2>/dev/null | grep -o '[0-9]*%' | head -1 | tr -d '%'\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) vol = atoi(buf); pclose(p); }\n";
            out += "  return vol;\n";
            out += "#endif\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_set_mute(int on) {\n";
            out += "#ifdef _WIN32\n";
            out += "  IAudioEndpointVolume* v = __nexa_win_audio();\n";
            out += "  if (v) { v->SetMute(on ? TRUE : FALSE, NULL); v->Release(); }\n";
            out += "  CoUninitialize();\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(on ? \"osascript -e 'set volume output muted true'\" : \"osascript -e 'set volume output muted false'\");\n";
            out += "#else\n";
            out += "  const char* s = on ? \"1\" : \"0\";\n";
            out += "  if (system((std::string(\"wpctl set-mute @DEFAULT_AUDIO_SINK@ \") + s + \" 2>/dev/null\").c_str()) == 0) return;\n";
            out += "  if (system((std::string(\"pactl set-sink-mute @DEFAULT_SINK@ \") + s + \" 2>/dev/null\").c_str()) == 0) return;\n";
            out += "  (void)system(on ? \"amixer set Master mute 2>/dev/null\" : \"amixer set Master unmute 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_toggle_mute() {\n";
            out += "#ifdef _WIN32\n";
            out += "  IAudioEndpointVolume* v = __nexa_win_audio();\n";
            out += "  if (v) { BOOL m = FALSE; v->GetMute(&m); v->SetMute(m ? FALSE : TRUE, NULL); v->Release(); }\n";
            out += "  CoUninitialize();\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  (void)system(\"osascript -e 'set volume output muted not (output muted of (get volume settings))'\");\n";
            out += "#else\n";
            out += "  if (system(\"wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null\") == 0) return;\n";
            out += "  if (system(\"pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null\") == 0) return;\n";
            out += "  (void)system(\"amixer set Master toggle 2>/dev/null\");\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osBrightness) {
            out += "#include <string>\n";
            out += "#include <cstdio>\n";
            out += "static void __nexa_os_set_brightness(int p) {\n";
            out += "  if (p < 0) p = 0; if (p > 100) p = 100;\n";
            out += "  std::string s = std::to_string(p);\n";
            out += "#ifdef _WIN32\n";
            out += "  (void)system((\"powershell -NoProfile -Command \\\"(Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods).WmiSetBrightness(1,\" + s + \")\\\" >nul 2>&1\").c_str());\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  char b[32]; snprintf(b, sizeof(b), \"%.2f\", p / 100.0);\n";
            out += "  (void)system((std::string(\"brightness \") + b + \" 2>/dev/null\").c_str());\n";
            out += "#else\n";
            out += "  if (system((\"brightnessctl set \" + s + \"% 2>/dev/null >/dev/null\").c_str()) == 0) return;\n";
            out += "  if (system((\"test -n \\\"$(ls /sys/class/backlight 2>/dev/null)\\\" && for d in /sys/class/backlight/*; do m=$(cat \\\"$d/max_brightness\\\"); echo $((m * \" + s + \" / 100)) > \\\"$d/brightness\\\"; done 2>/dev/null\").c_str()) == 0) return;\n";
            out += "  char fb[16]; snprintf(fb, sizeof(fb), \"%.2f\", p / 100.0);\n";
            out += "  (void)system((std::string(\"o=$(xrandr 2>/dev/null | awk '/ connected/{print $1; exit}'); [ -n \\\"$o\\\" ] && xrandr --output \\\"$o\\\" --brightness \") + fb + \" 2>/dev/null\").c_str());\n";
            out += "#endif\n";
            out += "}\n";
            out += "static int __nexa_os_get_brightness() {\n";
            out += "  char buf[256]; int v = -1; FILE* p = NULL;\n";
            out += "#ifdef _WIN32\n";
            out += "  p = popen(\"powershell -NoProfile -Command \\\"(Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightness).CurrentBrightness\\\" 2>nul\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) v = atoi(buf); pclose(p); }\n";
            out += "  return v;\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  p = popen(\"brightness -l 2>/dev/null | awk '/brightness/{print int($NF*100)}' | tail -1\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) v = atoi(buf); pclose(p); }\n";
            out += "  return v;\n";
            out += "#else\n";
            out += "  p = popen(\"brightnessctl -m 2>/dev/null | cut -d, -f4 | tr -d '%'\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) v = atoi(buf); pclose(p); }\n";
            out += "  if (v > 0) return v;\n";
            out += "  p = popen(\"test -n \\\"$(ls /sys/class/backlight 2>/dev/null)\\\" && for d in /sys/class/backlight/*; do c=$(cat \\\"$d/brightness\\\"); m=$(cat \\\"$d/max_brightness\\\"); echo $((c * 100 / m)); break; done 2>/dev/null\", \"r\");\n";
            out += "  if (p) { buf[0] = 0; if (fgets(buf, sizeof(buf), p) && buf[0] >= '0' && buf[0] <= '9') v = atoi(buf); pclose(p); }\n";
            out += "  if (v >= 0) return v;\n";
            out += "  p = popen(\"xrandr --verbose 2>/dev/null | awk '/Brightness/{print int($2*100); exit}'\", \"r\");\n";
            out += "  if (p) { if (fgets(buf, sizeof(buf), p)) v = atoi(buf); pclose(p); }\n";
            out += "  return v;\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osClipboard) {
            out += "#include <string>\n";
            out += "#include <cstdio>\n";
            out += "static void __nexa_os_clip_set(const std::string& text) {\n";
            out += "  const char* cmd = NULL;\n";
            out += "#ifdef _WIN32\n";
            out += "  cmd = \"clip\";\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  cmd = \"pbcopy\";\n";
            out += "#else\n";
            out += "  if (getenv(\"WAYLAND_DISPLAY\") && system(\"command -v wl-copy >/dev/null 2>&1\") == 0) cmd = \"wl-copy\";\n";
            out += "  else if (system(\"command -v xclip >/dev/null 2>&1\") == 0) cmd = \"xclip -selection clipboard\";\n";
            out += "  else if (system(\"command -v xsel >/dev/null 2>&1\") == 0) cmd = \"xsel --clipboard --input\";\n";
            out += "#endif\n";
            out += "  if (!cmd) return;\n";
            out += "#ifdef _WIN32\n";
            out += "  FILE* p = _popen(cmd, \"w\");\n";
            out += "#else\n";
            out += "  FILE* p = popen(cmd, \"w\");\n";
            out += "#endif\n";
            out += "  if (!p) return;\n";
            out += "  fwrite(text.data(), 1, text.size(), p);\n";
            out += "#ifdef _WIN32\n";
            out += "  _pclose(p);\n";
            out += "#else\n";
            out += "  pclose(p);\n";
            out += "#endif\n";
            out += "}\n";
            out += "static std::string __nexa_os_clip_get() {\n";
            out += "  const char* cmd = NULL;\n";
            out += "#ifdef _WIN32\n";
            out += "  cmd = \"powershell -NoProfile -Command Get-Clipboard\";\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  cmd = \"pbpaste\";\n";
            out += "#else\n";
            out += "  if (getenv(\"WAYLAND_DISPLAY\") && system(\"command -v wl-paste >/dev/null 2>&1\") == 0) cmd = \"wl-paste -n\";\n";
            out += "  else if (system(\"command -v xclip >/dev/null 2>&1\") == 0) cmd = \"xclip -selection clipboard -o\";\n";
            out += "  else if (system(\"command -v xsel >/dev/null 2>&1\") == 0) cmd = \"xsel --clipboard --output\";\n";
            out += "#endif\n";
            out += "  if (!cmd) return std::string();\n";
            out += "  std::string out; char buf[4096]; size_t n;\n";
            out += "#ifdef _WIN32\n";
            out += "  FILE* p = _popen(cmd, \"r\");\n";
            out += "#else\n";
            out += "  FILE* p = popen(cmd, \"r\");\n";
            out += "#endif\n";
            out += "  if (!p) return std::string();\n";
            out += "  while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);\n";
            out += "#ifdef _WIN32\n";
            out += "  _pclose(p);\n";
            out += "  while (!out.empty() && (out.back() == '\\n' || out.back() == '\\r')) out.pop_back();\n";
            out += "#else\n";
            out += "  pclose(p);\n";
            out += "#endif\n";
            out += "  return out;\n";
            out += "}\n";
        }
        if (hasOs() && (usage.osDesktop || usage.osType)) {
            out += "#include <string>\n";
            out += "#ifdef _WIN32\n";
            out += "#include <windows.h>\n";
            out += "#include <shellapi.h>\n";
            out += "static std::string __nexa_ps_quote(const std::string& s) {\n";
            out += "  std::string r; for (char c : s) { if (c == '\\'') r += \"''\"; else r += c; } return r;\n";
            out += "}\n";
            out += "#else\n";
            out += "#include <unistd.h>\n";
            out += "#include <sys/wait.h>\n";
            out += "static int __nexa_spawn(const char* const argv[]) {\n";
            out += "  fflush(NULL);\n";
            out += "  pid_t pid = fork();\n";
            out += "  if (pid < 0) return -1;\n";
            out += "  if (pid == 0) {\n";
            out += "    if (freopen(\"/dev/null\", \"w\", stdout)) {}\n";
            out += "    if (freopen(\"/dev/null\", \"w\", stderr)) {}\n";
            out += "    execvp(argv[0], (char* const*)argv);\n";
            out += "    _exit(127);\n";
            out += "  }\n";
            out += "  int st = 0; waitpid(pid, &st, 0);\n";
            out += "  return WIFEXITED(st) ? WEXITSTATUS(st) : -1;\n";
            out += "}\n";
            out += "#endif\n";
        }
        if (hasOs() && usage.osType) {
            out += "static void __nexa_os_type(const std::string& text) {\n";
            out += "#ifdef _WIN32\n";
            out += "  auto send_vk = [](WORD vk) {\n";
            out += "    INPUT in[2] = {};\n";
            out += "    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;\n";
            out += "    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk; in[1].ki.dwFlags = KEYEVENTF_KEYUP;\n";
            out += "    SendInput(2, in, sizeof(INPUT));\n";
            out += "  };\n";
            out += "  auto send_uni = [](wchar_t w) {\n";
            out += "    INPUT in[2] = {};\n";
            out += "    in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = w; in[0].ki.dwFlags = KEYEVENTF_UNICODE;\n";
            out += "    in[1].type = INPUT_KEYBOARD; in[1].ki.wScan = w; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;\n";
            out += "    SendInput(2, in, sizeof(INPUT));\n";
            out += "  };\n";
            out += "  int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0);\n";
            out += "  if (wlen <= 0) return;\n";
            out += "  std::wstring ws((size_t)wlen, L'\\0');\n";
            out += "  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), ws.data(), wlen);\n";
            out += "  for (wchar_t w : ws) {\n";
            out += "    if (w == L'\\n' || w == L'\\r') { if (w == L'\\n') send_vk(VK_RETURN); continue; }\n";
            out += "    if (w == L'\\t') { send_vk(VK_TAB); continue; }\n";
            out += "    send_uni(w);\n";
            out += "  }\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  std::string esc; esc.reserve(text.size() * 2);\n";
            out += "  for (char c : text) {\n";
            out += "    if (c == '\\\\' || c == '\"') esc += '\\\\';\n";
            out += "    if (c == '\\n') { esc += \"\\\" & return & \\\"\"; continue; }\n";
            out += "    if (c == '\\t') { esc += \"\\\" & tab & \\\"\"; continue; }\n";
            out += "    esc += c;\n";
            out += "  }\n";
            out += "  std::string script = \"tell application \\\"System Events\\\" to keystroke \\\"\" + esc + \"\\\"\";\n";
            out += "  const char* argv[] = {\"osascript\", \"-e\", script.c_str(), NULL};\n";
            out += "  (void)__nexa_spawn(argv);\n";
            out += "#else\n";
            out += "  const char* a1[] = {\"xdotool\", \"type\", \"--clearmodifiers\", \"--\", text.c_str(), NULL};\n";
            out += "  if (__nexa_spawn(a1) == 0) return;\n";
            out += "  const char* a2[] = {\"wtype\", text.c_str(), NULL};\n";
            out += "  (void)__nexa_spawn(a2);\n";
            out += "#endif\n";
            out += "}\n";
        }
        if (hasOs() && usage.osDesktop) {
            out += "static void __nexa_os_notify(const std::string& title, const std::string& msg) {\n";
            out += "#ifdef _WIN32\n";
            out += "  std::string cmd = \"powershell -NoProfile -Command \\\"Add-Type -AssemblyName System.Windows.Forms; $n=New-Object System.Windows.Forms.NotifyIcon; $n.Icon=[System.Drawing.SystemIcons]::Information; $n.Visible=$true; $n.ShowBalloonTip(5000,'\" + __nexa_ps_quote(title) + \"','\" + __nexa_ps_quote(msg) + \"',[System.Windows.Forms.ToolTipIcon]::Info); Start-Sleep -Milliseconds 6000; $n.Dispose()\\\" >nul 2>&1\";\n";
            out += "  (void)system(cmd.c_str());\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  std::string et, em; for (char c : title) { if (c=='\\\\'||c=='\"') et += '\\\\'; et += c; } for (char c : msg) { if (c=='\\\\'||c=='\"') em += '\\\\'; em += c; }\n";
            out += "  std::string script = \"display notification \\\"\" + em + \"\\\" with title \\\"\" + et + \"\\\"\";\n";
            out += "  const char* argv[] = {\"osascript\", \"-e\", script.c_str(), NULL};\n";
            out += "  (void)__nexa_spawn(argv);\n";
            out += "#else\n";
            out += "  const char* a1[] = {\"notify-send\", title.c_str(), msg.c_str(), NULL};\n";
            out += "  if (__nexa_spawn(a1) == 0) return;\n";
            out += "  std::string combined = title + \": \" + msg;\n";
            out += "  const char* a2[] = {\"zenity\", \"--notification\", \"--text\", combined.c_str(), NULL};\n";
            out += "  if (__nexa_spawn(a2) == 0) return;\n";
            out += "  const char* a3[] = {\"kdialog\", \"--title\", title.c_str(), \"--passivepopup\", msg.c_str(), \"5\", NULL};\n";
            out += "  (void)__nexa_spawn(a3);\n";
            out += "#endif\n";
            out += "}\n";
            out += "static void __nexa_os_open(const std::string& target) {\n";
            out += "#ifdef _WIN32\n";
            out += "  ShellExecuteA(NULL, \"open\", target.c_str(), NULL, NULL, SW_SHOWNORMAL);\n";
            out += "#elif defined(__APPLE__)\n";
            out += "  const char* argv[] = {\"open\", target.c_str(), NULL}; (void)__nexa_spawn(argv);\n";
            out += "#else\n";
            out += "  const char* a1[] = {\"xdg-open\", target.c_str(), NULL};\n";
            out += "  if (__nexa_spawn(a1) == 0) return;\n";
            out += "  const char* a2[] = {\"gio\", \"open\", target.c_str(), NULL};\n";
            out += "  if (__nexa_spawn(a2) == 0) return;\n";
            out += "  const char* a3[] = {\"gvfs-open\", target.c_str(), NULL};\n";
            out += "  (void)__nexa_spawn(a3);\n";
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
            out += "#include <system_error>\n";
            out += fileRuntimeCpp();
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
        if (hasCrypto() && usage.crypto) {
            out += cryptoRuntimeCpp();
        }
        if (hasHttp() && usage.http) {
            out += httpRuntimeCpp();
        }
        // Core string methods (value.upper(), value.split(...), ...) need no #include from the user.
        if (usage.str) {
            out += "#include <string>\n";
            out += "#include <cctype>\n";
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
            out += "#include <functional>\n";
            out += "static std::vector<std::thread> __nexa_threads;\n";
            out += "static int __nexa_thread_spawn(void (*fn)()) {\n";
            out += "  __nexa_threads.emplace_back(fn);\n";
            out += "  return static_cast<int>(__nexa_threads.size()) - 1;\n";
            out += "}\n";
            if (usage.threadLambda) {
                out += "static int __nexa_thread_spawn_fn(std::function<void()> fn) {\n";
                out += "  __nexa_threads.emplace_back(std::move(fn));\n";
                out += "  return static_cast<int>(__nexa_threads.size()) - 1;\n";
                out += "}\n";
            }
            out += "static void __nexa_thread_join(int idx) {\n";
            out += "  if (idx >= 0 && static_cast<size_t>(idx) < __nexa_threads.size() && __nexa_threads[idx].joinable()) {\n";
            out += "    __nexa_threads[idx].join();\n";
            out += "  }\n";
            out += "}\n";
            if (usage.threadWorker) {
                out += "#include <deque>\n";
                out += "#include <mutex>\n";
                out += "#include <condition_variable>\n";
                out += "#include <memory>\n";
                out += "struct __nexa_worker {\n";
                out += "  std::thread t;\n";
                out += "  std::mutex mu;\n";
                out += "  std::condition_variable cv;\n";
                out += "  std::deque<std::function<void()>> jobs;\n";
                out += "  bool stop = false;\n";
                out += "};\n";
                out += "static std::vector<std::unique_ptr<__nexa_worker>> __nexa_workers;\n";
                out += "static int __nexa_thread_worker_create() {\n";
                out += "  auto w = std::make_unique<__nexa_worker>();\n";
                out += "  __nexa_worker* wp = w.get();\n";
                out += "  wp->t = std::thread([wp]() {\n";
                out += "    while (true) {\n";
                out += "      std::function<void()> job;\n";
                out += "      {\n";
                out += "        std::unique_lock<std::mutex> lock(wp->mu);\n";
                out += "        wp->cv.wait(lock, [&]{ return wp->stop || !wp->jobs.empty(); });\n";
                out += "        if (wp->stop && wp->jobs.empty()) return;\n";
                out += "        job = std::move(wp->jobs.front());\n";
                out += "        wp->jobs.pop_front();\n";
                out += "      }\n";
                out += "      if (job) job();\n";
                out += "    }\n";
                out += "  });\n";
                out += "  __nexa_workers.push_back(std::move(w));\n";
                out += "  return static_cast<int>(__nexa_workers.size()) - 1;\n";
                out += "}\n";
                out += "static void __nexa_thread_worker_run(int idx, std::function<void()> fn) {\n";
                out += "  if (idx < 0 || static_cast<size_t>(idx) >= __nexa_workers.size()) return;\n";
                out += "  auto& wp = *__nexa_workers[idx];\n";
                out += "  {\n";
                out += "    std::lock_guard<std::mutex> lock(wp.mu);\n";
                out += "    wp.jobs.push_back(std::move(fn));\n";
                out += "  }\n";
                out += "  wp.cv.notify_one();\n";
                out += "}\n";
                out += "static void __nexa_thread_worker_join(int idx) {\n";
                out += "  if (idx < 0 || static_cast<size_t>(idx) >= __nexa_workers.size()) return;\n";
                out += "  auto& wp = *__nexa_workers[idx];\n";
                out += "  {\n";
                out += "    std::lock_guard<std::mutex> lock(wp.mu);\n";
                out += "    wp.stop = true;\n";
                out += "  }\n";
                out += "  wp.cv.notify_one();\n";
                out += "  if (wp.t.joinable()) wp.t.join();\n";
                out += "}\n";
            }
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
        all.osSystem = all.osExec = all.osGetenv = all.osPlatform = all.osExeDir = true;
        all.osGetProcessId = true;
        all.osWindowControl = all.osMessageBox = all.osGrepKeys = all.osKeyPressed = true;
        all.osType = true;
        all.osLock = all.osShutdown = all.osReboot = all.osSuspend = all.osLogout = true;
        all.osAudio = true;
        all.osBrightness = true;
        all.osClipboard = true;
        all.osDesktop = true;
        all.osExit = all.osHostname = all.osUsername = all.osHome = all.osSetenv = true;
        all.file = all.random = all.math = all.time = all.thread = all.dll = true;
        all.str = true;
        return getCppIncludes(all);
    }

private:
    std::set<std::string> enabled_;
};

}  // namespace nexa
