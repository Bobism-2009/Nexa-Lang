#pragma once

#include <string>

namespace nexa {

// std/term: colored output, cursor control, and a key at a time.
//
// All of it is plain terminal control -- ANSI escape codes for colour and the
// cursor, the OS console API for reading keys and the window size. Nothing is
// linked. Each group is emitted only when the program uses it, so a program
// that only prints in colour carries none of the raw-input code.
//
// `style`  : term.color / bg / bold / dim / italic / underline / reverse  (return a string)
// `screen` : term.clear / clear_line / move / home / hide_cursor / show_cursor / alt_screen
// `raw`    : term.raw / getkey / key_available
// `size`   : term.width / term.height
inline std::string termRuntimeCpp(bool style, bool screen, bool raw, bool size, bool progress) {
    std::string out;
    out += "#include <string>\n";
    out += "#include <cstdio>\n";
    out += "#ifdef _WIN32\n#include <windows.h>\n";
    if (raw) out += "#include <conio.h>\n";
    out += "#else\n";
    out += "#include <unistd.h>\n";
    if (raw || size) out += "#include <sys/ioctl.h>\n#include <termios.h>\n";
    if (raw) out += "#include <fcntl.h>\n#include <sys/select.h>\n";
    out += "#endif\n";

    // On Windows the console honours ANSI escapes only once virtual-terminal
    // processing is turned on; everywhere else it always does. Done once, the
    // first time any colour or cursor call runs.
    if (style || screen || progress) {
        out += R"NEXA_TERM(
static void __nexa_term_vt() {
#ifdef _WIN32
    static int __done = 0;
    if (__done) return;
    __done = 1;
    HANDLE __h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD __m = 0;
    if (__h != INVALID_HANDLE_VALUE && GetConsoleMode(__h, &__m))
        SetConsoleMode(__h, __m | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
#endif
}
static void __nexa_term_emit(const std::string& __s) { std::fwrite(__s.data(), 1, __s.size(), stdout); std::fflush(stdout); }
)NEXA_TERM";
    }

    if (style) {
        // Each wrapper turns its one attribute on before the text and off after
        // -- not a full reset -- so they nest: bold(color(x,"red")) keeps both.
        out += R"NEXA_TERM(
static int __nexa_term_sgr(const std::string& __name, int __bg) {
    std::string __n = __name;
    for (char& __c : __n) if (__c >= 'A' && __c <= 'Z') __c = (char)(__c - 'A' + 'a');
    int __base = __bg ? 40 : 30;
    if (__n == "black")   return __base + 0;
    if (__n == "red")     return __base + 1;
    if (__n == "green")   return __base + 2;
    if (__n == "yellow")  return __base + 3;
    if (__n == "blue")    return __base + 4;
    if (__n == "magenta") return __base + 5;
    if (__n == "cyan")    return __base + 6;
    if (__n == "white")   return __base + 7;
    if (__n == "gray" || __n == "grey") return __base + 60;  /* bright black */
    if (__n.size() > 7 && __n.compare(0, 7, "bright_") == 0) {
        const std::string __r = __n.substr(7);
        int __c = __r == "black" ? 0 : __r == "red" ? 1 : __r == "green" ? 2 : __r == "yellow" ? 3 :
                  __r == "blue" ? 4 : __r == "magenta" ? 5 : __r == "cyan" ? 6 : __r == "white" ? 7 : -1;
        if (__c >= 0) return __base + 60 + __c;
    }
    return -1;  // an unknown name leaves the text uncoloured
}
static std::string __nexa_term_color(const std::string& __text, const std::string& __name, int __bg) {
    __nexa_term_vt();
    const int __c = __nexa_term_sgr(__name, __bg);
    if (__c < 0) return __text;
    return std::string("\x1b[") + std::to_string(__c) + "m" + __text + "\x1b[" + (__bg ? "49" : "39") + "m";
}
static std::string __nexa_term_attr(const std::string& __text, const char* __on, const char* __off) {
    __nexa_term_vt();
    return std::string("\x1b[") + __on + "m" + __text + "\x1b[" + __off + "m";
}
)NEXA_TERM";
    }

    if (screen) {
        out += R"NEXA_TERM(
static void __nexa_term_clear() { __nexa_term_vt(); __nexa_term_emit("\x1b[2J\x1b[H"); }
static void __nexa_term_clear_line() { __nexa_term_vt(); __nexa_term_emit("\x1b[2K\r"); }
static void __nexa_term_eol() { __nexa_term_vt(); __nexa_term_emit("\x1b[K"); }
static void __nexa_term_home() { __nexa_term_vt(); __nexa_term_emit("\x1b[H"); }
static void __nexa_term_move(int __col, int __row) {
    __nexa_term_vt();
    if (__col < 0) __col = 0;
    if (__row < 0) __row = 0;
    __nexa_term_emit(std::string("\x1b[") + std::to_string(__row + 1) + ";" + std::to_string(__col + 1) + "H");
}
static void __nexa_term_cursor(int __show) { __nexa_term_vt(); __nexa_term_emit(__show ? "\x1b[?25h" : "\x1b[?25l"); }
static void __nexa_term_alt(int __on) {
    __nexa_term_vt();
    // The alternate screen buffer: a program draws on a clean page and the
    // user's scrollback comes back untouched when it switches off.
    __nexa_term_emit(__on ? "\x1b[?1049h" : "\x1b[?1049l");
}
)NEXA_TERM";
    }

    if (size) {
        out += R"NEXA_TERM(
static int __nexa_term_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO __i;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &__i)) return __i.srWindow.Right - __i.srWindow.Left + 1;
    return 80;
#else
    struct winsize __w;
    if (ioctl(1, TIOCGWINSZ, &__w) == 0 && __w.ws_col) return __w.ws_col;
    return 80;
#endif
}
static int __nexa_term_height() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO __i;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &__i)) return __i.srWindow.Bottom - __i.srWindow.Top + 1;
    return 24;
#else
    struct winsize __w;
    if (ioctl(1, TIOCGWINSZ, &__w) == 0 && __w.ws_row) return __w.ws_row;
    return 24;
#endif
}
)NEXA_TERM";
    }

    if (progress) {
        // A loading bar on the current line, redrawn in place: "[####------]  40%".
        // The caller loops and calls it, then prints a newline when done. width is
        // the bar between the brackets; -1 means a sensible default.
        out += R"NEXA_TERM(
static void __nexa_term_progress(int __done, int __total, int __width) {
    __nexa_term_vt();
    if (__width < 1) __width = 30;
    if (__total < 1) __total = 1;
    if (__done < 0) __done = 0;
    if (__done > __total) __done = __total;
    const int __filled = (int)((long long)__done * __width / __total);
    const int __pct = (int)((long long)__done * 100 / __total);
    std::string __b = "\r[";
    for (int __i = 0; __i < __width; __i++) __b += (__i < __filled ? '#' : '-');
    __b += "] ";
    if (__pct < 100) __b += " ";
    if (__pct < 10) __b += " ";
    __b += std::to_string(__pct);
    __b += "%\x1b[K";
    __nexa_term_emit(__b);
}
)NEXA_TERM";
    }

    if (raw) {
        out += R"NEXA_TERM(
#ifndef _WIN32
static struct termios __nexa_term_saved;
static int __nexa_term_raw_on = 0;
#endif
// Raw mode: keys arrive one at a time, not echoed, and Ctrl-C/Ctrl-Z are read
// as keys rather than signals. Leave it on only as long as the program is
// reading keys -- pair it with a defer term.raw(false) so it is always undone.
static void __nexa_term_raw(int __on) {
#ifdef _WIN32
    HANDLE __h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD __m = 0;
    if (!GetConsoleMode(__h, &__m)) return;
    if (__on) __m &= ~(DWORD)(0x0002 /* ECHO */ | 0x0004 /* LINE */ | 0x0001 /* PROCESSED */);
    else __m |= (0x0002 | 0x0004 | 0x0001);
    SetConsoleMode(__h, __m);
#else
    if (__on) {
        if (__nexa_term_raw_on) return;
        if (tcgetattr(0, &__nexa_term_saved) != 0) return;
        struct termios __r = __nexa_term_saved;
        __r.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
        __r.c_iflag &= ~(tcflag_t)(IXON | ICRNL);
        __r.c_cc[VMIN] = 1;
        __r.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &__r);
        __nexa_term_raw_on = 1;
    } else if (__nexa_term_raw_on) {
        tcsetattr(0, TCSANOW, &__nexa_term_saved);
        __nexa_term_raw_on = 0;
    }
#endif
}

// One keypress, by a friendly name. Printable keys are themselves ("a", "7",
// "?"); the rest are named: "enter" "esc" "tab" "space" "backspace" "up" "down"
// "left" "right" "home" "end" "pageup" "pagedown" "delete" "insert", and a
// control combination is "ctrl-a".."ctrl-z". "" at end of input. Best in raw
// mode; without it the line is read first and keys come back one per call.
static std::string __nexa_term_named(int __c) {
    if (__c == '\r' || __c == '\n') return "enter";
    if (__c == '\t') return "tab";
    if (__c == ' ') return "space";
    if (__c == 127 || __c == 8) return "backspace";
    if (__c == 27) return "esc";
    if (__c >= 1 && __c <= 26) return std::string("ctrl-") + (char)('a' + __c - 1);
    if (__c >= 32 && __c < 127) return std::string(1, (char)__c);
    return "";
}
#ifdef _WIN32
static std::string __nexa_term_getkey() {
    int __c = _getch();
    if (__c == 0 || __c == 0xE0) {
        int __d = _getch();
        switch (__d) {
            case 72: return "up";
            case 80: return "down";
            case 75: return "left";
            case 77: return "right";
            case 71: return "home";
            case 79: return "end";
            case 73: return "pageup";
            case 81: return "pagedown";
            case 83: return "delete";
            case 82: return "insert";
            default: return "";
        }
    }
    return __nexa_term_named(__c);
}
static int __nexa_term_key_available() { return _kbhit() ? 1 : 0; }
#else
// A CSI sequence after ESC: ESC [ A is up, ESC [ 3 ~ is delete, and so on. A
// lone ESC (nothing waiting behind it) stays "esc".
static int __nexa_term_read_byte(int __block) {
    unsigned char __b;
    if (!__block) {
        int __fl = fcntl(0, F_GETFL);
        fcntl(0, F_SETFL, __fl | O_NONBLOCK);
        ssize_t __n = read(0, &__b, 1);
        fcntl(0, F_SETFL, __fl);
        return __n == 1 ? __b : -1;
    }
    return read(0, &__b, 1) == 1 ? __b : -1;
}
static std::string __nexa_term_getkey() {
    int __c = __nexa_term_read_byte(1);
    if (__c < 0) return "";
    if (__c != 27) return __nexa_term_named(__c);
    int __a = __nexa_term_read_byte(0);
    if (__a < 0) return "esc";
    if (__a != '[' && __a != 'O') return __nexa_term_named(__a);  // Alt+key, roughly
    int __b = __nexa_term_read_byte(0);
    switch (__b) {
        case 'A': return "up";
        case 'B': return "down";
        case 'C': return "right";
        case 'D': return "left";
        case 'H': return "home";
        case 'F': return "end";
    }
    if (__b >= '1' && __b <= '8') {
        int __t = __nexa_term_read_byte(0);  // the trailing '~'
        (void)__t;
        switch (__b) {
            case '1': case '7': return "home";
            case '4': case '8': return "end";
            case '2': return "insert";
            case '3': return "delete";
            case '5': return "pageup";
            case '6': return "pagedown";
        }
    }
    return "";
}
static int __nexa_term_key_available() {
    struct timeval __tv = {0, 0};
    fd_set __fs;
    FD_ZERO(&__fs);
    FD_SET(0, &__fs);
    return select(1, &__fs, nullptr, nullptr, &__tv) > 0 ? 1 : 0;
}
#endif
)NEXA_TERM";
    }
    return out;
}

}  // namespace nexa
