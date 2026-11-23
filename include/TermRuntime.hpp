#pragma once

#include <cstdio>
#include <set>
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
// `flags`  : term.c_iflag ... set_c_lflag / c_cc / set_c_cc / restore, and term.ICANON etc. (termFlagsRuntimeCpp)
inline std::string termRuntimeCpp(bool style, bool screen, bool raw, bool size, bool progress, bool spinner) {
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
    if (style || screen || progress || spinner) {
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

    if (spinner) {
        // A one-character spinner on the current line, for work with no known
        // length: call it with a tick that climbs each frame, optionally with a
        // label after it ("- working..."). Plain ASCII |/-\ so it shows anywhere.
        out += R"NEXA_TERM(
static void __nexa_term_spinner(int __tick, const std::string& __label) {
    __nexa_term_vt();
    // A spinning wheel in a casing: ( | ) ( / ) ( - ) ( \ ), a spoke turning a
    // step each frame inside parentheses so it reads as a wheel, not a bare
    // slash. Pure ASCII, so it looks the same in CMD, PowerShell, Windows
    // Terminal and every Linux/macOS terminal -- no font or codepage to depend on.
    static const char __frames[] = {'|', '/', '-', '\\'};
    std::string __b = "\r(";
    __b += __frames[((__tick % 4) + 4) % 4];
    __b += ")";
    if (!__label.empty()) { __b += " "; __b += __label; }
    __b += "\x1b[K";
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

// ---------------------------------------------------------------------------
// Direct terminal control: the termios flag words and control characters.
//
//   term.c_iflag() / c_oflag() / c_cflag() / c_lflag()   read a flag word
//   term.set_c_iflag(v) / ...                             write one back
//   term.c_cc(i) / term.set_c_cc(i, v)                    control characters
//   term.restore()                                        undo every set_*
//   term.IGNBRK, term.ICANON, term.VMIN, ...              the flag constants
//
// A constant is the platform's own macro wherever termios has it, so the bits
// are right on Linux, macOS and every CPU. Only the constants a program names
// are emitted. Windows has no termios: there the values are Linux's, and the
// flags the console can do (ICANON, ECHO, ISIG, OPOST, ONLCR) drive its mode;
// the rest are kept and read back, with no effect.

struct TermFlagConst { const char* name; unsigned long linuxValue; };

inline const TermFlagConst* termFlagConsts() {
    static const TermFlagConst table[] = {
        // c_iflag
        {"IGNBRK", 0000001}, {"BRKINT", 0000002}, {"IGNPAR", 0000004}, {"PARMRK", 0000010},
        {"INPCK", 0000020}, {"ISTRIP", 0000040}, {"INLCR", 0000100}, {"IGNCR", 0000200},
        {"ICRNL", 0000400}, {"IUCLC", 0001000}, {"IXON", 0002000}, {"IXANY", 0004000},
        {"IXOFF", 0010000}, {"IMAXBEL", 0020000}, {"IUTF8", 0040000},
        // c_oflag
        {"OPOST", 0000001}, {"OLCUC", 0000002}, {"ONLCR", 0000004}, {"OCRNL", 0000010},
        {"ONOCR", 0000020}, {"ONLRET", 0000040}, {"OFILL", 0000100}, {"OFDEL", 0000200},
        {"NLDLY", 0000400}, {"NL0", 0}, {"NL1", 0000400},
        {"CRDLY", 0003000}, {"CR0", 0}, {"CR1", 0001000}, {"CR2", 0002000}, {"CR3", 0003000},
        {"TABDLY", 0014000}, {"TAB0", 0}, {"TAB1", 0004000}, {"TAB2", 0010000}, {"TAB3", 0014000},
        {"BSDLY", 0020000}, {"BS0", 0}, {"BS1", 0020000},
        {"VTDLY", 0040000}, {"VT0", 0}, {"VT1", 0040000},
        {"FFDLY", 0100000}, {"FF0", 0}, {"FF1", 0100000},
        // c_cflag
        {"CSIZE", 0000060}, {"CS5", 0}, {"CS6", 0000020}, {"CS7", 0000040}, {"CS8", 0000060},
        {"CSTOPB", 0000100}, {"CREAD", 0000200}, {"PARENB", 0000400}, {"PARODD", 0001000},
        {"HUPCL", 0002000}, {"CLOCAL", 0004000}, {"CRTSCTS", 020000000000},
        // c_lflag
        {"ISIG", 0000001}, {"ICANON", 0000002}, {"XCASE", 0000004}, {"ECHO", 0000010},
        {"ECHOE", 0000020}, {"ECHOK", 0000040}, {"ECHONL", 0000100}, {"NOFLSH", 0000200},
        {"TOSTOP", 0000400}, {"ECHOCTL", 0001000}, {"ECHOPRT", 0002000}, {"ECHOKE", 0004000},
        {"FLUSHO", 0010000}, {"PENDIN", 0040000}, {"IEXTEN", 0100000},
        // c_cc indices
        {"VINTR", 0}, {"VQUIT", 1}, {"VERASE", 2}, {"VKILL", 3}, {"VEOF", 4}, {"VTIME", 5},
        {"VMIN", 6}, {"VSTART", 8}, {"VSTOP", 9}, {"VSUSP", 10}, {"VEOL", 11},
        {"VREPRINT", 12}, {"VDISCARD", 13}, {"VWERASE", 14}, {"VLNEXT", 15}, {"VEOL2", 16},
        {nullptr, 0},
    };
    return table;
}

inline const TermFlagConst* termFlagConst(const std::string& name) {
    for (const TermFlagConst* c = termFlagConsts(); c->name; ++c) {
        if (name == c->name) return c;
    }
    return nullptr;
}

// The methods that read or write the flags (each takes this runtime).
inline bool termIsFlagMethod(const std::string& m) {
    return m == "c_iflag" || m == "c_oflag" || m == "c_cflag" || m == "c_lflag" ||
           m == "set_c_iflag" || m == "set_c_oflag" || m == "set_c_cflag" || m == "set_c_lflag" ||
           m == "c_cc" || m == "set_c_cc" || m == "restore";
}

// `flags`: the read/write functions; `consts`: the constants the program names.
inline std::string termFlagsRuntimeCpp(bool flags, const std::set<std::string>& consts) {
    std::string out;
    out += "#ifdef _WIN32\n#include <windows.h>\n#else\n#include <termios.h>\n#include <unistd.h>\n#endif\n";
    for (const std::string& n : consts) {
        const TermFlagConst* c = termFlagConst(n);
        if (!c) continue;
        char lin[32];
        std::snprintf(lin, sizeof lin, "0x%lxu", c->linuxValue);
        out += "#if defined(" + n + ")\nstatic const int __nexa_tf_" + n + " = (int)(" + n + ");\n";
        out += "#elif defined(_WIN32)\nstatic const int __nexa_tf_" + n + " = (int)" + lin + ";\n";
        out += "#else\nstatic const int __nexa_tf_" + n + " = 0;  /* not on this platform */\n#endif\n";
    }
    if (!flags) return out;
    out += R"NEXA_TERM(
#ifdef _WIN32
// Windows: the console mode stands in for termios. Linux's bit values.
static int __nexa_tf_shadow[4] = {
    0000400 | 0002000,                               /* ICRNL IXON */
    0000001 | 0000004,                               /* OPOST ONLCR */
    0000060 | 0000200,                               /* CS8 CREAD */
    0000001 | 0000002 | 0000010 | 0000020 | 0000040 | 0100000 | 0001000 | 0004000,
};
static unsigned char __nexa_tf_cc[17] = {3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0};
static DWORD __nexa_tf_orig_in = 0, __nexa_tf_orig_out = 0;
static int __nexa_tf_saved = 0, __nexa_tf_have_in = 0, __nexa_tf_have_out = 0;
static void __nexa_tf_save() {
    if (__nexa_tf_saved) return;
    __nexa_tf_saved = 1;
    // Only a handle that is a console has a mode to give back.
    __nexa_tf_have_in = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &__nexa_tf_orig_in) ? 1 : 0;
    __nexa_tf_have_out = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &__nexa_tf_orig_out) ? 1 : 0;
}
static int __nexa_term_get_flag(int __w) {
    DWORD __m = 0;
    if (__w == 1) {
        if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &__m)) return -1;
        int __v = __nexa_tf_shadow[1] & ~(0000001 | 0000004);
        if (__m & 0x0001) __v |= 0000001;        /* ENABLE_PROCESSED_OUTPUT: OPOST */
        if (!(__m & 0x0008)) __v |= 0000004;     /* no DISABLE_NEWLINE_AUTO_RETURN: ONLCR */
        return __v;
    }
    if (!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &__m)) return -1;
    if (__w != 3) return __nexa_tf_shadow[__w];
    int __v = __nexa_tf_shadow[3] & ~(0000001 | 0000002 | 0000010);
    if (__m & 0x0001) __v |= 0000001;            /* ENABLE_PROCESSED_INPUT: ISIG */
    if (__m & 0x0002) __v |= 0000002;            /* ENABLE_LINE_INPUT: ICANON */
    if (__m & 0x0004) __v |= 0000010;            /* ENABLE_ECHO_INPUT: ECHO */
    return __v;
}
static int __nexa_term_set_flag(int __w, int __v) {
    __nexa_tf_save();
    DWORD __m = 0;
    if (__w == 1) {
        HANDLE __h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleMode(__h, &__m)) return 0;
        __m &= ~(DWORD)(0x0001 | 0x0008);
        if (__v & 0000001) __m |= 0x0001;
        if (!(__v & 0000004)) __m |= 0x0008;
        if (!SetConsoleMode(__h, __m)) return 0;
    } else {
        HANDLE __h = GetStdHandle(STD_INPUT_HANDLE);
        if (!GetConsoleMode(__h, &__m)) return 0;
        if (__w == 3) {
            __m &= ~(DWORD)(0x0001 | 0x0002 | 0x0004);
            if (__v & 0000001) __m |= 0x0001;
            if (__v & 0000002) __m |= 0x0002;
            if (__v & 0000010) __m |= 0x0004;    /* the console echoes only with ICANON */
            if (!SetConsoleMode(__h, __m)) return 0;
        }
    }
    __nexa_tf_shadow[__w] = __v;
    return 1;
}
static int __nexa_term_get_cc(int __i) {
    DWORD __m = 0;
    if (__i < 0 || __i >= 17 || !GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &__m)) return -1;
    return __nexa_tf_cc[__i];
}
static int __nexa_term_set_cc(int __i, int __v) {
    DWORD __m = 0;
    if (__i < 0 || __i >= 17 || !GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &__m)) return 0;
    __nexa_tf_cc[__i] = (unsigned char)__v;
    return 1;
}
static int __nexa_term_restore() {
    if (!__nexa_tf_saved) return 1;
    __nexa_tf_saved = 0;
    int __a = !__nexa_tf_have_in || SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), __nexa_tf_orig_in);
    int __b = !__nexa_tf_have_out || SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), __nexa_tf_orig_out);
    return __a && __b;
}
#else
// The terminal as the first set_* found it, for term.restore().
static struct termios __nexa_tf_orig;
static int __nexa_tf_saved = 0;
static int __nexa_term_get_flag(int __w) {
    struct termios __t;
    if (tcgetattr(0, &__t) != 0) return -1;
    switch (__w) {
        case 0: return (int)__t.c_iflag;
        case 1: return (int)__t.c_oflag;
        case 2: return (int)__t.c_cflag;
        default: return (int)__t.c_lflag;
    }
}
static int __nexa_term_set_flag(int __w, int __v) {
    struct termios __t;
    if (tcgetattr(0, &__t) != 0) return 0;
    if (!__nexa_tf_saved) { __nexa_tf_orig = __t; __nexa_tf_saved = 1; }
    const tcflag_t __f = (tcflag_t)(unsigned int)__v;
    switch (__w) {
        case 0: __t.c_iflag = __f; break;
        case 1: __t.c_oflag = __f; break;
        case 2: __t.c_cflag = __f; break;
        default: __t.c_lflag = __f; break;
    }
    return tcsetattr(0, TCSANOW, &__t) == 0 ? 1 : 0;
}
static int __nexa_term_get_cc(int __i) {
    struct termios __t;
    if (__i < 0 || __i >= NCCS || tcgetattr(0, &__t) != 0) return -1;
    return (int)(unsigned char)__t.c_cc[__i];
}
static int __nexa_term_set_cc(int __i, int __v) {
    struct termios __t;
    if (__i < 0 || __i >= NCCS || tcgetattr(0, &__t) != 0) return 0;
    if (!__nexa_tf_saved) { __nexa_tf_orig = __t; __nexa_tf_saved = 1; }
    __t.c_cc[__i] = (cc_t)__v;
    return tcsetattr(0, TCSANOW, &__t) == 0 ? 1 : 0;
}
static int __nexa_term_restore() {
    if (!__nexa_tf_saved) return 1;
    __nexa_tf_saved = 0;
    return tcsetattr(0, TCSANOW, &__nexa_tf_orig) == 0 ? 1 : 0;
}
#endif
)NEXA_TERM";
    return out;
}

}  // namespace nexa
