#pragma once

#include <string>

namespace nexa {

// Which gfx helpers to emit. The runtime is ~2,800 lines and a program that
// only opens a window and draws reaches a small corner of it, so the
// transpiler records which gfx builtins were called (Modules::CppUsage) and
// every group below is emitted only when something can reach it. This is dead
// code removal: a program that uses a feature gets byte-identical code.
//
// The groups are deliberately coarse. `shapesFill` is one flag for
// fill/rect/fill_circle/fill_ellipse/fill_tri/fill_poly rather than six,
// because a six-way dependency table is a maintenance cost that buys back
// eighty lines. The internal helpers -- spans, line clipping, the ellipse
// maths, the u8 clamp -- are derived from these flags in gfxRuntimeCpp below
// rather than tracked separately.
//
// Window open/close/poll/present/closed/clear, the framebuffer plumbing every
// draw goes through, and gfx.fullscreen are always emitted. fullscreen is not
// optional because the Win32 window procedure and the Emscripten key handler
// call it themselves: maximising a window is a way into fullscreen that does
// not go through gfx.fullscreen, so slicing it would change behaviour rather
// than remove dead code.
struct GfxNeed {
    bool alpha = false;          // gfx.alpha() -- the reader; the setter is core
    bool plot = false;           // gfx.plot
    bool get = false;            // gfx.get
    bool shapesFill = false;     // fill, rect, fill_circle/ellipse/tri/poly
    bool shapesOutline = false;  // circle, ellipse, tri, poly
    bool arc = false;            // gfx.arc
    bool pie = false;            // gfx.pie
    bool roundRect = false;      // gfx.round_rect
    bool fillRoundRect = false;  // gfx.fill_round_rect
    bool line = false;           // gfx.line, 7 arguments
    bool lineThick = false;      // gfx.line, 8 arguments
    bool text = false;           // text, text_size, text_width, text_height
    bool mouse = false;          // mouse, mouse_x, mouse_y
    bool keys = false;           // key, pressed, released
    bool typed = false;          // typed
    bool wheel = false;          // wheel, wheel_x
    bool imageStore = false;     // the loaded-image table: image_w, image_h, blit
    bool imageLoad = false;      // image, decode -- the decoder and the file read
    bool blit = false;           // blit
    bool blitRot = false;        // blit_rot
    bool icon = false;           // gfx.icon
    bool save = false;           // gfx.save
    bool dialogs = false;        // opendialog, drop
    bool audio = false;          // audio, sample, audio_queued, audio_flush
    bool sound = false;          // sound, play, loop, stop, volume -- the mixer
    bool cursor = false;         // gfx.cursor
    bool window = false;         // resize, width, height, scale, title
    bool maxfps = false;         // gfx.maxfps -- the clock and the frame wait
};

// gfx.sound / play / loop / stop / volume: a WAV loader and a polyphonic
// mixer, sitting entirely on top of the platform audio block above
// (__nexa_gfx_audio, __nexa_gfx_sample, __nexa_gfx_audio_queued). Nothing in
// here is per-platform, which is why the whole thing is one string rather than
// a third copy inside the #ifdef ladder: wherever the stream opens, sound
// plays, and where it does not the mixer finds a rate of 0 and does nothing.
inline std::string soundRuntimeCpp() {
    return R"NEXA_GFX(
#include <utility>

#define NEXA_VOICES 16

// A loaded sound is mono signed-16 at whatever rate the file was, kept as it
// came off disk rather than resampled on load: the stream rate is not
// necessarily known yet when gfx.sound runs, and a voice resamples as it mixes
// anyway.
struct __nexa_Snd {
    std::vector<short> pcm;
    int rate;
};

// Slot 0 is the reserved "no sound" entry, so handles start at 1 and 0 is
// never valid -- the same shape as the image table.
static std::vector<__nexa_Snd> __nexa_snds;
static std::vector<std::string> __nexa_snd_paths;

// pos and step are 16.16 fixed point in source samples: step is how far a
// voice walks through its sound per output sample, which is what turns any
// file rate into the stream rate. id is 0 when the slot is free, and otherwise
// the voice id handed back by gfx.play -- monotonic, so the smallest live id
// is also the oldest voice, which is the one gfx.play steals when all sixteen
// are busy.
struct __nexa_Voice {
    int id;
    int snd;
    int vol;
    int loop;
    long long pos;
    long long step;
};

static __nexa_Voice __nexa_voices[NEXA_VOICES];
static int __nexa_voice_next = 0;
static int __nexa_master_vol = 255;

static unsigned int __nexa_wav_u16(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int __nexa_wav_u32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int __nexa_wav_s16(const unsigned char* p) {
    int v = (int)__nexa_wav_u16(p);
    return v >= 32768 ? v - 65536 : v;
}

// RIFF: a twelve byte header and then a flat list of chunks, each an id, a
// little-endian length and a body padded to an even length. Only fmt and data
// mean anything here; LIST, fact, cue and whatever else a tool wrote are
// walked over. Anything that is not 8- or 16-bit mono/stereo PCM is refused
// rather than guessed at.
static int __nexa_gfx_wav_decode(const std::string& bytes, __nexa_Snd* out) {
    const unsigned char* p = (const unsigned char*)bytes.data();
    size_t n = bytes.size();
    if (n < 12) return 0;
    if (std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) return 0;
    int chans = 0, rate = 0, bits = 0, pcm = 0;
    const unsigned char* data = NULL;
    size_t dlen = 0;
    size_t off = 12;
    while (off + 8 <= n) {
        const unsigned char* id = p + off;
        size_t len = (size_t)__nexa_wav_u32(p + off + 4);
        off += 8;
        if (len > n - off) len = n - off;   // truncated file: take what is there
        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16) {
            unsigned int tag = __nexa_wav_u16(p + off);
            chans = (int)__nexa_wav_u16(p + off + 2);
            rate = (int)__nexa_wav_u32(p + off + 4);
            bits = (int)__nexa_wav_u16(p + off + 14);
            // WAVE_FORMAT_EXTENSIBLE keeps the real tag in the first two bytes
            // of its subformat GUID; plenty of tools write plain PCM that way.
            if (tag == 0xFFFE && len >= 40) tag = __nexa_wav_u16(p + off + 24);
            pcm = (tag == 1) ? 1 : 0;
        } else if (std::memcmp(id, "data", 4) == 0) {
            data = p + off;
            dlen = len;
        }
        off += len + (len & 1);
    }
    if (!pcm || !data) return 0;
    if (rate < 1 || rate > 4000000) return 0;
    if (chans != 1 && chans != 2) return 0;
    if (bits != 8 && bits != 16) return 0;
    size_t frame = (size_t)chans * (size_t)(bits / 8);
    size_t frames = dlen / frame;
    if (frames < 1) return 0;
    out->rate = rate;
    out->pcm.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        const unsigned char* f = data + i * frame;
        int l, r;
        if (bits == 8) {
            // 8-bit WAV is unsigned with 128 as silence.
            l = ((int)f[0] - 128) * 256;
            r = (chans == 2) ? ((int)f[1] - 128) * 256 : l;
        } else {
            l = __nexa_wav_s16(f);
            r = (chans == 2) ? __nexa_wav_s16(f + 2) : l;
        }
        // The stream is mono, so stereo folds down rather than picking a side.
        out->pcm[i] = (short)((chans == 2) ? (l + r) / 2 : l);
    }
    return 1;
}

static int __nexa_gfx_sound(const std::string& path) {
    if (path.empty()) return 0;
    if (__nexa_snds.empty()) {
        __nexa_Snd z;
        z.rate = 0;
        __nexa_snds.push_back(z);
        __nexa_snd_paths.push_back("");
    }
    for (size_t i = 1; i < __nexa_snd_paths.size(); i++) {
        if (__nexa_snd_paths[i] == path) return (int)i;
    }
    std::string bytes = __nexa_gfx_read_file(path);
    if (bytes.empty()) return 0;
    __nexa_Snd s;
    s.rate = 0;
    if (!__nexa_gfx_wav_decode(bytes, &s)) return 0;
    __nexa_snds.push_back(std::move(s));
    __nexa_snd_paths.push_back(path);
    return (int)__nexa_snds.size() - 1;
}

// gfx.play and gfx.loop are the same act with one bit different.
static int __nexa_gfx_voice_start(int snd, int vol, int loop) {
    if (snd < 1 || snd >= (int)__nexa_snds.size()) return 0;
    // Playing a sound is enough to want a stream: open one at the usual rate
    // if the program never did. A stream already open at another rate is left
    // alone -- the voice resamples to whatever it finds.
    if (__nexa_audio_rate < 1 && !__nexa_gfx_audio(44100)) return 0;
    if (__nexa_audio_rate < 1) return 0;
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    int slot = -1;
    for (int i = 0; i < NEXA_VOICES; i++) {
        if (__nexa_voices[i].id == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < NEXA_VOICES; i++) {
            if (__nexa_voices[i].id < __nexa_voices[slot].id) slot = i;
        }
    }
    __nexa_voice_next++;
    if (__nexa_voice_next < 1) __nexa_voice_next = 1;
    __nexa_Voice& v = __nexa_voices[slot];
    v.id = __nexa_voice_next;
    v.snd = snd;
    v.vol = vol;
    v.loop = loop;
    v.pos = 0;
    v.step = ((long long)__nexa_snds[(size_t)snd].rate << 16) / (long long)__nexa_audio_rate;
    if (v.step < 1) v.step = 1;
    return v.id;
}

// gfx.stop() passes 0, which means every voice.
static int __nexa_gfx_stop(int voice) {
    int hit = 0;
    for (int i = 0; i < NEXA_VOICES; i++) {
        if (__nexa_voices[i].id == 0) continue;
        if (voice != 0 && __nexa_voices[i].id != voice) continue;
        __nexa_voices[i].id = 0;
        hit = 1;
    }
    return hit;
}

// gfx.close() closes the stream, so the voices that were feeding it go too.
static void __nexa_gfx_sound_reset() {
    for (int i = 0; i < NEXA_VOICES; i++) __nexa_voices[i].id = 0;
}

static int __nexa_gfx_volume_get() {
    return __nexa_master_vol;
}

static int __nexa_gfx_volume_set(int v) {
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    __nexa_master_vol = v;
    return v;
}

// Called from gfx.poll() and gfx.audio_flush(): keeps the stream about a tenth
// of a second ahead of the speaker, so a normal poll/present loop plays sound
// without the program ever calling gfx.sample.
//
// It returns at once when no voice is live, which is what lets it share the
// stream with a program that synthesises its own samples: silence is never
// pumped, so gfx.sample keeps writing into an otherwise untouched queue.
static void __nexa_gfx_mix_pump() {
    if (__nexa_audio_rate < 1) return;
    int live = 0;
    for (int i = 0; i < NEXA_VOICES; i++) {
        if (__nexa_voices[i].id != 0) live = 1;
    }
    if (!live) return;
    int ahead = __nexa_audio_rate / 10;
    int want = ahead - __nexa_gfx_audio_queued();
    if (want < 1) return;
    if (want > ahead) want = ahead;
    for (int k = 0; k < want; k++) {
        int acc = 0;
        for (int i = 0; i < NEXA_VOICES; i++) {
            __nexa_Voice& v = __nexa_voices[i];
            if (v.id == 0) continue;
            const __nexa_Snd& s = __nexa_snds[(size_t)v.snd];
            long long len = (long long)s.pcm.size();
            if ((v.pos >> 16) >= len) {
                if (!v.loop) {
                    v.id = 0;
                    continue;
                }
                v.pos %= (len << 16);
            }
            // Two 0..255 gains rather than one 0..65025 multiply, so the
            // product stays comfortably inside an int on every voice.
            int g = v.vol * __nexa_master_vol / 255;
            acc += (int)s.pcm[(size_t)(v.pos >> 16)] * g / 255;
            v.pos += v.step;
        }
        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        if (!__nexa_gfx_sample(acc)) break;
    }
}
)NEXA_GFX";
}

inline std::string gfxRuntimeCpp(const GfxNeed& need) {
    // Internal dependency closure: each of these is "some helper we are about
    // to emit calls it", worked out once here instead of at every use.
    const bool wantLine = need.line || need.lineThick || need.shapesOutline;
    // Rounded rectangles are stacks of spans like every other filled shape;
    // arcs and pies go through the span writer that filters by angle, but they
    // still need the row range it is paired with.
    const bool wantRound = need.roundRect || need.fillRoundRect;
    const bool wantSector = need.arc || need.pie;
    // Turning an angle in degrees into a direction: a sweep needs it for its
    // two rays, a rotated blit for the turn itself.
    const bool wantCosSin = wantSector || need.blitRot;
    const bool wantSpan = need.shapesFill || need.shapesOutline || need.lineThick ||
                          wantRound || wantSector;
    const bool wantEllipse = need.shapesFill || need.shapesOutline || wantRound || wantSector;
    // The per-row runs of an ellipse outline: gfx.circle/gfx.ellipse draw all
    // of them, gfx.arc the ones the sweep keeps.
    const bool wantEllipseRuns = need.shapesOutline || need.arc;
    const bool wantPolyTake = need.shapesFill || need.shapesOutline;
    const bool wantU8 = need.shapesFill || need.shapesOutline || need.line ||
                        need.lineThick || need.text || wantRound || wantSector;
    const bool wantGet = need.get || need.save;
    // Both the image decoder and the WAV loader start by slurping a file.
    const bool wantReadFile = need.imageLoad || need.sound;
    const bool wantDraw = need.plot || need.shapesFill || need.shapesOutline ||
                          need.line || need.lineThick || need.text || wantRound ||
                          wantSector;
    const bool wantPutA = wantDraw || need.blit || need.blitRot;

    std::string out;
    out += R"NEXA_GFX(
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <objbase.h>
#include <wincodec.h>
#include <mmsystem.h>
#elif defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif
)NEXA_GFX";
    // The frame limiter reads a monotonic clock and waits on it. Windows has
    // both in windows.h and Emscripten has both in emscripten.h, already
    // included above; everywhere else it is POSIX, and clock_gettime and
    // nanosleep live in the same header.
    if (need.maxfps) out += R"NEXA_GFX(
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <cerrno>
#include <time.h>
#endif
)NEXA_GFX";
    out += R"NEXA_GFX(
struct __nexa_Gfx {
    int w;
    int h;
    int scale;
    int closed;
    int ready;
    int mx;
    int my;
    int min;
    int mlb;
    int mmb;
    int mrb;
    int text_scale;
    int fullscreen;
    // 1 while the OS cursor is visible over the window, which is how a window
    // starts. Kept here rather than asked of the OS: X11 has no "is my cursor
    // hidden" query, and NSCursor's hide/unhide is a counter that only stays
    // balanced if the program remembers what it last asked for.
    int cursor;
    unsigned char* fb;
    std::string title;
    std::string drop_path;
    int k_now[64];
    int k_prev[64];
    // Wheel and typed text are edge events, so they are collected as they arrive
    // and published as a whole-frame value by gfx.poll(). The *_acc fields are
    // what the backend event handlers write to; the published fields are what
    // gfx.wheel()/gfx.wheel_x()/gfx.typed() read.
    double wheel_acc_y;
    double wheel_acc_x;
    int wheel_y;
    int wheel_x;
    std::string type_acc;
    std::string type_buf;
    // A DBCS lead byte held over from one WM_CHAR to the next (Win32 only).
    int type_lead;
)NEXA_GFX";
    if (need.maxfps) out += R"NEXA_GFX(
    // gfx.maxfps: the frame period in nanoseconds, 0 while uncapped, and the
    // monotonic timestamp the next frame is due at. A deadline rather than a
    // duration, because a frame that took 3 ms of a 20 ms budget has to be
    // followed by a 17 ms wait, not another 20 -- sleeping a fixed amount every
    // frame drifts by however long the drawing took.
    long long fps_period;
    long long fps_due;
)NEXA_GFX";
    out += R"NEXA_GFX(
#ifdef _WIN32
    HWND hwnd;
    BITMAPINFO bmi;
    WINDOWPLACEMENT wnd_place;
    LONG wnd_style;
    // Icons are owned by whoever created them: WM_SETICON does not take them
    // over, so the two we made are ours to destroy when they are replaced.
    HICON icon_big;
    HICON icon_small;
#endif
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    Display* dpy;
    Window win;
    GC gc;
    XImage* img;
    Visual* vis;
    unsigned char* xbuf;
    int xbw;
    int xbh;
    int wm_delete;
    // The 1x1 fully-transparent cursor gfx.cursor(0) points the window at, and
    // the pixmap it is cut from. Made once, on the first hide.
    Cursor blank_cursor;
    Pixmap blank_pixmap;
#endif
#ifdef __EMSCRIPTEN__
    int keys[512];
#endif
};

static __nexa_Gfx __nexa_g = {};

static void __nexa_gfx_clear(int r, int g, int b);
static void __nexa_gfx_present();
static int __nexa_gfx_alpha_set(int a);
)NEXA_GFX";
    if (need.typed) out += R"NEXA_GFX(
// --- typed-text queue -------------------------------------------------------
// gfx.typed() reports characters, not keys, so every backend hands its own
// notion of "the user typed something" to these two helpers and they do the
// filtering in one place. A frame's text is capped so that a program which
// stops calling gfx.poll() cannot grow the queue without bound.
static const size_t __nexa_gfx_type_cap = 1024;

// Appends one code point as UTF-8, dropping anything that is not printable
// text: C0 controls, DEL, the C1 range (which is what a Latin-1 control byte
// decodes to), surrogates, and out-of-range values.
static void __nexa_gfx_type_push_cp(unsigned cp) {
    if (cp < 0x20u || cp == 0x7Fu) return;
    if (cp >= 0x80u && cp <= 0x9Fu) return;
    if (cp >= 0xD800u && cp <= 0xDFFFu) return;
    if (cp > 0x10FFFFu) return;
    char out[4];
    int n = 0;
    if (cp < 0x80u) {
        out[n++] = (char)cp;
    } else if (cp < 0x800u) {
        out[n++] = (char)(0xC0u | (cp >> 6));
        out[n++] = (char)(0x80u | (cp & 0x3Fu));
    } else if (cp < 0x10000u) {
        out[n++] = (char)(0xE0u | (cp >> 12));
        out[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[n++] = (char)(0x80u | (cp & 0x3Fu));
    } else {
        out[n++] = (char)(0xF0u | (cp >> 18));
        out[n++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[n++] = (char)(0x80u | (cp & 0x3Fu));
    }
    if (__nexa_g.type_acc.size() + (size_t)n > __nexa_gfx_type_cap) return;
    __nexa_g.type_acc.append(out, (size_t)n);
}

#if defined(_WIN32) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
// Appends UTF-8 bytes, decoding as it goes so that a malformed sequence from a
// backend is dropped rather than corrupting the queue. n < 0 means NUL-terminated.
static void __nexa_gfx_type_push_utf8(const char* s, int n) {
    if (!s) return;
    size_t len = (n < 0) ? std::strlen(s) : (size_t)n;
    size_t i = 0;
    while (i < len) {
        unsigned char b = (unsigned char)s[i];
        unsigned cp = 0;
        size_t extra = 0;
        if (b < 0x80u) { cp = b; extra = 0; }
        else if ((b & 0xE0u) == 0xC0u) { cp = b & 0x1Fu; extra = 1; }
        else if ((b & 0xF0u) == 0xE0u) { cp = b & 0x0Fu; extra = 2; }
        else if ((b & 0xF8u) == 0xF0u) { cp = b & 0x07u; extra = 3; }
        else { i++; continue; }              // stray continuation or invalid lead
        if (i + extra >= len) break;         // sequence runs past the end
        size_t j = 1;
        for (; j <= extra; j++) {
            unsigned char c = (unsigned char)s[i + j];
            if ((c & 0xC0u) != 0x80u) break;
            cp = (cp << 6) | (unsigned)(c & 0x3Fu);
        }
        if (j <= extra) { i++; continue; }   // bad continuation; resync one byte on
        __nexa_gfx_type_push_cp(cp);
        i += extra + 1;
    }
}
#endif

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
// Latin-1 bytes, as XLookupString hands them back.
static void __nexa_gfx_type_push_latin1(const char* s, int n) {
    for (int i = 0; i < n; i++) __nexa_gfx_type_push_cp((unsigned char)s[i]);
}
#endif
)NEXA_GFX";
    else out += R"NEXA_GFX(
// gfx.typed() is never called, so nothing reads the typed-text queue. The
// backends still report "the user typed something" -- that is wired into the
// window procedure and the X11 event loop -- so the helpers they hand it to
// stay, as no-ops.
#if defined(_WIN32) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
static void __nexa_gfx_type_push_utf8(const char*, int) {}
#endif

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
static void __nexa_gfx_type_push_latin1(const char*, int) {}
#endif
)NEXA_GFX";
    out += R"NEXA_GFX(
// --- wheel accumulator ------------------------------------------------------
// Backends report scrolling in wildly different units (whole notches on X11,
// 1/120ths on Win32, pixels from a trackpad). Each converts to fractional
// notches and adds them here; gfx.poll() publishes the whole part and keeps the
// remainder, so a slow trackpad scroll still eventually reports a notch instead
// of rounding to nothing every frame.
//
// Sign, on every backend: +y is up / away from the user, +x is to the right.
static void __nexa_gfx_wheel_add(double dx, double dy) {
    double ax = __nexa_g.wheel_acc_x + dx;
    double ay = __nexa_g.wheel_acc_y + dy;
    // Keep a runaway, absurd or non-finite delta away from the int conversion
    // in __nexa_gfx_input_publish, where it would be undefined behaviour.
    // x != x is true only for NaN, and needs no <cmath>.
    if (ax != ax) ax = 0.0;
    if (ay != ay) ay = 0.0;
    if (ax > 1e6) ax = 1e6;
    if (ax < -1e6) ax = -1e6;
    if (ay > 1e6) ay = 1e6;
    if (ay < -1e6) ay = -1e6;
    __nexa_g.wheel_acc_x = ax;
    __nexa_g.wheel_acc_y = ay;
}

static int __nexa_gfx_map_mouse(int px, int py, int cw, int ch, int* ox, int* oy) {
    if (cw < 1 || ch < 1 || __nexa_g.w < 1 || __nexa_g.h < 1) return 0;
    if (px < 0 || py < 0 || px >= cw || py >= ch) return 0;
    int x = px * __nexa_g.w / cw;
    int y = py * __nexa_g.h / ch;
    if (x >= __nexa_g.w) x = __nexa_g.w - 1;
    if (y >= __nexa_g.h) y = __nexa_g.h - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *ox = x;
    *oy = y;
    return 1;
}

static void __nexa_gfx_mouse_refresh();
)NEXA_GFX";
    if (need.keys) out += "static void __nexa_gfx_key_snapshot();\n";
    out += R"NEXA_GFX(static int __nexa_gfx_has_focus();

static void __nexa_gfx_mouse_apply(int x, int y, int inside, int left, int middle, int right) {
    if (inside) {
        __nexa_g.mx = x;
        __nexa_g.my = y;
        __nexa_g.min = 1;
        __nexa_g.mlb = left ? 1 : 0;
        __nexa_g.mmb = middle ? 1 : 0;
        __nexa_g.mrb = right ? 1 : 0;
    } else {
        __nexa_g.min = 0;
        __nexa_g.mlb = 0;
        __nexa_g.mmb = 0;
        __nexa_g.mrb = 0;
    }
}

#ifdef __APPLE__
@interface __NexaGfxDelegate : NSObject <NSWindowDelegate>
@end
@implementation __NexaGfxDelegate
- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    __nexa_g.closed = 1;
    return YES;
}
@end

@interface __NexaGfxView : NSView
@end
@implementation __NexaGfxView
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType]];
    }
    return self;
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    (void)sender;
    return NSDragOperationCopy;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSArray* files = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if ([files count] < 1) return NO;
    NSString* p = [files objectAtIndex:0];
    if (!p) return NO;
    __nexa_g.drop_path = [p UTF8String];
    return YES;
}
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!__nexa_g.fb || __nexa_g.w < 1 || __nexa_g.h < 1) return;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) return;
    CGContextRef bmp = CGBitmapContextCreate(
        __nexa_g.fb, (size_t)__nexa_g.w, (size_t)__nexa_g.h, 8,
        (size_t)__nexa_g.w * 4, cs,
        (CGBitmapInfo)kCGImageAlphaNoneSkipLast | (CGBitmapInfo)kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!bmp) return;
    CGImageRef img = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    if (!img) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
    NSRect b = [self bounds];
    CGContextTranslateCTM(ctx, 0, b.size.height);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, b.size.width, b.size.height), img);
    CGContextRestoreGState(ctx);
    CGImageRelease(img);
}
@end

static NSWindow* __nexa_gfx_nswin = nil;
static __NexaGfxView* __nexa_gfx_nsview = nil;
static __NexaGfxDelegate* __nexa_gfx_delegate = nil;
#endif

#ifdef _WIN32
static void __nexa_gfx_bb_free();
#endif
)NEXA_GFX";
    // Tearing a window down is core -- gfx.open does it to the window before
    // it, gfx.close to the last one -- but what it has to hand back belongs to
    // the polish features. Both reach them through a declaration that becomes
    // an empty inline body when the feature is sliced out, the same way the
    // audio shutdown further down does.
    out += need.cursor ? "static void __nexa_gfx_cursor_reset();\n"
                       : "static void __nexa_gfx_cursor_reset() {}\n";
    out += need.icon ? "static void __nexa_gfx_icon_reset();\n"
                     : "static void __nexa_gfx_icon_reset() {}\n";
    out += R"NEXA_GFX(
static void __nexa_gfx_free() {
    __nexa_gfx_cursor_reset();
    __nexa_gfx_icon_reset();
    delete[] __nexa_g.fb;
    __nexa_g.fb = nullptr;
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (__nexa_g.img) {
        __nexa_g.img->data = nullptr;
        XDestroyImage(__nexa_g.img);
        __nexa_g.img = nullptr;
    }
    delete[] __nexa_g.xbuf;
    __nexa_g.xbuf = nullptr;
    __nexa_g.xbw = 0;
    __nexa_g.xbh = 0;
    if (__nexa_g.dpy && __nexa_g.win) {
        XDestroyWindow(__nexa_g.dpy, __nexa_g.win);
        __nexa_g.win = 0;
    }
    if (__nexa_g.dpy) {
        XCloseDisplay(__nexa_g.dpy);
        __nexa_g.dpy = nullptr;
    }
    __nexa_g.vis = nullptr;
#endif
#ifdef _WIN32
    __nexa_gfx_bb_free();
    if (__nexa_g.hwnd) {
        DestroyWindow(__nexa_g.hwnd);
        __nexa_g.hwnd = nullptr;
    }
#endif
#ifdef __APPLE__
    if (__nexa_gfx_nswin) {
        [__nexa_gfx_nswin setDelegate:nil];
        [__nexa_gfx_nswin close];
        __nexa_gfx_nswin = nil;
    }
    __nexa_gfx_nsview = nil;
    __nexa_gfx_delegate = nil;
#endif
    __nexa_g.ready = 0;
    __nexa_g.min = 0;
    __nexa_g.mlb = 0;
    __nexa_g.mmb = 0;
    __nexa_g.mrb = 0;
    // A closed window reports no scrolling and no typed text, the same way it
    // reports no keys and no mouse buttons.
    __nexa_g.wheel_acc_x = 0.0;
    __nexa_g.wheel_acc_y = 0.0;
    __nexa_g.wheel_x = 0;
    __nexa_g.wheel_y = 0;
    __nexa_g.type_acc.clear();
    __nexa_g.type_buf.clear();
    __nexa_g.type_lead = 0;
}

static int __nexa_gfx_fullscreen(int on);

#ifdef _WIN32
static const DWORD __nexa_gfx_overlapped =
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
static HDC __nexa_bb_dc = NULL;
static HBITMAP __nexa_bb_bmp = NULL;
static int __nexa_bb_w = 0;
static int __nexa_bb_h = 0;

static void __nexa_gfx_bb_free() {
    if (__nexa_bb_dc) {
        DeleteDC(__nexa_bb_dc);
        __nexa_bb_dc = NULL;
    }
    if (__nexa_bb_bmp) {
        DeleteObject(__nexa_bb_bmp);
        __nexa_bb_bmp = NULL;
    }
    __nexa_bb_w = 0;
    __nexa_bb_h = 0;
}

static int __nexa_gfx_bb_lock(int w, int h) {
    if (w < 1 || h < 1) return 0;
    if (__nexa_bb_dc && __nexa_bb_w == w && __nexa_bb_h == h) return 1;
    __nexa_gfx_bb_free();
    if (!__nexa_g.hwnd) return 0;
    HDC wnd = GetDC(__nexa_g.hwnd);
    if (!wnd) return 0;
    HDC dc = CreateCompatibleDC(wnd);
    HBITMAP bmp = CreateCompatibleBitmap(wnd, w, h);
    ReleaseDC(__nexa_g.hwnd, wnd);
    if (!dc || !bmp) {
        if (dc) DeleteDC(dc);
        if (bmp) DeleteObject(bmp);
        return 0;
    }
    SelectObject(dc, bmp);
    __nexa_bb_dc = dc;
    __nexa_bb_bmp = bmp;
    __nexa_bb_w = w;
    __nexa_bb_h = h;
    return 1;
}

static void __nexa_gfx_blit_letterbox(HDC hdc, int cw, int ch) {
    if (!__nexa_g.fb || __nexa_g.w < 1 || __nexa_g.h < 1 || cw < 1 || ch < 1) return;
    int sx = cw / __nexa_g.w;
    int sy = ch / __nexa_g.h;
    int sc = sx < sy ? sx : sy;
    if (sc < 1) sc = 1;
    int dw = __nexa_g.w * sc;
    int dh = __nexa_g.h * sc;
    if (dw > cw) dw = cw;
    if (dh > ch) dh = ch;
    int ox = (cw - dw) / 2;
    int oy = (ch - dh) / 2;
    SetStretchBltMode(hdc, COLORONCOLOR);
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (oy > 0) {
        RECT r = {0, 0, cw, oy};
        FillRect(hdc, &r, black);
    }
    if (oy + dh < ch) {
        RECT r = {0, oy + dh, cw, ch};
        FillRect(hdc, &r, black);
    }
    if (ox > 0) {
        RECT r = {0, oy, ox, oy + dh};
        FillRect(hdc, &r, black);
    }
    if (ox + dw < cw) {
        RECT r = {ox + dw, oy, cw, oy + dh};
        FillRect(hdc, &r, black);
    }
    StretchDIBits(hdc, ox, oy, dw, dh, 0, 0, __nexa_g.w, __nexa_g.h,
        __nexa_g.fb, &__nexa_g.bmi, DIB_RGB_COLORS, SRCCOPY);
}

static void __nexa_gfx_flip(HDC dst, int cw, int ch) {
    if (__nexa_gfx_bb_lock(cw, ch)) {
        __nexa_gfx_blit_letterbox(__nexa_bb_dc, cw, ch);
        BitBlt(dst, 0, 0, cw, ch, __nexa_bb_dc, 0, 0, SRCCOPY);
    } else {
        __nexa_gfx_blit_letterbox(dst, cw, ch);
    }
}

static LRESULT CALLBACK __nexa_gfx_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE) {
        __nexa_g.closed = 1;
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        if (__nexa_g.hwnd == hwnd) __nexa_g.hwnd = nullptr;
        return 0;
    }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE && __nexa_g.fullscreen) {
        __nexa_gfx_fullscreen(0);
        return 0;
    }
    if (msg == WM_SYSCOMMAND) {
        UINT cmd = (UINT)(wParam & 0xFFF0);
        if (cmd == SC_MINIMIZE) {
            if (__nexa_g.fullscreen) __nexa_gfx_fullscreen(0);
        } else if (cmd == SC_MAXIMIZE) {
            if (!IsIconic(hwnd)) {
                __nexa_gfx_fullscreen(1);
                return 0;
            }
        } else if (cmd == SC_RESTORE) {
            if (IsIconic(hwnd)) {
                return DefWindowProcA(hwnd, msg, wParam, lParam);
            }
            if (__nexa_g.fullscreen) {
                __nexa_gfx_fullscreen(0);
                return 0;
            }
        }
    }
    // The window class is ANSI (RegisterClassA / DispatchMessageA), so WM_CHAR
    // arrives as one byte in the process code page — possibly the lead byte of
    // a DBCS pair. Pair it up, then convert through UTF-16 to UTF-8.
    if (msg == WM_CHAR) {
        char mb[2];
        int mbn = 0;
        if (__nexa_g.type_lead) {
            mb[0] = (char)(unsigned char)__nexa_g.type_lead;
            mb[1] = (char)(unsigned char)wParam;
            mbn = 2;
            __nexa_g.type_lead = 0;
        } else if (IsDBCSLeadByteEx(CP_ACP, (BYTE)wParam)) {
            __nexa_g.type_lead = (int)(unsigned char)wParam;
            return 0;
        } else {
            mb[0] = (char)(unsigned char)wParam;
            mbn = 1;
        }
        wchar_t wide[4];
        int wn = MultiByteToWideChar(CP_ACP, 0, mb, mbn, wide, 4);
        if (wn > 0) {
            char utf8[16];
            int un = WideCharToMultiByte(CP_UTF8, 0, wide, wn, utf8, (int)sizeof(utf8), nullptr, nullptr);
            if (un > 0) __nexa_gfx_type_push_utf8(utf8, un);
        }
        return 0;
    }
    if (msg == WM_MOUSEWHEEL) {
        __nexa_gfx_wheel_add(0.0, (double)GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA);
        return 0;
    }
    // Windows reports a positive WM_MOUSEHWHEEL delta for a rightward tilt,
    // which is the sign gfx.wheel_x() promises.
    if (msg == WM_MOUSEHWHEEL) {
        __nexa_gfx_wheel_add((double)GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA, 0.0);
        return 0;
    }
    if (msg == WM_DROPFILES) {
        HDROP drop = (HDROP)wParam;
        char path[MAX_PATH];
        if (DragQueryFileA(drop, 0, path, MAX_PATH) > 0) __nexa_g.drop_path = path;
        DragFinish(drop);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!IsIconic(hwnd)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            __nexa_gfx_flip(hdc, rc.right, rc.bottom);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
#endif

#ifdef __EMSCRIPTEN__
// True when a DOM KeyboardEvent.key holds a single character rather than a key
// name: "a", "A", "€", " " are text; "ArrowUp", "Shift", "Enter" are not.
static int __nexa_gfx_dom_key_is_text(const char* k) {
    if (!k || !k[0]) return 0;
    unsigned char b = (unsigned char)k[0];
    size_t want = 1;
    if ((b & 0xE0u) == 0xC0u) want = 2;
    else if ((b & 0xF0u) == 0xE0u) want = 3;
    else if ((b & 0xF8u) == 0xF0u) want = 4;
    else if (b >= 0x80u) return 0;
    return std::strlen(k) == want;
}

static EM_BOOL __nexa_gfx_ekey(int type, const EmscriptenKeyboardEvent* e, void*) {
    int down = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? 1 : 0;
    int code = (int)e->keyCode;
    if (code >= 0 && code < 512) __nexa_g.keys[code] = down;
    // keypress is deprecated, so character input comes off keydown: the browser
    // has already applied shift and the keyboard layout to e->key.
    if (down && !e->ctrlKey && !e->altKey && !e->metaKey &&
        __nexa_gfx_dom_key_is_text(e->key)) {
        __nexa_gfx_type_push_utf8(e->key, -1);
    }
    if (code == 27 && down && __nexa_g.fullscreen) {
        __nexa_gfx_fullscreen(0);
        return EM_TRUE;
    }
    return EM_TRUE;
}

static EM_BOOL __nexa_gfx_emouse(int type, const EmscriptenMouseEvent* e, void*) {
    if (type == EMSCRIPTEN_EVENT_MOUSELEAVE) {
        __nexa_gfx_mouse_apply(0, 0, 0, 0, 0, 0);
        return EM_TRUE;
    }
    double css_w = 0, css_h = 0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    int cw = (int)css_w;
    int ch = (int)css_h;
    if (cw < 1) cw = __nexa_g.w;
    if (ch < 1) ch = __nexa_g.h;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse((int)e->targetX, (int)e->targetY, cw, ch, &ox, &oy);
    unsigned short bt = e->buttons;
    __nexa_gfx_mouse_apply(ox, oy, inside, (bt & 1) != 0, (bt & 4) != 0, (bt & 2) != 0);
    return EM_TRUE;
}

static EM_BOOL __nexa_gfx_ewheel(int, const EmscriptenWheelEvent* e, void*) {
    // DOM deltas depend on deltaMode: 0 is pixels, 1 is lines, 2 is pages. Scale
    // each to notches. DOM deltaY is positive scrolling *down*, the opposite of
    // gfx.wheel(), so it is negated; deltaX is already positive to the right.
    double div = 120.0;
    if (e->deltaMode == DOM_DELTA_LINE) div = 3.0;
    else if (e->deltaMode == DOM_DELTA_PAGE) div = 1.0;
    __nexa_gfx_wheel_add(e->deltaX / div, -e->deltaY / div);
    return EM_TRUE;
}
#endif

static const int __nexa_gfx_max = 4096;

static void __nexa_gfx_clamp_whs(int* w, int* h, int* scale, int defaultScale) {
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
    if (*w > __nexa_gfx_max) *w = __nexa_gfx_max;
    if (*h > __nexa_gfx_max) *h = __nexa_gfx_max;
    if (*scale < 1) *scale = defaultScale < 1 ? 12 : defaultScale;
    if (*scale > 64) *scale = 64;
}
)NEXA_GFX";
    if (need.window) out += R"NEXA_GFX(
static void __nexa_gfx_drop_x11_image() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (__nexa_g.img) {
        __nexa_g.img->data = nullptr;
        XDestroyImage(__nexa_g.img);
        __nexa_g.img = nullptr;
    }
    delete[] __nexa_g.xbuf;
    __nexa_g.xbuf = nullptr;
    __nexa_g.xbw = 0;
    __nexa_g.xbh = 0;
#endif
}

static void __nexa_gfx_apply_window_size(int w, int h, int scale) {
    if (__nexa_g.fullscreen) return;
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (!c) return;
        c.width = $0;
        c.height = $1;
        c.style.width = ($0 * $2) + 'px';
        c.style.height = ($1 * $2) + 'px';
    }), w, h, scale);
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return;
    __nexa_g.bmi.bmiHeader.biWidth = w;
    __nexa_g.bmi.bmiHeader.biHeight = -h;
    DWORD style = (DWORD)GetWindowLongA(__nexa_g.hwnd, GWL_STYLE);
    RECT wr = {0, 0, w * scale, h * scale};
    AdjustWindowRect(&wr, style, FALSE);
    SetWindowPos(__nexa_g.hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(__nexa_g.hwnd, nullptr, FALSE);
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return;
    NSSize sz = NSMakeSize((CGFloat)(w * scale), (CGFloat)(h * scale));
    [__nexa_gfx_nswin setContentSize:sz];
    if (__nexa_gfx_nsview) {
        [__nexa_gfx_nsview setFrame:NSMakeRect(0, 0, sz.width, sz.height)];
        [__nexa_gfx_nsview setNeedsDisplay:YES];
    }
#elif defined(__linux__)
    __nexa_gfx_drop_x11_image();
    if (__nexa_g.dpy && __nexa_g.win) {
        XResizeWindow(__nexa_g.dpy, __nexa_g.win, (unsigned)(w * scale), (unsigned)(h * scale));
        XFlush(__nexa_g.dpy);
    }
#endif
}
)NEXA_GFX";
    out += R"NEXA_GFX(
static int __nexa_gfx_open(const std::string& title, int w, int h, int scale) {
    __nexa_gfx_clamp_whs(&w, &h, &scale, 12);
    __nexa_gfx_free();
    // A new window starts opaque, the same way it starts at text size 1.
    __nexa_gfx_alpha_set(255);
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.scale = scale;
    __nexa_g.closed = 0;
    __nexa_g.fullscreen = 0;
    // A new window starts with the cursor showing, the same way it starts
    // opaque and windowed.
    __nexa_g.cursor = 1;
    __nexa_g.mx = 0;
    __nexa_g.my = 0;
    __nexa_g.min = 0;
    __nexa_g.mlb = 0;
    __nexa_g.mmb = 0;
    __nexa_g.mrb = 0;
    if (__nexa_g.text_scale < 1) __nexa_g.text_scale = 1;
    __nexa_g.title = title;
    __nexa_g.drop_path.clear();
    std::memset(__nexa_g.k_now, 0, sizeof(__nexa_g.k_now));
    std::memset(__nexa_g.k_prev, 0, sizeof(__nexa_g.k_prev));
    __nexa_g.wheel_acc_x = 0.0;
    __nexa_g.wheel_acc_y = 0.0;
    __nexa_g.wheel_x = 0;
    __nexa_g.wheel_y = 0;
    __nexa_g.type_acc.clear();
    __nexa_g.type_buf.clear();
    __nexa_g.type_lead = 0;
    __nexa_g.fb = new unsigned char[(size_t)w * (size_t)h * 4];
    std::memset(__nexa_g.fb, 0, (size_t)w * (size_t)h * 4);
    __nexa_gfx_clear(0, 0, 0);
#ifdef __EMSCRIPTEN__
    std::memset(__nexa_g.keys, 0, sizeof(__nexa_g.keys));
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, __nexa_gfx_ekey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, __nexa_gfx_ekey);
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (!c) {
            c = document.createElement('canvas');
            c.id = 'canvas';
            document.body.appendChild(c);
        }
        Module['canvas'] = c;
        c.width = $0;
        c.height = $1;
        c.style.width = ($0 * $2) + 'px';
        c.style.height = ($1 * $2) + 'px';
        c.style.imageRendering = 'pixelated';
        c.style.background = '#000';
        document.title = UTF8ToString($3);
        Module["nexaDropPath"] = "";
        var cnv = Module["canvas"] || document.getElementById("canvas");
        var inp = document.getElementById("nexa-file");
        if (!inp) {
            inp = document.createElement("input");
            inp.type = "file";
            inp.id = "nexa-file";
            inp.style.display = "none";
            document.body.appendChild(inp);
        }
        function nexaTakeFile(f) {
            if (!f) return;
            var r = new FileReader();
            r.onload = function() {
                var u8 = new Uint8Array(r.result);
                var name = "/tmp/" + f.name;
                if (typeof FS !== "undefined" && FS.writeFile) FS.writeFile(name, u8);
                Module["nexaDropPath"] = name;
            };
            r.readAsArrayBuffer(f);
        }
        inp.onchange = function() {
            nexaTakeFile(inp.files && inp.files[0]);
            inp.value = "";
        };
        function nexaBindDrop(el) {
            if (!el || el.getAttribute('data-nexa-drop')) return;
            el.setAttribute('data-nexa-drop', '1');
            el.addEventListener('dragover', function(e) { e.preventDefault(); });
            el.addEventListener('drop', function(e) {
                e.preventDefault();
                nexaTakeFile(e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0]);
            });
        }
        nexaBindDrop(cnv);
        nexaBindDrop(document.body);
    }), w, h, scale, title.c_str());
    emscripten_set_mousemove_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mousedown_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mouseup_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mouseleave_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_wheel_callback("#canvas", 0, 1, __nexa_gfx_ewheel);
    __nexa_g.ready = 1;
    __nexa_gfx_present();
    return 1;
#elif defined(_WIN32)
    WNDCLASSA wc = {};
    wc.lpfnWndProc = __nexa_gfx_wndproc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "NexaGfx";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);
    RECT wr = {0, 0, w * scale, h * scale};
    AdjustWindowRect(&wr, __nexa_gfx_overlapped, FALSE);
    __nexa_g.hwnd = CreateWindowExA(WS_EX_APPWINDOW, "NexaGfx", title.c_str(),
        __nexa_gfx_overlapped | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    std::memset(&__nexa_g.bmi, 0, sizeof(__nexa_g.bmi));
    __nexa_g.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    __nexa_g.bmi.bmiHeader.biWidth = w;
    __nexa_g.bmi.bmiHeader.biHeight = -h;
    __nexa_g.bmi.bmiHeader.biPlanes = 1;
    __nexa_g.bmi.bmiHeader.biBitCount = 32;
    __nexa_g.bmi.bmiHeader.biCompression = BI_RGB;
    __nexa_g.ready = __nexa_g.hwnd ? 1 : 0;
    if (__nexa_g.hwnd) DragAcceptFiles(__nexa_g.hwnd, TRUE);
    if (!__nexa_g.hwnd) __nexa_g.closed = 1;
    return __nexa_g.ready;
#elif defined(__APPLE__)
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        static int launched = 0;
        if (!launched) {
            [NSApp finishLaunching];
            launched = 1;
        }
        NSRect content = NSMakeRect(0, 0, (CGFloat)(w * scale), (CGFloat)(h * scale));
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        __nexa_gfx_nswin = [[NSWindow alloc] initWithContentRect:content
            styleMask:style backing:NSBackingStoreBuffered defer:NO];
        [__nexa_gfx_nswin setReleasedWhenClosed:NO];
        [__nexa_gfx_nswin setTitle:[NSString stringWithUTF8String:title.c_str()]];
        __nexa_gfx_nsview = [[__NexaGfxView alloc] initWithFrame:content];
        [__nexa_gfx_nswin setContentView:__nexa_gfx_nsview];
        __nexa_gfx_delegate = [[__NexaGfxDelegate alloc] init];
        [__nexa_gfx_nswin setDelegate:__nexa_gfx_delegate];
        [__nexa_gfx_nswin center];
        [__nexa_gfx_nswin makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        __nexa_g.ready = __nexa_gfx_nswin ? 1 : 0;
        if (!__nexa_g.ready) __nexa_g.closed = 1;
        return __nexa_g.ready;
    }
#elif defined(__linux__)
    XInitThreads();
    __nexa_g.dpy = XOpenDisplay(nullptr);
    if (!__nexa_g.dpy) {
        __nexa_g.closed = 1;
        return 0;
    }
    int scr = DefaultScreen(__nexa_g.dpy);
    __nexa_g.vis = DefaultVisual(__nexa_g.dpy, scr);
    __nexa_g.win = XCreateSimpleWindow(__nexa_g.dpy, RootWindow(__nexa_g.dpy, scr),
        80, 80, (unsigned)(w * scale), (unsigned)(h * scale), 1,
        BlackPixel(__nexa_g.dpy, scr), BlackPixel(__nexa_g.dpy, scr));
    XStoreName(__nexa_g.dpy, __nexa_g.win, title.c_str());
    Atom wm = XInternAtom(__nexa_g.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(__nexa_g.dpy, __nexa_g.win, &wm, 1);
    __nexa_g.wm_delete = (int)wm;
    XSelectInput(__nexa_g.dpy, __nexa_g.win,
        ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask |
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    __nexa_g.gc = DefaultGC(__nexa_g.dpy, scr);
    XMapWindow(__nexa_g.dpy, __nexa_g.win);
    XFlush(__nexa_g.dpy);
    __nexa_g.ready = 1;
    return 1;
#else
    (void)title;
    __nexa_g.closed = 1;
    return 0;
#endif
}
)NEXA_GFX";
    if (need.window) out += R"NEXA_GFX(
static int __nexa_gfx_resize(int w, int h, int scale) {
    if (!__nexa_g.ready || !__nexa_g.fb) return 0;
    __nexa_gfx_clamp_whs(&w, &h, &scale, __nexa_g.scale);
    if (w != __nexa_g.w || h != __nexa_g.h) {
        unsigned char* nfb = new unsigned char[(size_t)w * (size_t)h * 4];
        std::memset(nfb, 0, (size_t)w * (size_t)h * 4);
        int cw = w < __nexa_g.w ? w : __nexa_g.w;
        int ch = h < __nexa_g.h ? h : __nexa_g.h;
        for (int y = 0; y < ch; y++) {
            std::memcpy(nfb + (size_t)y * (size_t)w * 4,
                __nexa_g.fb + (size_t)y * (size_t)__nexa_g.w * 4, (size_t)cw * 4);
        }
        delete[] __nexa_g.fb;
        __nexa_g.fb = nfb;
    }
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.scale = scale;
    if (__nexa_g.mx >= w) __nexa_g.mx = w > 0 ? w - 1 : 0;
    if (__nexa_g.my >= h) __nexa_g.my = h > 0 ? h - 1 : 0;
    __nexa_gfx_apply_window_size(w, h, scale);
    return 1;
}

static int __nexa_gfx_width() {
    return __nexa_g.ready ? __nexa_g.w : 0;
}

static int __nexa_gfx_height() {
    return __nexa_g.ready ? __nexa_g.h : 0;
}

static int __nexa_gfx_scale() {
    return __nexa_g.ready ? __nexa_g.scale : 0;
}
)NEXA_GFX";
    out += R"NEXA_GFX(
static int __nexa_gfx_fullscreen(int on) {
    if (!__nexa_g.ready) return 0;
    if (on < 0) return __nexa_g.fullscreen;
#ifdef __EMSCRIPTEN__
    int want = on ? 1 : 0;
    EM_ASM(({
        var el = Module["canvas"] || document.documentElement;
        if ($0) {
            if (el.requestFullscreen) el.requestFullscreen();
        } else if (document.exitFullscreen) {
            document.exitFullscreen();
        }
    }), want);
    __nexa_g.fullscreen = want;
    return __nexa_g.fullscreen;
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return 0;
    int want = on ? 1 : 0;
    if (want == __nexa_g.fullscreen) return __nexa_g.fullscreen;
    if (want) {
        __nexa_g.wnd_place.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(__nexa_g.hwnd, &__nexa_g.wnd_place);
        __nexa_g.wnd_place.showCmd = SW_SHOWNORMAL;
        LONG style = GetWindowLongA(__nexa_g.hwnd, GWL_STYLE);
        if (style & WS_POPUP) style = __nexa_gfx_overlapped;
        __nexa_g.wnd_style = (style & ~(WS_MAXIMIZE | WS_MINIMIZE)) | WS_VISIBLE;
        HMONITOR mon = MonitorFromWindow(__nexa_g.hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        std::memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoA(mon, &mi)) return __nexa_g.fullscreen;
        SetWindowLongA(__nexa_g.hwnd, GWL_EXSTYLE, GetWindowLongA(__nexa_g.hwnd, GWL_EXSTYLE) | WS_EX_APPWINDOW);
        SetWindowLongA(__nexa_g.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(__nexa_g.hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        __nexa_g.fullscreen = 1;
    } else {
        LONG style = __nexa_g.wnd_style ? __nexa_g.wnd_style : __nexa_gfx_overlapped;
        SetWindowLongA(__nexa_g.hwnd, GWL_STYLE, style | WS_VISIBLE);
        __nexa_g.wnd_place.length = sizeof(WINDOWPLACEMENT);
        __nexa_g.wnd_place.showCmd = SW_SHOWNORMAL;
        SetWindowPlacement(__nexa_g.hwnd, &__nexa_g.wnd_place);
        SetWindowPos(__nexa_g.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        ShowWindow(__nexa_g.hwnd, SW_SHOWNORMAL);
        __nexa_g.fullscreen = 0;
    }
    InvalidateRect(__nexa_g.hwnd, nullptr, FALSE);
    return __nexa_g.fullscreen;
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return 0;
    int want = on ? 1 : 0;
    if (want != __nexa_g.fullscreen) {
        [__nexa_gfx_nswin toggleFullScreen:nil];
        __nexa_g.fullscreen = want;
    }
    return __nexa_g.fullscreen;
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    int want = on ? 1 : 0;
    if (want == __nexa_g.fullscreen) return __nexa_g.fullscreen;
    Atom wm = XInternAtom(__nexa_g.dpy, "_NET_WM_STATE", False);
    Atom fs = XInternAtom(__nexa_g.dpy, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = __nexa_g.win;
    ev.xclient.message_type = wm;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = want ? 1 : 0;
    ev.xclient.data.l[1] = (long)fs;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 1;
    XSendEvent(__nexa_g.dpy, DefaultRootWindow(__nexa_g.dpy), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(__nexa_g.dpy);
    __nexa_g.fullscreen = want;
    return __nexa_g.fullscreen;
#else
    (void)on;
    return 0;
#endif
}
)NEXA_GFX";
    if (need.window) out += R"NEXA_GFX(
static std::string __nexa_gfx_title_get() {
    return __nexa_g.title;
}

static int __nexa_gfx_title_set(const std::string& s) {
    __nexa_g.title = s;
    if (!__nexa_g.ready) return 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({ document.title = UTF8ToString($0); }, s.c_str());
    return 1;
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return 0;
    return SetWindowTextA(__nexa_g.hwnd, s.c_str()) ? 1 : 0;
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return 0;
    [__nexa_gfx_nswin setTitle:[NSString stringWithUTF8String:s.c_str()]];
    return 1;
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    XStoreName(__nexa_g.dpy, __nexa_g.win, s.c_str());
    XFlush(__nexa_g.dpy);
    return 1;
#else
    return 0;
#endif
}
)NEXA_GFX";
    if (need.cursor) out += R"NEXA_GFX(
// gfx.cursor: whether the OS draws a pointer over the window. on < 0 reports,
// anything else sets and reports back what it set. The answer is kept in
// __nexa_g.cursor rather than asked of the platform, because two of the four
// backends cannot be asked.
static int __nexa_gfx_cursor(int on) {
    if (!__nexa_g.ready) return 0;
    if (on < 0) return __nexa_g.cursor;
    int want = on ? 1 : 0;
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (c) c.style.cursor = $0 ? "" : "none";
    }), want);
    __nexa_g.cursor = want;
    return __nexa_g.cursor;
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return 0;
    // The class cursor, not ShowCursor: ShowCursor is a per-thread counter
    // that goes wrong the moment it is unbalanced, while the class cursor is
    // simply what Windows draws over this window and nothing else. SetCursor
    // makes it take effect now rather than at the next mouse move.
    HCURSOR c = want ? LoadCursorA(nullptr, IDC_ARROW) : nullptr;
    SetClassLongPtrA(__nexa_g.hwnd, GCLP_HCURSOR, (LONG_PTR)c);
    SetCursor(c);
    __nexa_g.cursor = want;
    return __nexa_g.cursor;
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return 0;
    // NSCursor's hide/unhide is a counter, so it is moved only when the state
    // really changes -- two gfx.cursor(0) calls must not take two unhides to
    // undo.
    if (want != __nexa_g.cursor) {
        if (want) [NSCursor unhide];
        else [NSCursor hide];
        __nexa_g.cursor = want;
    }
    return __nexa_g.cursor;
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    if (want) {
        XUndefineCursor(__nexa_g.dpy, __nexa_g.win);
    } else {
        if (!__nexa_g.blank_cursor) {
            // A 1x1 all-zero bitmap used as both the shape and its mask is a
            // cursor with nothing in it, which is how X11 spells "hidden".
            char none[1] = {0};
            __nexa_g.blank_pixmap = XCreateBitmapFromData(__nexa_g.dpy, __nexa_g.win, none, 1, 1);
            XColor black;
            std::memset(&black, 0, sizeof(black));
            __nexa_g.blank_cursor = XCreatePixmapCursor(__nexa_g.dpy,
                __nexa_g.blank_pixmap, __nexa_g.blank_pixmap, &black, &black, 0, 0);
        }
        if (!__nexa_g.blank_cursor) return __nexa_g.cursor;
        XDefineCursor(__nexa_g.dpy, __nexa_g.win, __nexa_g.blank_cursor);
    }
    XFlush(__nexa_g.dpy);
    __nexa_g.cursor = want;
    return __nexa_g.cursor;
#else
    (void)want;
    return 0;
#endif
}

// Hands the cursor back when the window goes away: a program that hid it must
// not leave the user's pointer missing, and the X11 cursor is ours to free.
static void __nexa_gfx_cursor_reset() {
#ifdef __EMSCRIPTEN__
    if (!__nexa_g.cursor) {
        EM_ASM({
            var c = Module['canvas'] || document.getElementById('canvas');
            if (c) c.style.cursor = "";
        });
    }
#elif defined(_WIN32)
    if (!__nexa_g.cursor && __nexa_g.hwnd) {
        HCURSOR c = LoadCursorA(nullptr, IDC_ARROW);
        SetClassLongPtrA(__nexa_g.hwnd, GCLP_HCURSOR, (LONG_PTR)c);
        SetCursor(c);
    }
#elif defined(__APPLE__)
    if (!__nexa_g.cursor) [NSCursor unhide];
#elif defined(__linux__)
    if (__nexa_g.dpy) {
        if (__nexa_g.blank_cursor) XFreeCursor(__nexa_g.dpy, __nexa_g.blank_cursor);
        if (__nexa_g.blank_pixmap) XFreePixmap(__nexa_g.dpy, __nexa_g.blank_pixmap);
    }
    __nexa_g.blank_cursor = 0;
    __nexa_g.blank_pixmap = 0;
#endif
    __nexa_g.cursor = 1;
}
)NEXA_GFX";
    out += need.audio ? "\nstatic void __nexa_gfx_audio_close();\n"
                      : "\nstatic void __nexa_gfx_audio_close() {}\n";
    // gfx.close() and gfx.poll() are core, so they are emitted whether or not
    // the program plays a sound. Both reach the mixer through a declaration
    // that becomes an empty inline body when it is sliced out, the same way
    // the audio shutdown above does.
    out += need.sound ? "\nstatic void __nexa_gfx_mix_pump();\nstatic void __nexa_gfx_sound_reset();\n"
                      : "\nstatic void __nexa_gfx_mix_pump() {}\nstatic void __nexa_gfx_sound_reset() {}\n";
    out += R"NEXA_GFX(
static void __nexa_gfx_close() {
    __nexa_gfx_sound_reset();
    __nexa_gfx_audio_close();
    __nexa_g.closed = 1;
    __nexa_gfx_free();
}

// Turns everything the backends accumulated since the last gfx.poll() into the
// values gfx.wheel()/gfx.wheel_x()/gfx.typed() report for this frame. Whole
// notches are published and the fraction is carried forward.
//
// Like gfx.key and gfx.mouse, this input is only visible while the window has
// focus: anything that arrived while it did not is dropped rather than queued
// up to land in the program's lap the moment it comes back.
static void __nexa_gfx_input_publish() {
    if (!__nexa_gfx_has_focus()) {
        __nexa_g.wheel_acc_x = 0.0;
        __nexa_g.wheel_acc_y = 0.0;
        __nexa_g.wheel_x = 0;
        __nexa_g.wheel_y = 0;
        __nexa_g.type_acc.clear();
        __nexa_g.type_buf.clear();
        return;
    }
    int nx = (int)__nexa_g.wheel_acc_x;   // truncates toward zero
    int ny = (int)__nexa_g.wheel_acc_y;
    __nexa_g.wheel_acc_x -= (double)nx;
    __nexa_g.wheel_acc_y -= (double)ny;
    __nexa_g.wheel_x = nx;
    __nexa_g.wheel_y = ny;
    __nexa_g.type_buf.swap(__nexa_g.type_acc);
    __nexa_g.type_acc.clear();
}

static void __nexa_gfx_poll() {
    // Ahead of the window check on purpose: the audio stream is not owned by
    // the window, so a program that plays a sound without opening one still
    // gets its mixer topped up by the poll loop.
    __nexa_gfx_mix_pump();
    if (!__nexa_g.ready) return;
#ifdef _WIN32
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) __nexa_g.closed = 1;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (__nexa_g.hwnd && !IsWindow(__nexa_g.hwnd)) __nexa_g.closed = 1;
#elif defined(__APPLE__)
    @autoreleasepool {
        NSEvent* ev;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:[NSDate distantPast]
            inMode:NSDefaultRunLoopMode
            dequeue:YES])) {
            NSEventType et = [ev type];
            if (et == NSEventTypeScrollWheel) {
                // A trackpad reports pixels ("precise deltas"); a wheel reports
                // whole lines. Scale the former into the same notch unit.
                double s = [ev hasPreciseScrollingDeltas] ? 0.1 : 1.0;
                __nexa_gfx_wheel_add((double)[ev scrollingDeltaX] * s,
                                     (double)[ev scrollingDeltaY] * s);
            } else if (et == NSEventTypeKeyDown) {
                // -characters has already applied shift and the layout. Function
                // keys arrive here too, as private-use code points, and the
                // printable filter in the push helper drops them. Auto-repeat is
                // deliberately not filtered: holding a key types it again, which
                // is what X11 and Win32 do and what a text field wants.
                NSString* chars = [ev characters];
                if (chars) __nexa_gfx_type_push_utf8([chars UTF8String], -1);
            }
            [NSApp sendEvent:ev];
        }
    }
#elif defined(__linux__)
    while (__nexa_g.dpy && XPending(__nexa_g.dpy)) {
        XEvent ev;
        XNextEvent(__nexa_g.dpy, &ev);
        if (ev.type == ClientMessage && (int)ev.xclient.data.l[0] == __nexa_g.wm_delete) __nexa_g.closed = 1;
        if (ev.type == DestroyNotify) __nexa_g.closed = 1;
        if (ev.type == ButtonPress) {
            // X11 sends scrolling as button clicks: 4/5 are up/down and 6/7 are
            // left/right. The matching ButtonRelease is ignored so one click of
            // the wheel counts once.
            switch (ev.xbutton.button) {
                case 4: __nexa_gfx_wheel_add(0.0, 1.0); break;
                case 5: __nexa_gfx_wheel_add(0.0, -1.0); break;
                case 6: __nexa_gfx_wheel_add(-1.0, 0.0); break;
                case 7: __nexa_gfx_wheel_add(1.0, 0.0); break;
                default: break;
            }
        }
        if (ev.type == KeyPress) {
            // XLookupString applies shift and the layout, but only reaches
            // Latin-1: scripts beyond it need an input method, which the
            // runtime does not open (it would mean changing the process locale).
            char buf[32];
            KeySym ks = 0;
            int n = XLookupString(&ev.xkey, buf, (int)sizeof(buf), &ks, nullptr);
            if (n > 0) __nexa_gfx_type_push_latin1(buf, n);
        }
    }
#endif
#ifdef __EMSCRIPTEN__
    {
        char buf[1024];
        int got = EM_ASM_INT(({
            var p = Module["nexaDropPath"] || "";
            if (!p.length) return 0;
            stringToUTF8(p, $0, $1);
            Module["nexaDropPath"] = "";
            return 1;
        }), buf, 1024);
        if (got) __nexa_g.drop_path = buf;
    }
#endif
    __nexa_gfx_mouse_refresh();
)NEXA_GFX";
    if (need.keys) out += "    __nexa_gfx_key_snapshot();\n";
    out += R"NEXA_GFX(    __nexa_gfx_input_publish();
}

static int __nexa_gfx_closed() {
    return __nexa_g.closed;
}

static void __nexa_gfx_mouse_refresh() {
    if (!__nexa_g.ready) return;
#ifdef _WIN32
    if (!__nexa_g.hwnd) return;
    if (!__nexa_gfx_has_focus()) {
        __nexa_gfx_mouse_apply(0, 0, 0, 0, 0, 0);
        return;
    }
    POINT p;
    if (!GetCursorPos(&p)) return;
    if (!ScreenToClient(__nexa_g.hwnd, &p)) return;
    RECT rc;
    if (!GetClientRect(__nexa_g.hwnd, &rc)) return;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse((int)p.x, (int)p.y, (int)rc.right, (int)rc.bottom, &ox, &oy);
    int swap = GetSystemMetrics(SM_SWAPBUTTON);
    int leftVk = swap ? VK_RBUTTON : VK_LBUTTON;
    int rightVk = swap ? VK_LBUTTON : VK_RBUTTON;
    int left = (GetAsyncKeyState(leftVk) & 0x8000) ? 1 : 0;
    int right = (GetAsyncKeyState(rightVk) & 0x8000) ? 1 : 0;
    int middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 1 : 0;
    __nexa_gfx_mouse_apply(ox, oy, inside, left, middle, right);
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return;
    NSPoint s = [NSEvent mouseLocation];
    NSRect wr = [__nexa_gfx_nswin frame];
    NSRect cr = [__nexa_gfx_nswin contentRectForFrameRect:wr];
    int cw = (int)cr.size.width;
    int ch = (int)cr.size.height;
    int px = (int)(s.x - cr.origin.x);
    int py = (int)(cr.size.height - (s.y - cr.origin.y));
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse(px, py, cw, ch, &ox, &oy);
    NSUInteger bt = [NSEvent pressedMouseButtons];
    __nexa_gfx_mouse_apply(ox, oy, inside, (bt & 1) != 0, (bt & 4) != 0, (bt & 2) != 0);
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return;
    Window root = 0, child = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned mask = 0;
    if (!XQueryPointer(__nexa_g.dpy, __nexa_g.win, &root, &child, &rx, &ry, &wx, &wy, &mask)) return;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(__nexa_g.dpy, __nexa_g.win, &wa)) return;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse(wx, wy, wa.width, wa.height, &ox, &oy);
    __nexa_gfx_mouse_apply(ox, oy, inside,
        (mask & Button1Mask) != 0, (mask & Button2Mask) != 0, (mask & Button3Mask) != 0);
#endif
}
)NEXA_GFX";
    if (need.mouse) out += R"NEXA_GFX(
static int __nexa_gfx_mouse_x() {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    return __nexa_g.mx;
}

static int __nexa_gfx_mouse_y() {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    return __nexa_g.my;
}

static int __nexa_gfx_mouse(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "left" || s == "lmb" || s == "l") return __nexa_g.mlb;
    if (s == "right" || s == "rmb" || s == "r") return __nexa_g.mrb;
    if (s == "middle" || s == "mmb" || s == "m") return __nexa_g.mmb;
    return 0;
}
)NEXA_GFX";
    // --- framebuffer rasterizers --------------------------------------------
    //
    // Everything in the chunks that follow reaches the screen only through
    // __nexa_g.fb / __nexa_gfx_put: no window handle, no display connection, no
    // platform call. That is what lets Tests/gfx_shapes_cases.sh lift them out
    // of this header and unit-test the shapes against a stub framebuffer on a
    // machine with no display and no X11 development headers.
    //
    // Each one is bracketed by a [nexa:rasterizers-*] pair sitting on the glue
    // lines either side of the raw literal, so the test's awk lifts the union
    // of the chunk bodies and none of the glue. Keep window, audio and image
    // code out of them, keep the markers spelled exactly as they are, put the
    // begin marker before the `R"NEXA_GFX(` rather than after it -- anything
    // after it lands in the emitted program -- and never spell a marker out in
    // full inside a literal, which would switch the lift on in the middle of a
    // comment. The test fails loudly if it cannot find what it needs.
    //
    // Three more marker pairs further down do the same job for the code that
    // needs the image store or the filesystem and so cannot live in these
    // chunks -- [nexa:imgstore-*] (the loaded-image table), [nexa:blit-*] (the
    // blit itself) and [nexa:screenshot-*] (gfx.save). Tests/gfx_alpha_cases.sh
    // lifts all four.
    out += R"NEXA_GFX(
// --- framebuffer rasterizers ------------------------------------------------
)NEXA_GFX";
    out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_put(int i, unsigned char R, unsigned char G, unsigned char B) {
#ifdef _WIN32
    __nexa_g.fb[i + 0] = B;
    __nexa_g.fb[i + 1] = G;
    __nexa_g.fb[i + 2] = R;
    __nexa_g.fb[i + 3] = 255;
#else
    __nexa_g.fb[i + 0] = R;
    __nexa_g.fb[i + 1] = G;
    __nexa_g.fb[i + 2] = B;
    __nexa_g.fb[i + 3] = 255;
#endif
}

// The global draw alpha behind gfx.alpha(). 255 -- the default, and what
// gfx.open() puts it back to -- means every draw below takes the opaque path
// in __nexa_gfx_put_a and writes exactly the bytes it always has.
static int __nexa_gfx_alpha_v = 255;
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.alpha || need.blit || need.blitRot) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static int __nexa_gfx_alpha_get() {
    return __nexa_gfx_alpha_v;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static int __nexa_gfx_alpha_set(int a) {
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    __nexa_gfx_alpha_v = a;
    return a;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantPutA) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Source-over blend of one pixel, in integers: dst = (src*A + dst*(255-A))/255,
// rounded to nearest. A == 255 is the plain opaque write and A == 0 leaves the
// pixel untouched, so both ends of the range cost nothing extra.
static void __nexa_gfx_put_a(int i, unsigned char R, unsigned char G, unsigned char B, unsigned char A) {
    if (!__nexa_g.fb || A == 0) return;
    if (A == 255) {
        __nexa_gfx_put(i, R, G, B);
        return;
    }
    unsigned char* d = __nexa_g.fb + i;
#ifdef _WIN32
    d[0] = (unsigned char)((B * A + d[0] * (255 - A) + 127) / 255);
    d[1] = (unsigned char)((G * A + d[1] * (255 - A) + 127) / 255);
    d[2] = (unsigned char)((R * A + d[2] * (255 - A) + 127) / 255);
#else
    d[0] = (unsigned char)((R * A + d[0] * (255 - A) + 127) / 255);
    d[1] = (unsigned char)((G * A + d[1] * (255 - A) + 127) / 255);
    d[2] = (unsigned char)((B * A + d[2] * (255 - A) + 127) / 255);
#endif
    d[3] = 255;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantDraw) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Every shape puts its pixels down through here, which is what makes the
// global alpha apply to all of them without a rasterizer having to know it
// exists. Two consequences worth knowing, both documented in SYNTAX/Modules.txt:
// gfx.clear does NOT go through here (clearing is a reset, not a draw), and a
// shape that covers a pixel twice -- the corners of a triangle outline, the
// joins of a thick line -- blends that pixel twice at alpha < 255.
static void __nexa_gfx_draw(int i, unsigned char R, unsigned char G, unsigned char B) {
    __nexa_gfx_put_a(i, R, G, B, (unsigned char)__nexa_gfx_alpha_v);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_clear(int r, int g, int b) {
    if (!__nexa_g.fb) return;
    unsigned char R = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    unsigned char G = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    unsigned char B = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    int n = __nexa_g.w * __nexa_g.h;
    for (int i = 0; i < n; i++) __nexa_gfx_put(i * 4, R, G, B);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.plot) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_plot(int x, int y, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (x < 0 || y < 0 || x >= __nexa_g.w || y >= __nexa_g.h) return;
    unsigned char R = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    unsigned char G = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    unsigned char B = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    __nexa_gfx_draw((y * __nexa_g.w + x) * 4, R, G, B);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantGet) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static int __nexa_gfx_get(int x, int y) {
    if (!__nexa_g.fb || !__nexa_g.ready) return -1;
    if (x < 0 || y < 0 || x >= __nexa_g.w || y >= __nexa_g.h) return -1;
    const unsigned char* p = __nexa_g.fb + ((size_t)y * (size_t)__nexa_g.w + (size_t)x) * 4;
#ifdef _WIN32
    int r = p[2];
    int g = p[1];
    int b = p[0];
#else
    int r = p[0];
    int g = p[1];
    int b = p[2];
#endif
    return (r << 16) | (g << 8) | b;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantU8) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static unsigned char __nexa_gfx_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_fill(int x, int y, int w, int h, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (w < 1 || h < 1) return;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > __nexa_g.w) x1 = __nexa_g.w;
    if (y1 > __nexa_g.h) y1 = __nexa_g.h;
    if (x0 >= x1 || y0 >= y1) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    for (int yy = y0; yy < y1; yy++) {
        int row = yy * __nexa_g.w;
        for (int xx = x0; xx < x1; xx++) {
            __nexa_gfx_draw((row + xx) * 4, R, G, B);
        }
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantLine) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static int __nexa_gfx_outcode(int x, int y) {
    int c = 0;
    if (x < 0) c |= 1;
    else if (x >= __nexa_g.w) c |= 2;
    if (y < 0) c |= 4;
    else if (y >= __nexa_g.h) c |= 8;
    return c;
}

static int __nexa_gfx_clip_line(int* x0, int* y0, int* x1, int* y1) {
    if (__nexa_g.w < 1 || __nexa_g.h < 1) return 0;
    int xmin = 0, ymin = 0, xmax = __nexa_g.w - 1, ymax = __nexa_g.h - 1;
    int c0 = __nexa_gfx_outcode(*x0, *y0);
    int c1 = __nexa_gfx_outcode(*x1, *y1);
    for (;;) {
        if (!(c0 | c1)) return 1;
        if (c0 & c1) return 0;
        int c = c0 ? c0 : c1;
        int dx = *x1 - *x0;
        int dy = *y1 - *y0;
        long long x = 0, y = 0;
        if (c & 8) {
            x = (long long)*x0 + (dy != 0 ? (long long)dx * (ymax - *y0) / dy : 0);
            y = ymax;
        } else if (c & 4) {
            x = (long long)*x0 + (dy != 0 ? (long long)dx * (ymin - *y0) / dy : 0);
            y = ymin;
        } else if (c & 2) {
            y = (long long)*y0 + (dx != 0 ? (long long)dy * (xmax - *x0) / dx : 0);
            x = xmax;
        } else {
            y = (long long)*y0 + (dx != 0 ? (long long)dy * (xmin - *x0) / dx : 0);
            x = xmin;
        }
        if (x < -2147483647ll) x = -2147483647ll;
        if (x > 2147483647ll) x = 2147483647ll;
        if (y < -2147483647ll) y = -2147483647ll;
        if (y > 2147483647ll) y = 2147483647ll;
        if (c == c0) {
            *x0 = (int)x;
            *y0 = (int)y;
            c0 = __nexa_gfx_outcode(*x0, *y0);
        } else {
            *x1 = (int)x;
            *y1 = (int)y;
            c1 = __nexa_gfx_outcode(*x1, *y1);
        }
    }
}

static void __nexa_gfx_line(int x0, int y0, int x1, int y1, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (!__nexa_gfx_clip_line(&x0, &y0, &x1, &y1)) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    int dx = x1 - x0;
    if (dx < 0) dx = -dx;
    int dy = y1 - y0;
    if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0;
    int y = y0;
    for (;;) {
        if (x >= 0 && y >= 0 && x < __nexa_g.w && y < __nexa_g.h) {
            __nexa_gfx_draw((y * __nexa_g.w + x) * 4, R, G, B);
        }
        if (x == x1 && y == y1) break;
        int e2 = err + err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantSpan) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// One horizontal run, both ends inclusive, clipped to the framebuffer. Every
// shape below is drawn as a stack of these, so clipping lives in one place and
// a shape whose coordinates are millions of pixels off screen costs nothing
// beyond the rows the framebuffer actually has.
static void __nexa_gfx_span(long long y, long long xa, long long xb,
                            unsigned char R, unsigned char G, unsigned char B) {
    if (!__nexa_g.fb) return;
    if (y < 0 || y >= (long long)__nexa_g.h) return;
    if (xa < 0) xa = 0;
    if (xb > (long long)__nexa_g.w - 1) xb = (long long)__nexa_g.w - 1;
    if (xa > xb) return;
    long long row = y * (long long)__nexa_g.w;
    for (long long x = xa; x <= xb; x++) __nexa_gfx_draw((int)((row + x) * 4), R, G, B);
}

// Rows of the framebuffer a shape spanning [ya, yb] can actually touch.
static void __nexa_gfx_row_range(long long ya, long long yb, long long* first, long long* last) {
    if (ya < 0) ya = 0;
    if (yb > (long long)__nexa_g.h - 1) yb = (long long)__nexa_g.h - 1;
    *first = ya;
    *last = yb;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Rectangle outline: the border of the pixels gfx.fill would have filled, so
// gfx.rect and gfx.fill agree on which pixels a rectangle covers. Negative
// w/h flip the same way, and the arithmetic is done in 64 bits because
// x + w is allowed to run past INT_MAX (it is clipped, not drawn).
static void __nexa_gfx_rect(int x, int y, int w, int h, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    long long x0 = x, y0 = y, ww = w, hh = h;
    if (ww < 0) { x0 += ww; ww = -ww; }
    if (hh < 0) { y0 += hh; hh = -hh; }
    if (ww < 1 || hh < 1) return;
    long long x1 = x0 + ww - 1;
    long long y1 = y0 + hh - 1;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    __nexa_gfx_span(y0, x0, x1, R, G, B);
    if (y1 != y0) __nexa_gfx_span(y1, x0, x1, R, G, B);
    long long first, last;
    __nexa_gfx_row_range(y0 + 1, y1 - 1, &first, &last);
    for (long long yy = first; yy <= last; yy++) {
        __nexa_gfx_span(yy, x0, x0, R, G, B);
        if (x1 != x0) __nexa_gfx_span(yy, x1, x1, R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantEllipse) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Half width of the ellipse (rx, ry) on the row dy away from its centre, or -1
// when that row misses the ellipse entirely. Rows are sampled at their centre,
// so a radius of 0 is one pixel and rx == ry gives a circle.
static long long __nexa_gfx_ellipse_halfwidth(long long dy, long long rx, long long ry) {
    if (rx < 0 || ry < 0) return -1;
    if (dy < 0) dy = -dy;
    if (dy > ry) return -1;
    if (ry == 0) return rx;
    double t = 1.0 - ((double)dy * (double)dy) / ((double)ry * (double)ry);
    if (t < 0.0) t = 0.0;
    return (long long)((double)rx * std::sqrt(t));
}

// Radii are clamped well above any drawable size (the framebuffer itself tops
// out at 4096) purely so the double arithmetic above stays exact.
static long long __nexa_gfx_radius(int v) {
    if (v < 0) return -1;
    return v > 1048576 ? 1048576 : (long long)v;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_fill_ellipse(int cx, int cy, int rx, int ry, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    long long RX = __nexa_gfx_radius(rx);
    long long RY = __nexa_gfx_radius(ry);
    if (RX < 0 || RY < 0) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range((long long)cy - RY, (long long)cy + RY, &first, &last);
    for (long long y = first; y <= last; y++) {
        long long hw = __nexa_gfx_ellipse_halfwidth(y - (long long)cy, RX, RY);
        if (hw < 0) continue;
        __nexa_gfx_span(y, (long long)cx - hw, (long long)cx + hw, R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantEllipseRuns) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// The ellipse outline on one row, as the one or two runs it covers there.
// Outline means the pixels of the filled ellipse that the two neighbouring
// rows do not both cover: where the curve is flat (top and bottom) that is a
// single long run, where it is steep it is the two end pixels, which is what
// keeps the ring connected without a second rasterization pass. Returns how
// many of xa[]/xb[] were filled in: 0 when the row misses the ellipse.
static int __nexa_gfx_ellipse_runs(long long dy, long long RX, long long RY, long long cx,
                                   long long* xa, long long* xb) {
    long long hw = __nexa_gfx_ellipse_halfwidth(dy, RX, RY);
    if (hw < 0) return 0;
    long long up = __nexa_gfx_ellipse_halfwidth(dy - 1, RX, RY);
    long long down = __nexa_gfx_ellipse_halfwidth(dy + 1, RX, RY);
    long long inner = up < down ? up : down;
    if (inner > hw - 1) inner = hw - 1;
    if (inner < 0) {
        xa[0] = cx - hw;
        xb[0] = cx + hw;
        return 1;
    }
    xa[0] = cx - hw;
    xb[0] = cx - inner - 1;
    xa[1] = cx + inner + 1;
    xb[1] = cx + hw;
    return 2;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesOutline) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_ellipse(int cx, int cy, int rx, int ry, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    long long RX = __nexa_gfx_radius(rx);
    long long RY = __nexa_gfx_radius(ry);
    if (RX < 0 || RY < 0) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range((long long)cy - RY, (long long)cy + RY, &first, &last);
    for (long long y = first; y <= last; y++) {
        long long xa[2], xb[2];
        int n = __nexa_gfx_ellipse_runs(y - (long long)cy, RX, RY, (long long)cx, xa, xb);
        for (int i = 0; i < n; i++) __nexa_gfx_span(y, xa[i], xb[i], R, G, B);
    }
}

static void __nexa_gfx_circle(int cx, int cy, int rad, int r, int g, int b) {
    __nexa_gfx_ellipse(cx, cy, rad, rad, r, g, b);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_fill_circle(int cx, int cy, int rad, int r, int g, int b) {
    __nexa_gfx_fill_ellipse(cx, cy, rad, rad, r, g, b);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantCosSin) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// cos and sin of an angle in degrees, exact on the quarter turns. std::cos
// does not return a zero for a right angle, and the error is not harmless
// here: a sweep ending at 90 degrees would miss the pixel that sits at exactly
// 90 degrees, and a sprite turned a quarter of a turn would land near the
// pixel grid rather than on it.
static void __nexa_gfx_cos_sin(double deg, double* c, double* s) {
    double a = deg - std::floor(deg / 360.0) * 360.0;
    // Written as a positive test so a NaN angle -- or one so large that the
    // subtraction above lost every digit of it -- comes out as no turn at all.
    if (!(a >= 0.0 && a < 360.0)) a = 0.0;
    long long q = (long long)(a / 90.0);
    if (a == (double)(q * 90)) {
        static const double QC[4] = {1.0, 0.0, -1.0, 0.0};
        static const double QS[4] = {0.0, 1.0, 0.0, -1.0};
        *c = QC[q & 3];
        *s = QS[q & 3];
        return;
    }
    double rd = a * (3.14159265358979323846 / 180.0);
    *c = std::cos(rd);
    *s = std::sin(rd);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantSector) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// A sweep of angles, in degrees, 0 straight up and increasing clockwise -- the
// way a clock hand turns, which on a y-down framebuffer is also the direction
// the standard rotation matrix turns. The sweep runs from a0 to a1; a full
// turn or more is the whole circle, and a sweep that does not advance is
// nothing at all.
struct __nexa_GfxSector {
    int mode;      // 0 nothing, 1 the whole circle, 2 up to half a turn, 3 more
    double ax;     // the a0 ray
    double ay;
    double bx;     // the a1 ray
    double by;
};

static __nexa_GfxSector __nexa_gfx_sector(double a0, double a1) {
    __nexa_GfxSector s;
    s.mode = 0;
    s.ax = 0.0;
    s.ay = 0.0;
    s.bx = 0.0;
    s.by = 0.0;
    double span = a1 - a0;
    // Written as a positive test so that a NaN angle is the empty sweep rather
    // than a shape nobody can predict.
    if (!(span > 0.0)) return s;
    if (span >= 360.0) {
        s.mode = 1;
        return s;
    }
    // Up is (0, -1) on a y-down framebuffer, so the ray at a0 degrees
    // clockwise of up is (sin a0, -cos a0).
    double c, n;
    __nexa_gfx_cos_sin(a0, &c, &n);
    s.ax = n;
    s.ay = -c;
    __nexa_gfx_cos_sin(a1, &c, &n);
    s.bx = n;
    s.by = -c;
    s.mode = span <= 180.0 ? 2 : 3;
    return s;
}

// Does the direction (dx, dy) out of the centre lie in the sweep? Two cross
// products rather than an atan2 per pixel: cross(u, v) is positive exactly
// when v is clockwise of u by less than half a turn, so a sweep of up to half
// a turn is "clockwise of a0 and not past a1". A wider sweep is tested as the
// one it leaves out, which is narrower than half a turn by the same amount.
// The centre itself belongs to every sweep, which is what makes a pie a solid
// wedge rather than one with a pinhole at its point.
static int __nexa_gfx_in_sector(const __nexa_GfxSector& s, long long dx, long long dy) {
    if (s.mode == 1) return 1;
    if (s.mode == 0) return 0;
    double px = (double)dx;
    double py = (double)dy;
    double fa = s.ax * py - s.ay * px;   // >= 0: at or clockwise of the a0 ray
    double fb = px * s.by - py * s.bx;   // >= 0: at or anticlockwise of the a1 ray
    if (s.mode == 2) return (fa >= 0.0 && fb >= 0.0) ? 1 : 0;
    return (fa < 0.0 && fb < 0.0) ? 0 : 1;
}

// One horizontal run, both ends inclusive and clipped exactly as
// __nexa_gfx_span clips it, keeping only the pixels whose direction out of
// (cx, cy) lies in the sweep.
static void __nexa_gfx_span_sector(long long y, long long xa, long long xb,
                                   long long cx, long long cy, const __nexa_GfxSector& s,
                                   unsigned char R, unsigned char G, unsigned char B) {
    if (!__nexa_g.fb) return;
    if (y < 0 || y >= (long long)__nexa_g.h) return;
    if (xa < 0) xa = 0;
    if (xb > (long long)__nexa_g.w - 1) xb = (long long)__nexa_g.w - 1;
    if (xa > xb) return;
    long long row = y * (long long)__nexa_g.w;
    for (long long x = xa; x <= xb; x++) {
        if (!__nexa_gfx_in_sector(s, x - cx, y - cy)) continue;
        __nexa_gfx_draw((int)((row + x) * 4), R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.arc) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// An arc is the circle outline gfx.circle draws with everything outside the
// sweep left off, so an arc and a circle of the same radius agree pixel for
// pixel wherever they overlap, and a sweep of a full turn is the circle.
static void __nexa_gfx_arc(int cx, int cy, int rad, double a0, double a1, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    __nexa_GfxSector s = __nexa_gfx_sector(a0, a1);
    if (!s.mode) return;
    long long RD = __nexa_gfx_radius(rad);
    if (RD < 0) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range((long long)cy - RD, (long long)cy + RD, &first, &last);
    for (long long y = first; y <= last; y++) {
        long long xa[2], xb[2];
        int n = __nexa_gfx_ellipse_runs(y - (long long)cy, RD, RD, (long long)cx, xa, xb);
        for (int i = 0; i < n; i++) {
            __nexa_gfx_span_sector(y, xa[i], xb[i], (long long)cx, (long long)cy, s, R, G, B);
        }
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.pie) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// And a pie is the filled circle with the same cut: the wedge from the centre
// out to the arc, every pixel of it one gfx.fill_circle would have drawn.
static void __nexa_gfx_pie(int cx, int cy, int rad, double a0, double a1, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    __nexa_GfxSector s = __nexa_gfx_sector(a0, a1);
    if (!s.mode) return;
    long long RD = __nexa_gfx_radius(rad);
    if (RD < 0) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range((long long)cy - RD, (long long)cy + RD, &first, &last);
    for (long long y = first; y <= last; y++) {
        long long hw = __nexa_gfx_ellipse_halfwidth(y - (long long)cy, RD, RD);
        if (hw < 0) continue;
        __nexa_gfx_span_sector(y, (long long)cx - hw, (long long)cx + hw,
                               (long long)cx, (long long)cy, s, R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantRound) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// The pixel box of a rounded rectangle, with the corner radius clamped.
// Negative w/h flip exactly as gfx.fill's do. The radius is clamped to
// (shorter side - 1) / 2: a corner of radius rad eats rad pixels off the edge
// row, so two of them meeting across a side would leave that row with nothing
// at all, and half of an even side is one pixel past where they meet. On a
// square of odd side at the largest radius the shape is precisely the circle
// gfx.fill_circle draws.
static int __nexa_gfx_round_box(int x, int y, int w, int h, int rad,
                                long long* x0, long long* y0,
                                long long* x1, long long* y1, long long* rr) {
    long long X = x, Y = y, W = w, H = h;
    if (W < 0) { X += W; W = -W; }
    if (H < 0) { Y += H; H = -H; }
    if (W < 1 || H < 1) return 0;
    long long R = rad < 0 ? 0 : (long long)rad;
    long long half = ((W < H ? W : H) - 1) / 2;
    if (R > half) R = half;
    *x0 = X;
    *y0 = Y;
    *x1 = X + W - 1;
    *y1 = Y + H - 1;
    *rr = R;
    return 1;
}

// The run [xa, xb] the rounded rectangle covers on row y, or 0 when the row
// misses it. Away from the corners that is the whole width; within rad of the
// top or bottom it is the width less what the corner circle of that radius
// cuts off, which is the same halfwidth gfx.fill_circle uses -- so a corner
// and a circle of equal radius curve identically. A radius of 0 never leaves
// the first branch, which is why gfx.round_rect at rad 0 is gfx.rect exactly.
static int __nexa_gfx_round_row(long long y, long long x0, long long y0,
                                long long x1, long long y1, long long rad,
                                long long* xa, long long* xb) {
    if (y < y0 || y > y1) return 0;
    long long dy;
    if (y < y0 + rad) {
        dy = y - (y0 + rad);
    } else if (y > y1 - rad) {
        dy = y - (y1 - rad);
    } else {
        *xa = x0;
        *xb = x1;
        return 1;
    }
    long long hw = __nexa_gfx_ellipse_halfwidth(dy, rad, rad);
    if (hw < 0) return 0;
    *xa = x0 + rad - hw;
    *xb = x1 - rad + hw;
    return 1;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.fillRoundRect) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_fill_round_rect(int x, int y, int w, int h, int rad, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    long long x0, y0, x1, y1, rr;
    if (!__nexa_gfx_round_box(x, y, w, h, rad, &x0, &y0, &x1, &y1, &rr)) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range(y0, y1, &first, &last);
    for (long long yy = first; yy <= last; yy++) {
        long long xa, xb;
        if (!__nexa_gfx_round_row(yy, x0, y0, x1, y1, rr, &xa, &xb)) continue;
        __nexa_gfx_span(yy, xa, xb, R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.roundRect) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Outline: the border of the pixels gfx.fill_round_rect would have filled, the
// same relationship gfx.rect has to gfx.fill. A pixel is on the border when a
// neighbour is not inside, which on a row-by-row rasterizer is its two ends
// plus whatever the row above or the row below leaves uncovered. A row the
// shape does not reach at all leaves everything uncovered, which is how the
// top and bottom rows come out solid.
static void __nexa_gfx_round_rect(int x, int y, int w, int h, int rad, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    long long x0, y0, x1, y1, rr;
    if (!__nexa_gfx_round_box(x, y, w, h, rad, &x0, &y0, &x1, &y1, &rr)) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    long long first, last;
    __nexa_gfx_row_range(y0, y1, &first, &last);
    for (long long yy = first; yy <= last; yy++) {
        long long xa, xb;
        if (!__nexa_gfx_round_row(yy, x0, y0, x1, y1, rr, &xa, &xb)) continue;
        long long ua, ub, da, db;
        if (!__nexa_gfx_round_row(yy - 1, x0, y0, x1, y1, rr, &ua, &ub)) {
            ua = xb + 1;
            ub = xa - 1;
        }
        if (!__nexa_gfx_round_row(yy + 1, x0, y0, x1, y1, rr, &da, &db)) {
            da = xb + 1;
            db = xa - 1;
        }
        __nexa_gfx_span(yy, xa, xa, R, G, B);
        __nexa_gfx_span(yy, xb, xb, R, G, B);
        __nexa_gfx_span(yy, xa, (ua > da ? ua : da) - 1, R, G, B);
        __nexa_gfx_span(yy, (ub < db ? ub : db) + 1, xb, R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Even-odd scanline fill. A pixel belongs to the polygon when its centre does,
// with the same half-open rule gfx.fill uses: an edge exactly on the left or
// top boundary is inside, one exactly on the right or bottom boundary is not.
// Two polygons sharing an edge therefore tile it without a seam or an overlap.
static void __nexa_gfx_fill_poly_pts(const int* xs, const int* ys, int n,
                                     unsigned char R, unsigned char G, unsigned char B) {
    if (!__nexa_g.fb || n < 3) return;
    long long ymin = ys[0];
    long long ymax = ys[0];
    for (int i = 1; i < n; i++) {
        if ((long long)ys[i] < ymin) ymin = ys[i];
        if ((long long)ys[i] > ymax) ymax = ys[i];
    }
    long long first, last;
    __nexa_gfx_row_range(ymin, ymax, &first, &last);
    std::vector<double> hits;
    for (long long y = first; y <= last; y++) {
        hits.clear();
        for (int i = 0; i < n; i++) {
            int j = i + 1 == n ? 0 : i + 1;
            long long ya = ys[i];
            long long yb = ys[j];
            if ((ya <= y) == (yb <= y)) continue;
            double t = (double)(y - ya) / (double)(yb - ya);
            hits.push_back((double)xs[i] + t * ((double)xs[j] - (double)xs[i]));
        }
        std::sort(hits.begin(), hits.end());
        for (size_t k = 0; k + 1 < hits.size(); k += 2) {
            long long xa = (long long)std::ceil(hits[k]);
            long long xb = (long long)std::ceil(hits[k + 1]) - 1;
            __nexa_gfx_span(y, xa, xb, R, G, B);
        }
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesOutline) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_poly_pts(const int* xs, const int* ys, int n, int r, int g, int b) {
    if (!__nexa_g.fb || n < 3) return;
    for (int i = 0; i < n; i++) {
        int j = i + 1 == n ? 0 : i + 1;
        __nexa_gfx_line(xs[i], ys[i], xs[j], ys[j], r, g, b);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesOutline) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_tri(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b) {
    int xs[3] = {x1, x2, x3};
    int ys[3] = {y1, y2, y3};
    __nexa_gfx_poly_pts(xs, ys, 3, r, g, b);
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
static void __nexa_gfx_fill_tri(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    int xs[3] = {x1, x2, x3};
    int ys[3] = {y1, y2, y3};
    __nexa_gfx_fill_poly_pts(xs, ys, 3, __nexa_gfx_u8(r), __nexa_gfx_u8(g), __nexa_gfx_u8(b));
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (wantPolyTake) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// A Nexa []int is a std::vector of whatever integer type held the literals, so
// the points arrive through a template and are narrowed here, once.
template <class TX, class TY>
static int __nexa_gfx_poly_take(const TX& xs, const TY& ys, std::vector<int>& px, std::vector<int>& py) {
    if (!__nexa_g.fb) return 0;
    if (xs.size() != ys.size() || xs.size() < 3) return 0;
    px.reserve(xs.size());
    py.reserve(ys.size());
    for (size_t i = 0; i < xs.size(); i++) {
        long long vx = (long long)xs[i];
        long long vy = (long long)ys[i];
        if (vx < -2147483647ll) vx = -2147483647ll;
        if (vx > 2147483647ll) vx = 2147483647ll;
        if (vy < -2147483647ll) vy = -2147483647ll;
        if (vy > 2147483647ll) vy = 2147483647ll;
        px.push_back((int)vx);
        py.push_back((int)vy);
    }
    return 1;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesOutline) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
template <class TX, class TY>
static int __nexa_gfx_poly(const TX& xs, const TY& ys, int r, int g, int b) {
    std::vector<int> px, py;
    if (!__nexa_gfx_poly_take(xs, ys, px, py)) return 0;
    __nexa_gfx_poly_pts(px.data(), py.data(), (int)px.size(), r, g, b);
    return 1;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.shapesFill) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
template <class TX, class TY>
static int __nexa_gfx_fill_poly(const TX& xs, const TY& ys, int r, int g, int b) {
    std::vector<int> px, py;
    if (!__nexa_gfx_poly_take(xs, ys, px, py)) return 0;
    __nexa_gfx_fill_poly_pts(px.data(), py.data(), (int)px.size(),
                             __nexa_gfx_u8(r), __nexa_gfx_u8(g), __nexa_gfx_u8(b));
    return 1;
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.lineThick) out += /* [nexa:rasterizers-begin] */ R"NEXA_GFX(
// Thick line: the pixels whose centre lies within t/2 of the segment, i.e. a
// capsule with round caps. A capsule is convex, so each row meets it in one
// run, and that run is the widest of the two end discs and the body quad.
// Thickness is measured across the line at every angle, which a stack of
// offset Bresenham lines does not give you on a diagonal.
static void __nexa_gfx_line_thick(int x0, int y0, int x1, int y1, int r, int g, int b, int t) {
    if (!__nexa_g.fb) return;
    if (t <= 1) {
        __nexa_gfx_line(x0, y0, x1, y1, r, g, b);
        return;
    }
    if (t > 4096) t = 4096;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    double rad = (double)t * 0.5;
    double ax = (double)x0, ay = (double)y0;
    double bx = (double)x1, by = (double)y1;
    double dx = bx - ax, dy = by - ay;
    double len = std::sqrt(dx * dx + dy * dy);
    double qx[4] = {0, 0, 0, 0};
    double qy[4] = {0, 0, 0, 0};
    if (len > 0.0) {
        double nx = -dy / len * rad;
        double ny = dx / len * rad;
        qx[0] = ax + nx; qy[0] = ay + ny;
        qx[1] = bx + nx; qy[1] = by + ny;
        qx[2] = bx - nx; qy[2] = by - ny;
        qx[3] = ax - nx; qy[3] = ay - ny;
    }
    long long ylo = (long long)std::floor((ay < by ? ay : by) - rad);
    long long yhi = (long long)std::ceil((ay > by ? ay : by) + rad);
    long long first, last;
    __nexa_gfx_row_range(ylo, yhi, &first, &last);
    for (long long y = first; y <= last; y++) {
        double lo = 0.0, hi = 0.0;
        int have = 0;
        double ex[2] = {ax, bx};
        double ey[2] = {ay, by};
        for (int i = 0; i < 2; i++) {
            double d = rad * rad - ((double)y - ey[i]) * ((double)y - ey[i]);
            if (d < 0.0) continue;
            double s = std::sqrt(d);
            if (!have || ex[i] - s < lo) lo = ex[i] - s;
            if (!have || ex[i] + s > hi) hi = ex[i] + s;
            have = 1;
        }
        if (len > 0.0) {
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) & 3;
                if ((qy[i] <= (double)y) == (qy[j] <= (double)y)) continue;
                double f = ((double)y - qy[i]) / (qy[j] - qy[i]);
                double x = qx[i] + f * (qx[j] - qx[i]);
                if (!have || x < lo) lo = x;
                if (!have || x > hi) hi = x;
                have = 1;
            }
        }
        if (!have) continue;
        __nexa_gfx_span(y, (long long)std::ceil(lo), (long long)std::floor(hi), R, G, B);
    }
}
)NEXA_GFX";  // [nexa:rasterizers-end]
    if (need.text) out += R"NEXA_GFX(
// 5x7, columns left-to-right, bit 0 = top. Printable ASCII 32..126.
static const unsigned char __nexa_gfx_font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00}, {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00},
    {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33}, {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},
    {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00},
    {0x00,0x40,0x34,0x00,0x00}, {0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06}, {0x3E,0x41,0x5D,0x59,0x4E},
    {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x41,0x51,0x73}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x26,0x49,0x49,0x49,0x32}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x41,0x7F}, {0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40}, {0x00,0x03,0x07,0x08,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x18,0xA4,0xA4,0xA4,0x7C},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x40,0x80,0x84,0x7D,0x00},
    {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x78,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0xFC,0x18,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x18,0xFC}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x24},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x1C,0xA0,0xA0,0xA0,0x7C},
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x77,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08}
};

static void __nexa_gfx_glyph(int x, int y, int ch, unsigned char R, unsigned char G, unsigned char B, int scale) {
    if (ch < 32 || ch > 126) ch = '?';
    if (scale < 1) scale = 1;
    const unsigned char* col = __nexa_gfx_font5x7[ch - 32];
    for (int i = 0; i < 5; i++) {
        unsigned char bits = col[i];
        for (int j = 0; j < 8; j++) {
            if (!(bits & (1u << j))) continue;
            for (int sy = 0; sy < scale; sy++) {
                int py = y + j * scale + sy;
                if (py < 0 || py >= __nexa_g.h) continue;
                int row = py * __nexa_g.w;
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + i * scale + sx;
                    if (px < 0 || px >= __nexa_g.w) continue;
                    __nexa_gfx_draw((row + px) * 4, R, G, B);
                }
            }
        }
    }
}

static int __nexa_gfx_text_scale() {
    int s = __nexa_g.text_scale;
    if (s < 1) s = 1;
    if (s > 64) s = 64;
    return s;
}

static int __nexa_gfx_text_size_set(int n) {
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    __nexa_g.text_scale = n;
    return n;
}

static int __nexa_gfx_clamp_text_scale(int scale) {
    if (scale < 1) scale = __nexa_gfx_text_scale();
    if (scale > 64) scale = 64;
    return scale;
}

static void __nexa_gfx_text_dims(const std::string& s, int scale, int* out_w, int* out_h) {
    scale = __nexa_gfx_clamp_text_scale(scale);
    const int adv = 6 * scale;
    const int lh = 8 * scale;
    const int tab = 24 * scale;
    if (s.empty()) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }
    int cx = 0;
    int maxw = 0;
    int lines = 1;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            if (cx > maxw) maxw = cx;
            cx = 0;
            lines++;
            continue;
        }
        if (ch == '\t') {
            int next = ((cx / tab) + 1) * tab;
            cx = next;
            continue;
        }
        cx += adv;
    }
    if (cx > maxw) maxw = cx;
    if (out_w) *out_w = maxw;
    if (out_h) *out_h = lines * lh;
}

static int __nexa_gfx_text_width(const std::string& s, int scale) {
    int w = 0, h = 0;
    __nexa_gfx_text_dims(s, scale, &w, &h);
    return w;
}

static int __nexa_gfx_text_height(const std::string& s, int scale) {
    int w = 0, h = 0;
    __nexa_gfx_text_dims(s, scale, &w, &h);
    return h;
}

static int __nexa_gfx_text(int x, int y, const std::string& s, int r, int g, int b, int scale) {
    if (!__nexa_g.fb) return 0;
    if (scale < 1) scale = __nexa_gfx_text_scale();
    if (scale > 64) scale = 64;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    int cx = x;
    int cy = y;
    int maxw = 0;
    const int adv = 6 * scale;
    const int lh = 8 * scale;
    const int tab = 24 * scale;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            int w = cx - x;
            if (w > maxw) maxw = w;
            cx = x;
            cy += lh;
            continue;
        }
        if (ch == '\t') {
            int cell = cx - x;
            if (cell < 0) cell = 0;
            int next = ((cell / tab) + 1) * tab;
            cx = x + next;
            continue;
        }
        __nexa_gfx_glyph(cx, cy, (int)ch, R, G, B, scale);
        cx += adv;
    }
    int w = cx - x;
    if (w > maxw) maxw = w;
    return maxw;
}
)NEXA_GFX";
    out += R"NEXA_GFX(
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
static void __nexa_gfx_x11_present() {
    if (!__nexa_g.dpy || !__nexa_g.win || !__nexa_g.fb) return;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(__nexa_g.dpy, __nexa_g.win, &wa)) return;
    int dw = wa.width > 0 ? wa.width : (__nexa_g.w * __nexa_g.scale);
    int dh = wa.height > 0 ? wa.height : (__nexa_g.h * __nexa_g.scale);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > 8192) dw = 8192;
    if (dh > 8192) dh = 8192;
    if (!__nexa_g.xbuf || __nexa_g.xbw != dw || __nexa_g.xbh != dh) {
        if (__nexa_g.img) {
            __nexa_g.img->data = nullptr;
            XDestroyImage(__nexa_g.img);
            __nexa_g.img = nullptr;
        }
        delete[] __nexa_g.xbuf;
        __nexa_g.xbuf = new unsigned char[(size_t)dw * (size_t)dh * 4];
        __nexa_g.xbw = dw;
        __nexa_g.xbh = dh;
        int scr = DefaultScreen(__nexa_g.dpy);
        int depth = DefaultDepth(__nexa_g.dpy, scr);
        __nexa_g.img = XCreateImage(__nexa_g.dpy, __nexa_g.vis ? __nexa_g.vis : DefaultVisual(__nexa_g.dpy, scr),
            (unsigned)depth, ZPixmap, 0, (char*)__nexa_g.xbuf, (unsigned)dw, (unsigned)dh, 32, 0);
        if (!__nexa_g.img) return;
    }
    const int sw = __nexa_g.w;
    const int sh = __nexa_g.h;
    const int lsb = (__nexa_g.img->byte_order != MSBFirst);
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            const unsigned char* s = __nexa_g.fb + (size_t)(sy * sw + sx) * 4;
            unsigned char* d = __nexa_g.xbuf + (size_t)(y * dw + x) * 4;
            if (lsb) {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = 255;
            } else {
                d[0] = 255;
                d[1] = s[0];
                d[2] = s[1];
                d[3] = s[2];
            }
        }
    }
    XPutImage(__nexa_g.dpy, __nexa_g.win, __nexa_g.gc, __nexa_g.img, 0, 0, 0, 0,
        (unsigned)dw, (unsigned)dh);
    XFlush(__nexa_g.dpy);
}
#endif

)NEXA_GFX";
    if (need.maxfps) out += R"NEXA_GFX(
// --- frame limiter ----------------------------------------------------------
// An uncapped loop that draws a few shapes runs at thousands of frames a
// second: it spends the whole machine on redraws, and because a key that goes
// down and up between two of those frames was never seen down, input starts
// going missing. gfx.maxfps(n) hands each frame 1/n of a second and present()
// waits out whatever the drawing did not use.
static long long __nexa_gfx_now_ns() {
#ifdef __EMSCRIPTEN__
    return (long long)(emscripten_get_now() * 1000000.0);
#elif defined(_WIN32)
    LARGE_INTEGER freq;
    LARGE_INTEGER now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    if (freq.QuadPart <= 0) return 0;
    // Whole seconds first, then the remainder: the counter ticks fast enough
    // that scaling it to nanoseconds in one multiply overflows 64 bits within a
    // day of uptime, and a frame limiter built on a clock that wraps is worse
    // than no limiter at all.
    long long secs = (long long)(now.QuadPart / freq.QuadPart);
    long long rest = (long long)(now.QuadPart % freq.QuadPart);
    return secs * 1000000000LL + rest * 1000000000LL / (long long)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
#endif
}

static void __nexa_gfx_maxfps(int fps) {
    if (fps <= 0) {
        // Not a rejected argument: "no cap" is a thing to ask for, and it is
        // also where every program starts.
        __nexa_g.fps_period = 0;
        return;
    }
    __nexa_g.fps_period = 1000000000LL / fps;
    // 0 is "nothing is due yet", so the next present() starts the schedule from
    // itself rather than from whenever some earlier cap was set.
    __nexa_g.fps_due = 0;
#ifdef _WIN32
    // Sleep() rounds up to the scheduler tick, 15.6 ms by default, which would
    // turn a 60 fps cap into 64 ms frames. Asking for a 1 ms tick is a
    // process-wide setting, so it is asked for once and left in place.
    static int __nexa_gfx_tick_set = 0;
    if (!__nexa_gfx_tick_set) {
        __nexa_gfx_tick_set = 1;
        timeBeginPeriod(1);
    }
#endif
}

static void __nexa_gfx_pace() {
    if (__nexa_g.fps_period <= 0) return;
    long long now = __nexa_gfx_now_ns();
    long long due = __nexa_g.fps_due;
    // The first frame under a cap, and any frame that overran its budget by a
    // whole period, start the count again from now. Catching up instead would
    // fire a burst of frames with no wait at all, which is the stutter the cap
    // was there to stop.
    if (due == 0 || now > due + __nexa_g.fps_period) due = now;
    long long left = due - now;
    if (left > 0) {
#ifdef __EMSCRIPTEN__
        // Rounded up to the millisecond the browser deals in; whatever it
        // actually gives back, the deadline below is measured, not assumed.
        emscripten_sleep((unsigned int)((left + 999999LL) / 1000000LL));
#elif defined(_WIN32)
        // Even on a 1 ms tick Sleep is only good to about a millisecond, and a
        // limiter that overshoots its frame is worse than one that burns a few
        // microseconds, so the last millisecond is spun out instead.
        long long ms = (left - 1000000LL) / 1000000LL;
        if (ms > 0) Sleep((DWORD)ms);
        while (__nexa_gfx_now_ns() < due) {}
#else
        struct timespec ts;
        struct timespec rem;
        ts.tv_sec = (time_t)(left / 1000000000LL);
        ts.tv_nsec = (long)(left % 1000000000LL);
        // A signal cuts a sleep short and leaves the rest of it in rem; the
        // frame is not over until the deadline is.
        while (nanosleep(&ts, &rem) != 0 && errno == EINTR) ts = rem;
#endif
    }
    __nexa_g.fps_due = due + __nexa_g.fps_period;
}
)NEXA_GFX";
    out += R"NEXA_GFX(
static void __nexa_gfx_present() {
    if (!__nexa_g.ready || !__nexa_g.fb) return;
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var c = Module["canvas"] || document.getElementById("canvas");
        if (!c) return;
        var ww = $0;
        var hh = $1;
        if (c.width !== ww) c.width = ww;
        if (c.height !== hh) c.height = hh;
        var ctx = c.getContext("2d");
        var img = ctx.createImageData(ww, hh);
        var src = HEAPU8.subarray($2, $2 + ww * hh * 4);
        img.data.set(src);
        ctx.putImageData(img, 0, 0);
    }), __nexa_g.w, __nexa_g.h, (int)(uintptr_t)__nexa_g.fb);
    emscripten_sleep(0);
#elif defined(_WIN32)
    if (__nexa_g.hwnd && !IsIconic(__nexa_g.hwnd)) {
        RECT rc;
        GetClientRect(__nexa_g.hwnd, &rc);
        HDC hdc = GetDC(__nexa_g.hwnd);
        if (hdc) {
            __nexa_gfx_flip(hdc, rc.right, rc.bottom);
            ReleaseDC(__nexa_g.hwnd, hdc);
        }
    }
    __nexa_gfx_poll();
#elif defined(__APPLE__)
    if (__nexa_gfx_nsview) [__nexa_gfx_nsview setNeedsDisplay:YES];
    if (__nexa_gfx_nswin) [__nexa_gfx_nswin displayIfNeeded];
    __nexa_gfx_poll();
#elif defined(__linux__)
    __nexa_gfx_x11_present();
    __nexa_gfx_poll();
#endif
)NEXA_GFX";
    // Last thing in the frame, after the pixels are out and the events are in.
    // Nothing is read during the wait, so whatever the user does lands in the
    // OS queue and is there for the next frame's poll to pick up.
    if (need.maxfps) out += "    __nexa_gfx_pace();\n";
    out += R"NEXA_GFX(}
)NEXA_GFX";
    if (need.keys) out += R"NEXA_GFX(
#ifdef __APPLE__
static int __nexa_gfx_mac_held(unsigned short kc) {
    return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kc) ? 1 : 0;
}
#endif
)NEXA_GFX";
    out += R"NEXA_GFX(
static int __nexa_gfx_has_focus() {
#ifdef _WIN32
    return (__nexa_g.hwnd && GetForegroundWindow() == __nexa_g.hwnd) ? 1 : 0;
#elif defined(__APPLE__)
    return (__nexa_gfx_nswin && [__nexa_gfx_nswin isKeyWindow]) ? 1 : 0;
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    Window focused = 0;
    int revert = 0;
    XGetInputFocus(__nexa_g.dpy, &focused, &revert);
    return (focused == __nexa_g.win) ? 1 : 0;
#elif defined(__EMSCRIPTEN__)
    return EM_ASM_INT(({ return document.hasFocus() ? 1 : 0; }));
#else
    return 1;
#endif
}
)NEXA_GFX";
    if (need.keys) out += R"NEXA_GFX(
static int __nexa_gfx_vk(const std::string& name) {
    if (name.empty()) return 0;
    if (!__nexa_gfx_has_focus()) return 0;
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#ifdef _WIN32
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return GetAsyncKeyState((int)c) & 0x8000 ? 1 : 0;
        if (c >= 'a' && c <= 'z') return GetAsyncKeyState((int)(c - 'a' + 'A')) & 0x8000 ? 1 : 0;
    }
    if (s == "escape") return GetAsyncKeyState(VK_ESCAPE) & 0x8000 ? 1 : 0;
    if (s == "space") return GetAsyncKeyState(VK_SPACE) & 0x8000 ? 1 : 0;
    if (s == "enter" || s == "return") return GetAsyncKeyState(VK_RETURN) & 0x8000 ? 1 : 0;
    if (s == "up") return GetAsyncKeyState(VK_UP) & 0x8000 ? 1 : 0;
    if (s == "down") return GetAsyncKeyState(VK_DOWN) & 0x8000 ? 1 : 0;
    if (s == "left") return GetAsyncKeyState(VK_LEFT) & 0x8000 ? 1 : 0;
    if (s == "right") return GetAsyncKeyState(VK_RIGHT) & 0x8000 ? 1 : 0;
    if (s == "shift") return GetAsyncKeyState(VK_SHIFT) & 0x8000 ? 1 : 0;
    if (s == "ctrl" || s == "control") return GetAsyncKeyState(VK_CONTROL) & 0x8000 ? 1 : 0;
    if (s == "alt") return GetAsyncKeyState(VK_MENU) & 0x8000 ? 1 : 0;
    if (s == "tab") return GetAsyncKeyState(VK_TAB) & 0x8000 ? 1 : 0;
    if (s == "backspace" || s == "bksp") return GetAsyncKeyState(VK_BACK) & 0x8000 ? 1 : 0;
    if (s == "delete" || s == "del") return GetAsyncKeyState(VK_DELETE) & 0x8000 ? 1 : 0;
    if (s == "f11") return GetAsyncKeyState(VK_F11) & 0x8000 ? 1 : 0;
#elif defined(__EMSCRIPTEN__)
    auto down = [&](int code) -> int {
        return (code >= 0 && code < 512) ? __nexa_g.keys[code] : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return down(48 + (c - '0'));
        if (c >= 'a' && c <= 'z') return down(65 + (c - 'a'));
    }
    if (s == "escape") return down(27);
    if (s == "space") return down(32);
    if (s == "enter" || s == "return") return down(13);
    if (s == "up") return down(38);
    if (s == "down") return down(40);
    if (s == "left") return down(37);
    if (s == "right") return down(39);
    if (s == "shift") return down(16);
    if (s == "ctrl" || s == "control") return down(17);
    if (s == "alt") return down(18);
    if (s == "tab") return down(9);
    if (s == "backspace" || s == "bksp") return down(8);
    if (s == "delete" || s == "del") return down(46);
    if (s == "f11") return down(122);
#elif defined(__APPLE__)
    static const unsigned char letters[26] = {
        0x00, 0x0B, 0x08, 0x02, 0x0E, 0x03, 0x05, 0x04, 0x22, 0x26,
        0x28, 0x25, 0x2E, 0x2D, 0x1F, 0x23, 0x0C, 0x0F, 0x01, 0x11,
        0x20, 0x09, 0x0D, 0x07, 0x10, 0x06
    };
    static const unsigned char digits[10] = {
        0x1D, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return __nexa_gfx_mac_held(digits[c - '0']);
        if (c >= 'a' && c <= 'z') return __nexa_gfx_mac_held(letters[c - 'a']);
    }
    if (s == "escape") return __nexa_gfx_mac_held(0x35);
    if (s == "space") return __nexa_gfx_mac_held(0x31);
    if (s == "enter" || s == "return") return __nexa_gfx_mac_held(0x24);
    if (s == "up") return __nexa_gfx_mac_held(0x7E);
    if (s == "down") return __nexa_gfx_mac_held(0x7D);
    if (s == "left") return __nexa_gfx_mac_held(0x7B);
    if (s == "right") return __nexa_gfx_mac_held(0x7C);
    if (s == "shift") return (__nexa_gfx_mac_held(0x38) || __nexa_gfx_mac_held(0x3C)) ? 1 : 0;
    if (s == "ctrl" || s == "control") return (__nexa_gfx_mac_held(0x3B) || __nexa_gfx_mac_held(0x3E)) ? 1 : 0;
    if (s == "alt") return (__nexa_gfx_mac_held(0x3A) || __nexa_gfx_mac_held(0x3D)) ? 1 : 0;
    if (s == "tab") return __nexa_gfx_mac_held(0x30);
    if (s == "backspace" || s == "bksp") return __nexa_gfx_mac_held(0x33);
    if (s == "delete" || s == "del") return __nexa_gfx_mac_held(0x75);
    if (s == "f11") return __nexa_gfx_mac_held(0x67);
#elif defined(__linux__)
    if (!__nexa_g.dpy) return 0;
    char keys[32];
    XQueryKeymap(__nexa_g.dpy, keys);
    auto held = [&](KeySym ks) -> int {
        KeyCode kc = XKeysymToKeycode(__nexa_g.dpy, ks);
        if (!kc) return 0;
        return (keys[kc / 8] & (1 << (kc % 8))) ? 1 : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (held((KeySym)c) || held((KeySym)(c - 'a' + 'A'))) return 1;
        return 0;
    }
    if (s == "escape") return held(XK_Escape);
    if (s == "space") return held(XK_space);
    if (s == "enter" || s == "return") return held(XK_Return);
    if (s == "up") return held(XK_Up);
    if (s == "down") return held(XK_Down);
    if (s == "left") return held(XK_Left);
    if (s == "right") return held(XK_Right);
    if (s == "shift") return (held(XK_Shift_L) || held(XK_Shift_R)) ? 1 : 0;
    if (s == "ctrl" || s == "control") return (held(XK_Control_L) || held(XK_Control_R)) ? 1 : 0;
    if (s == "alt") return (held(XK_Alt_L) || held(XK_Alt_R) || held(XK_Meta_L) || held(XK_Meta_R)) ? 1 : 0;
    if (s == "tab") return held(XK_Tab);
    if (s == "backspace" || s == "bksp") return held(XK_BackSpace);
    if (s == "delete" || s == "del") return held(XK_Delete);
    if (s == "f11") return held(XK_F11);
#endif
    return 0;
}

static const char* const __nexa_gfx_key_names[] = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","o","p","q","r","s","t","u","v","w","x","y","z",
    "escape","space","enter","up","down","left","right",
    "shift","ctrl","alt","tab","backspace","delete","f11"
};

static int __nexa_gfx_key_slot(const std::string& name) {
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "return") s = "enter";
    if (s == "control") s = "ctrl";
    if (s == "bksp") s = "backspace";
    if (s == "del") s = "delete";
    const int n = (int)(sizeof(__nexa_gfx_key_names) / sizeof(__nexa_gfx_key_names[0]));
    for (int i = 0; i < n; i++) {
        if (s == __nexa_gfx_key_names[i]) return i;
    }
    return -1;
}

static void __nexa_gfx_key_snapshot() {
    if (!__nexa_g.ready) return;
    const int n = (int)(sizeof(__nexa_gfx_key_names) / sizeof(__nexa_gfx_key_names[0]));
    for (int i = 0; i < n; i++) {
        __nexa_g.k_prev[i] = __nexa_g.k_now[i];
        __nexa_g.k_now[i] = __nexa_gfx_vk(__nexa_gfx_key_names[i]);
    }
}

static int __nexa_gfx_key(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    return __nexa_gfx_vk(name);
}

static int __nexa_gfx_pressed(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    int slot = __nexa_gfx_key_slot(name);
    if (slot < 0) return 0;
    return (__nexa_g.k_now[slot] && !__nexa_g.k_prev[slot]) ? 1 : 0;
}

// The mirror of gfx.pressed, off the same two snapshots gfx.poll() keeps.
static int __nexa_gfx_released(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    int slot = __nexa_gfx_key_slot(name);
    if (slot < 0) return 0;
    return (!__nexa_g.k_now[slot] && __nexa_g.k_prev[slot]) ? 1 : 0;
}
)NEXA_GFX";
    if (need.wheel) out += R"NEXA_GFX(
static int __nexa_gfx_wheel() {
    if (!__nexa_g.ready) return 0;
    return __nexa_g.wheel_y;
}

static int __nexa_gfx_wheel_x() {
    if (!__nexa_g.ready) return 0;
    return __nexa_g.wheel_x;
}
)NEXA_GFX";
    if (need.typed) out += R"NEXA_GFX(
// Consuming, like gfx.drop(): the text belongs to whoever asks for it first.
static std::string __nexa_gfx_typed() {
    if (!__nexa_g.ready) return std::string();
    std::string s;
    s.swap(__nexa_g.type_buf);
    return s;
}
)NEXA_GFX";
    if (need.imageStore) out += R"NEXA_GFX(
// [nexa:imgstore-begin]
struct __nexa_GfxImg {
    int w;
    int h;
    unsigned char* px;
};

static std::vector<__nexa_GfxImg> __nexa_imgs;
static std::vector<std::string> __nexa_img_paths;
// [nexa:imgstore-end]
)NEXA_GFX";
    if (need.imageLoad) out += R"NEXA_GFX(
static int __nexa_gfx_pixels_ok(int w, int h) {
    if (w < 1 || h < 1 || w > 4096 || h > 4096) return 0;
    return 1;
}

#if defined(__linux__) || defined(__EMSCRIPTEN__)
unsigned char* __nexa_gfx_stbi_load_rgba(const unsigned char* p, int n, int* w, int* h);
void __nexa_gfx_stbi_free(void* p);
#endif

#ifdef _WIN32
static void __nexa_gfx_com_once() {
    static int once = 0;
    if (once) return;
    once = 1;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
}

static int __nexa_gfx_wic_decode(const unsigned char* data, int n, int* ow, int* oh, unsigned char** out) {
    if (!data || n < 8 || !ow || !oh || !out) return 0;
    __nexa_gfx_com_once();
    IWICImagingFactory* fac = NULL;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) || !fac) {
        return 0;
    }
    IWICStream* stream = NULL;
    if (FAILED(fac->CreateStream(&stream)) || !stream) {
        fac->Release();
        return 0;
    }
    if (FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)n))) {
        stream->Release();
        fac->Release();
        return 0;
    }
    IWICBitmapDecoder* dec = NULL;
    HRESULT hr = fac->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnLoad, &dec);
    stream->Release();
    if (FAILED(hr) || !dec) {
        fac->Release();
        return 0;
    }
    IWICBitmapFrameDecode* frame = NULL;
    if (FAILED(dec->GetFrame(0, &frame)) || !frame) {
        dec->Release();
        fac->Release();
        return 0;
    }
    IWICFormatConverter* conv = NULL;
    if (FAILED(fac->CreateFormatConverter(&conv)) || !conv) {
        frame->Release();
        dec->Release();
        fac->Release();
        return 0;
    }
    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    UINT w = 0, h = 0;
    if (FAILED(hr) || FAILED(conv->GetSize(&w, &h)) || !__nexa_gfx_pixels_ok((int)w, (int)h)) {
        conv->Release();
        frame->Release();
        dec->Release();
        fac->Release();
        return 0;
    }
    unsigned char* px = new unsigned char[(size_t)w * (size_t)h * 4];
    hr = conv->CopyPixels(NULL, w * 4, w * h * 4, px);
    conv->Release();
    frame->Release();
    dec->Release();
    fac->Release();
    if (FAILED(hr)) {
        delete[] px;
        return 0;
    }
    *ow = (int)w;
    *oh = (int)h;
    *out = px;
    return 1;
}
#endif

#ifdef __APPLE__
static int __nexa_gfx_cg_decode(const unsigned char* data, int n, int* ow, int* oh, unsigned char** out) {
    if (!data || n < 8 || !ow || !oh || !out) return 0;
    CFDataRef cf = CFDataCreate(kCFAllocatorDefault, data, (CFIndex)n);
    if (!cf) return 0;
    CGImageSourceRef src = CGImageSourceCreateWithData(cf, NULL);
    CFRelease(cf);
    if (!src) return 0;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return 0;
    size_t w = CGImageGetWidth(img);
    size_t h = CGImageGetHeight(img);
    if (!__nexa_gfx_pixels_ok((int)w, (int)h)) {
        CGImageRelease(img);
        return 0;
    }
    unsigned char* px = new unsigned char[w * h * 4];
    std::memset(px, 0, w * h * 4);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) {
        delete[] px;
        CGImageRelease(img);
        return 0;
    }
    CGContextRef ctx = CGBitmapContextCreate(
        px, w, h, 8, w * 4, cs,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!ctx) {
        delete[] px;
        CGImageRelease(img);
        return 0;
    }
    CGContextTranslateCTM(ctx, 0, (CGFloat)h);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetBlendMode(ctx, kCGBlendModeCopy);
    CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), img);
    CGContextRelease(ctx);
    CGImageRelease(img);
    for (size_t i = 0; i < w * h; i++) {
        unsigned char a = px[i * 4 + 3];
        if (a == 0 || a == 255) continue;
        px[i * 4 + 0] = (unsigned char)((px[i * 4 + 0] * 255 + a / 2) / a);
        px[i * 4 + 1] = (unsigned char)((px[i * 4 + 1] * 255 + a / 2) / a);
        px[i * 4 + 2] = (unsigned char)((px[i * 4 + 2] * 255 + a / 2) / a);
    }
    *ow = (int)w;
    *oh = (int)h;
    *out = px;
    return 1;
}
#endif

static int __nexa_gfx_decode_rgba(const unsigned char* data, int n, int* ow, int* oh, unsigned char** out) {
#ifdef _WIN32
    return __nexa_gfx_wic_decode(data, n, ow, oh, out);
#elif defined(__APPLE__)
    return __nexa_gfx_cg_decode(data, n, ow, oh, out);
#else
    if (!data || n < 8 || !ow || !oh || !out) return 0;
    int w = 0, h = 0;
    unsigned char* px = __nexa_gfx_stbi_load_rgba(data, n, &w, &h);
    if (!px) return 0;
    if (!__nexa_gfx_pixels_ok(w, h)) {
        __nexa_gfx_stbi_free(px);
        return 0;
    }
    unsigned char* copy = new unsigned char[(size_t)w * (size_t)h * 4];
    std::memcpy(copy, px, (size_t)w * (size_t)h * 4);
    __nexa_gfx_stbi_free(px);
    *ow = w;
    *oh = h;
    *out = copy;
    return 1;
#endif
}

)NEXA_GFX";
    if (wantReadFile) out += R"NEXA_GFX(
static std::string __nexa_gfx_read_file(const std::string& path) {
    if (path.empty()) return std::string();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (out.size() + n > (size_t)80 * 1024 * 1024) {
            std::fclose(f);
            return std::string();
        }
        out.append(buf, n);
    }
    std::fclose(f);
    return out;
}
)NEXA_GFX";
    if (need.imageLoad) out += R"NEXA_GFX(
static int __nexa_gfx_store_img(int w, int h, unsigned char* px, const std::string& key) {
    if (__nexa_imgs.empty()) {
        __nexa_GfxImg z;
        z.w = 0;
        z.h = 0;
        z.px = NULL;
        __nexa_imgs.push_back(z);
        __nexa_img_paths.push_back("");
    }
    __nexa_GfxImg im;
    im.w = w;
    im.h = h;
    im.px = px;
    __nexa_imgs.push_back(im);
    __nexa_img_paths.push_back(key);
    return (int)__nexa_imgs.size() - 1;
}

static int __nexa_gfx_decode(const std::string& bytes) {
    int w = 0, h = 0;
    unsigned char* px = NULL;
    if (!__nexa_gfx_decode_rgba((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &px) || !px) {
        return 0;
    }
    return __nexa_gfx_store_img(w, h, px, "");
}

static int __nexa_gfx_image(const std::string& path) {
    if (path.empty()) return 0;
    for (size_t i = 1; i < __nexa_img_paths.size(); i++) {
        if (__nexa_img_paths[i] == path && __nexa_imgs[i].px) return (int)i;
    }
    std::string bytes = __nexa_gfx_read_file(path);
    if (bytes.empty()) return 0;
    int w = 0, h = 0;
    unsigned char* px = NULL;
    if (!__nexa_gfx_decode_rgba((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &px) || !px) {
        return 0;
    }
    return __nexa_gfx_store_img(w, h, px, path);
}
)NEXA_GFX";
    if (need.imageStore) out += R"NEXA_GFX(
static int __nexa_gfx_image_w(int id) {
    if (id < 1 || id >= (int)__nexa_imgs.size() || !__nexa_imgs[(size_t)id].px) return 0;
    return __nexa_imgs[(size_t)id].w;
}

static int __nexa_gfx_image_h(int id) {
    if (id < 1 || id >= (int)__nexa_imgs.size() || !__nexa_imgs[(size_t)id].px) return 0;
    return __nexa_imgs[(size_t)id].h;
}
)NEXA_GFX";
    if (need.blit) out += R"NEXA_GFX(
// [nexa:blit-begin]
static int __nexa_gfx_blit(int x, int y, int id, int dw, int dh, int sx, int sy, int sw, int sh) {
    if (!__nexa_g.fb || !__nexa_g.ready) return 0;
    if (id < 1 || id >= (int)__nexa_imgs.size()) return 0;
    const __nexa_GfxImg& im = __nexa_imgs[(size_t)id];
    if (!im.px || im.w < 1 || im.h < 1) return 0;
    if (sw > 0 && sh > 0) {
        if (sx < 0) { sw += sx; sx = 0; }
        if (sy < 0) { sh += sy; sy = 0; }
        if (sx >= im.w || sy >= im.h || sw < 1 || sh < 1) return 0;
        if (sx + sw > im.w) sw = im.w - sx;
        if (sy + sh > im.h) sh = im.h - sy;
        if (sw < 1 || sh < 1) return 0;
    } else {
        sx = 0;
        sy = 0;
        sw = im.w;
        sh = im.h;
    }
    // A negative destination size mirrors the image on that axis. The box the
    // image lands in does not move: it is still |dw| wide starting at x and
    // |dh| tall starting at y, only the sampling runs the other way. Widths
    // are carried as long long from here on, so INT_MIN negates and a
    // destination the size of the coordinate space cannot overflow.
    long long DW = dw;
    long long DH = dh;
    int flipx = DW < 0;
    int flipy = DH < 0;
    if (flipx) DW = -DW;
    if (flipy) DH = -DH;
    if (DW < 1) DW = sw;
    if (DH < 1) DH = sh;
    long long X = x;
    long long Y = y;
    // Walk only the part of the destination box that is on screen. Clipping
    // per pixel instead would make a blit scaled to two billion pixels wide
    // spin for an hour to draw the twelve of them that are visible.
    long long yy0 = -Y > 0 ? -Y : 0;
    long long yy1 = DH < (long long)__nexa_g.h - Y ? DH : (long long)__nexa_g.h - Y;
    long long xx0 = -X > 0 ? -X : 0;
    long long xx1 = DW < (long long)__nexa_g.w - X ? DW : (long long)__nexa_g.w - X;
    if (yy0 >= yy1 || xx0 >= xx1) return 0;
    int ga = __nexa_gfx_alpha_get();
    int drew = 0;
    for (long long yy = yy0; yy < yy1; yy++) {
        int py = (int)(Y + yy);
        long long ty = flipy ? DH - 1 - yy : yy;
        int srcy = sy + (int)(ty * sh / DH);
        if (srcy < sy) srcy = sy;
        if (srcy >= sy + sh) srcy = sy + sh - 1;
        for (long long xx = xx0; xx < xx1; xx++) {
            int px = (int)(X + xx);
            long long tx = flipx ? DW - 1 - xx : xx;
            int srcx = sx + (int)(tx * sw / DW);
            if (srcx < sx) srcx = sx;
            if (srcx >= sx + sw) srcx = sx + sw - 1;
            const unsigned char* s = im.px + ((size_t)srcy * (size_t)im.w + (size_t)srcx) * 4;
            // Image alpha and the global draw alpha multiply, so gfx.alpha
            // fades a blit the same way it fades a shape, and an opaque image
            // at the default alpha still takes the straight-copy path.
            int A = s[3];
            if (ga != 255) A = (A * ga + 127) / 255;
            __nexa_gfx_put_a((py * __nexa_g.w + px) * 4, s[0], s[1], s[2], (unsigned char)A);
            drew = 1;
        }
    }
    return drew;
}
// [nexa:blit-end]

static int __nexa_gfx_blit_path(int x, int y, const std::string& path, int dw, int dh, int sx, int sy, int sw, int sh) {
    return __nexa_gfx_blit(x, y, __nexa_gfx_image(path), dw, dh, sx, sy, sw, sh);
}
)NEXA_GFX";
    if (need.blitRot) out += R"NEXA_GFX(
// [nexa:blitrot-begin]
// A rotated blit, by inverse mapping: rather than turning each source pixel
// into the framebuffer -- which scatters them and leaves holes between --
// every pixel of the rotated box's bounding box is turned *back* into the
// destination box and asks which source pixel it landed on. Nothing is drawn
// twice and nothing is missed.
static int __nexa_gfx_blit_rot(int x, int y, int id, double angle, int dw, int dh) {
    if (!__nexa_g.fb || !__nexa_g.ready) return 0;
    if (id < 1 || id >= (int)__nexa_imgs.size()) return 0;
    const __nexa_GfxImg& im = __nexa_imgs[(size_t)id];
    if (!im.px || im.w < 1 || im.h < 1) return 0;
    // The destination box is read exactly as gfx.blit reads it: |dw| by |dh|
    // at x,y, a negative size mirroring that axis without moving the box, and
    // an omitted one (0) meaning the image's own size.
    long long DW = dw;
    long long DH = dh;
    int flipx = DW < 0;
    int flipy = DH < 0;
    if (flipx) DW = -DW;
    if (flipy) DH = -DH;
    if (DW < 1) DW = im.w;
    if (DH < 1) DH = im.h;
    double cs, sn;
    __nexa_gfx_cos_sin(angle, &cs, &sn);
    double halfw = (double)DW * 0.5;
    double halfh = (double)DH * 0.5;
    double ox = (double)x + halfw;   // the centre the box turns about
    double oy = (double)y + halfh;
    // The turned box's bounding box, clipped to the framebuffer before the
    // loop rather than inside it: a sprite scaled to two billion pixels wide
    // must cost the twelve of them that are on screen, not the two billion.
    double ex = std::fabs(halfw * cs) + std::fabs(halfh * sn);
    double ey = std::fabs(halfw * sn) + std::fabs(halfh * cs);
    long long px0 = (long long)std::floor(ox - ex);
    long long px1 = (long long)std::ceil(ox + ex);
    long long py0 = (long long)std::floor(oy - ey);
    long long py1 = (long long)std::ceil(oy + ey);
    if (px0 < 0) px0 = 0;
    if (py0 < 0) py0 = 0;
    if (px1 > (long long)__nexa_g.w - 1) px1 = (long long)__nexa_g.w - 1;
    if (py1 > (long long)__nexa_g.h - 1) py1 = (long long)__nexa_g.h - 1;
    if (px0 > px1 || py0 > py1) return 0;
    int ga = __nexa_gfx_alpha_get();
    int drew = 0;
    for (long long py = py0; py <= py1; py++) {
        double dy = (double)py + 0.5 - oy;
        for (long long px = px0; px <= px1; px++) {
            double dx = (double)px + 0.5 - ox;
            // Turn the pixel's centre back the other way. Landing outside the
            // box is what makes the corners of the bounding box transparent.
            double u = dx * cs + dy * sn + halfw;
            double v = dy * cs - dx * sn + halfh;
            if (u < 0.0 || v < 0.0 || u >= (double)DW || v >= (double)DH) continue;
            long long tx = (long long)u;
            long long ty = (long long)v;
            if (tx > DW - 1) tx = DW - 1;   // u < DW already; rounding insurance
            if (ty > DH - 1) ty = DH - 1;
            // Mirror the texel index, not the continuous coordinate, exactly
            // as gfx.blit does. At a quarter turn the back-mapped u lands on a
            // whole number whenever the box's width and height disagree in
            // parity, and flipping before the truncation would slide the whole
            // sprite one texel -- dropping an edge row and doubling the other.
            if (flipx) tx = DW - 1 - tx;
            if (flipy) ty = DH - 1 - ty;
            int srcx = (int)(tx * (long long)im.w / DW);
            int srcy = (int)(ty * (long long)im.h / DH);
            const unsigned char* s = im.px + ((size_t)srcy * (size_t)im.w + (size_t)srcx) * 4;
            int A = s[3];
            if (ga != 255) A = (A * ga + 127) / 255;
            __nexa_gfx_put_a((int)((py * (long long)__nexa_g.w + px) * 4), s[0], s[1], s[2],
                             (unsigned char)A);
            drew = 1;
        }
    }
    return drew;
}
// [nexa:blitrot-end]

static int __nexa_gfx_blit_rot_path(int x, int y, const std::string& path, double angle, int dw, int dh) {
    return __nexa_gfx_blit_rot(x, y, __nexa_gfx_image(path), angle, dw, dh);
}
)NEXA_GFX";
    if (need.icon) out += R"NEXA_GFX(
// [nexa:icon-begin]
#if defined(_WIN32) || (defined(__linux__) && !defined(__EMSCRIPTEN__))
// Nearest-neighbour scale of a loaded image into a w by h RGBA buffer, for the
// two backends that need a size of their own: Win32 wants square icons, and
// X11 will not thank you for a window property holding a four-megapixel
// photograph. macOS and the browser take the artwork at its own size and scale
// it themselves, so neither reaches this.
static void __nexa_gfx_icon_pixels(const __nexa_GfxImg& im, int w, int h, unsigned char* out) {
    for (int y = 0; y < h; y++) {
        int sy = (int)((long long)y * (long long)im.h / (long long)h);
        if (sy > im.h - 1) sy = im.h - 1;
        for (int x = 0; x < w; x++) {
            int sx = (int)((long long)x * (long long)im.w / (long long)w);
            if (sx > im.w - 1) sx = im.w - 1;
            const unsigned char* s = im.px + ((size_t)sy * (size_t)im.w + (size_t)sx) * 4;
            unsigned char* d = out + ((size_t)y * (size_t)w + (size_t)x) * 4;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = s[3];
        }
    }
}
#endif
)NEXA_GFX";
    if (need.icon) out += R"NEXA_GFX(
#ifdef _WIN32
// One HICON at one square size. The colour bitmap carries the alpha channel,
// so the mask is all zeroes -- "no pixel is transparent by the mask" -- and
// the per-pixel alpha decides what shows, which is what a modern icon wants.
static HICON __nexa_gfx_win_icon(const __nexa_GfxImg& im, int size) {
    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP colour = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!colour || !bits) {
        if (colour) DeleteObject(colour);
        return nullptr;
    }
    std::vector<unsigned char> rgba((size_t)size * (size_t)size * 4);
    __nexa_gfx_icon_pixels(im, size, size, rgba.data());
    unsigned char* d = (unsigned char*)bits;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        d[i + 0] = rgba[i + 2];
        d[i + 1] = rgba[i + 1];
        d[i + 2] = rgba[i + 0];
        d[i + 3] = rgba[i + 3];
    }
    // Monochrome scanlines are WORD aligned; zeroed, so the mask hides nothing.
    std::vector<unsigned char> mbits((size_t)(((size + 15) / 16) * 2) * (size_t)size, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, mbits.data());
    HICON ic = nullptr;
    if (mask) {
        ICONINFO ii;
        std::memset(&ii, 0, sizeof(ii));
        ii.fIcon = TRUE;
        ii.hbmMask = mask;
        ii.hbmColor = colour;
        ic = CreateIconIndirect(&ii);
        DeleteObject(mask);
    }
    DeleteObject(colour);
    return ic;
}
#endif

// gfx.icon: hand the window an image to wear. 1 when the platform took it.
static int __nexa_gfx_icon(int id) {
    if (!__nexa_g.ready) return 0;
    if (id < 1 || id >= (int)__nexa_imgs.size()) return 0;
    const __nexa_GfxImg& im = __nexa_imgs[(size_t)id];
    if (!im.px || im.w < 1 || im.h < 1) return 0;
#ifdef __EMSCRIPTEN__
    // The page's favicon is the nearest thing a canvas has to a window icon,
    // and a canvas plus toDataURL is the whole encoder, so it is worth doing.
    EM_ASM(({
        var w = $1, h = $2;
        var cv = document.createElement('canvas');
        cv.width = w;
        cv.height = h;
        var cx = cv.getContext('2d');
        var id = cx.createImageData(w, h);
        id.data.set(HEAPU8.subarray($0, $0 + w * h * 4));
        cx.putImageData(id, 0, 0);
        var link = document.querySelector("link[rel~='icon']");
        if (!link) {
            link = document.createElement('link');
            link.rel = 'icon';
            document.head.appendChild(link);
        }
        link.href = cv.toDataURL('image/png');
    }), im.px, im.w, im.h);
    return 1;
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return 0;
    HICON big = __nexa_gfx_win_icon(im, 32);
    HICON small_ = __nexa_gfx_win_icon(im, 16);
    if (!big && !small_) return 0;
    if (big) SendMessageA(__nexa_g.hwnd, WM_SETICON, ICON_BIG, (LPARAM)big);
    if (small_) SendMessageA(__nexa_g.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small_);
    // Only now: the window was still drawing the old ones a moment ago.
    if (__nexa_g.icon_big) DestroyIcon(__nexa_g.icon_big);
    if (__nexa_g.icon_small) DestroyIcon(__nexa_g.icon_small);
    __nexa_g.icon_big = big;
    __nexa_g.icon_small = small_;
    return 1;
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return 0;
    @autoreleasepool {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:im.w
                          pixelsHigh:im.h
                       bitsPerSample:8
                     samplesPerPixel:4
                            hasAlpha:YES
                            isPlanar:NO
                      colorSpaceName:NSDeviceRGBColorSpace
                        bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                         bytesPerRow:im.w * 4
                        bitsPerPixel:32];
        if (!rep) return 0;
        std::memcpy([rep bitmapData], im.px, (size_t)im.w * (size_t)im.h * 4);
        NSImage* img = [[NSImage alloc] initWithSize:NSMakeSize(im.w, im.h)];
        [img addRepresentation:rep];
        [NSApp setApplicationIconImage:img];
        return 1;
    }
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    // _NET_WM_ICON is width, height, then one 32-bit ARGB pixel per cell, in a
    // property whose 32-bit format means `long` on this side of the wire. Big
    // artwork is scaled down first: this is a title-bar icon, and a property
    // holding a four-megapixel photograph helps nobody.
    int tw = im.w;
    int th = im.h;
    if (tw > 128 || th > 128) {
        double f = 128.0 / (double)(tw > th ? tw : th);
        tw = (int)((double)tw * f);
        th = (int)((double)th * f);
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
    }
    std::vector<unsigned char> rgba((size_t)tw * (size_t)th * 4);
    __nexa_gfx_icon_pixels(im, tw, th, rgba.data());
    std::vector<long> prop((size_t)tw * (size_t)th + 2);
    prop[0] = tw;
    prop[1] = th;
    for (size_t i = 0; i < (size_t)tw * (size_t)th; i++) {
        const unsigned char* s = &rgba[i * 4];
        prop[i + 2] = ((long)s[3] << 24) | ((long)s[0] << 16) | ((long)s[1] << 8) | (long)s[2];
    }
    Atom net_icon = XInternAtom(__nexa_g.dpy, "_NET_WM_ICON", False);
    Atom cardinal = XInternAtom(__nexa_g.dpy, "CARDINAL", False);
    XChangeProperty(__nexa_g.dpy, __nexa_g.win, net_icon, cardinal, 32, PropModeReplace,
                    (const unsigned char*)prop.data(), (int)prop.size());
    XFlush(__nexa_g.dpy);
    return 1;
#else
    return 0;
#endif
}

static int __nexa_gfx_icon_path(const std::string& path) {
    return __nexa_gfx_icon(__nexa_gfx_image(path));
}

// WM_SETICON borrows an icon, it does not adopt it, so the two we made are
// ours to destroy when the window they were drawn on goes away.
static void __nexa_gfx_icon_reset() {
#ifdef _WIN32
    if (__nexa_g.icon_big) DestroyIcon(__nexa_g.icon_big);
    if (__nexa_g.icon_small) DestroyIcon(__nexa_g.icon_small);
    __nexa_g.icon_big = nullptr;
    __nexa_g.icon_small = nullptr;
#endif
}
// [nexa:icon-end]
)NEXA_GFX";
    if (need.save) out += R"NEXA_GFX(
// [nexa:screenshot-begin]
// gfx.save writes a 24-bit uncompressed BMP: every platform can read one, and
// writing one needs nothing but fwrite -- no encoder, no OS imaging library, no
// new dependency on the WASM build. The framebuffer is read back through
// __nexa_gfx_get, so the BGRA/RGBA difference between the platforms is already
// handled in one place.
static void __nexa_gfx_le32(std::string& out, unsigned int v) {
    out.push_back((char)(unsigned char)(v & 0xFF));
    out.push_back((char)(unsigned char)((v >> 8) & 0xFF));
    out.push_back((char)(unsigned char)((v >> 16) & 0xFF));
    out.push_back((char)(unsigned char)((v >> 24) & 0xFF));
}

static void __nexa_gfx_le16(std::string& out, unsigned int v) {
    out.push_back((char)(unsigned char)(v & 0xFF));
    out.push_back((char)(unsigned char)((v >> 8) & 0xFF));
}

static int __nexa_gfx_save(const std::string& path) {
    if (path.empty()) return 0;
    if (!__nexa_g.fb || !__nexa_g.ready) return 0;
    int w = __nexa_g.w;
    int h = __nexa_g.h;
    if (w < 1 || h < 1) return 0;
    size_t stride = ((size_t)w * 3 + 3) & ~(size_t)3;
    size_t pixels = stride * (size_t)h;
    std::string out;
    out.reserve(54 + pixels);
    out.push_back('B');
    out.push_back('M');
    __nexa_gfx_le32(out, (unsigned int)(54 + pixels));
    __nexa_gfx_le32(out, 0);
    __nexa_gfx_le32(out, 54);
    __nexa_gfx_le32(out, 40);
    __nexa_gfx_le32(out, (unsigned int)w);
    __nexa_gfx_le32(out, (unsigned int)h);
    __nexa_gfx_le16(out, 1);
    __nexa_gfx_le16(out, 24);
    __nexa_gfx_le32(out, 0);
    __nexa_gfx_le32(out, (unsigned int)pixels);
    __nexa_gfx_le32(out, 2835);
    __nexa_gfx_le32(out, 2835);
    __nexa_gfx_le32(out, 0);
    __nexa_gfx_le32(out, 0);
    // A BMP with a positive height is stored bottom row first.
    for (int y = h - 1; y >= 0; y--) {
        size_t row = out.size();
        for (int x = 0; x < w; x++) {
            int c = __nexa_gfx_get(x, y);
            if (c < 0) c = 0;
            out.push_back((char)(unsigned char)(c & 0xFF));
            out.push_back((char)(unsigned char)((c >> 8) & 0xFF));
            out.push_back((char)(unsigned char)((c >> 16) & 0xFF));
        }
        while (out.size() - row < stride) out.push_back('\0');
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return 0;
    size_t n = std::fwrite(out.data(), 1, out.size(), f);
    // A short write is a full failure: a truncated BMP is not a screenshot.
    if (std::fclose(f) != 0 || n != out.size()) return 0;
    return 1;
}
// [nexa:screenshot-end]
)NEXA_GFX";
    if (need.dialogs) out += R"NEXA_GFX(
static std::string __nexa_gfx_filter_safe(const std::string& spec) {
    std::string o;
    for (char c : spec) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '*' || c == '.' || c == ';' || c == ',' || c == ' ' || c == '|') {
            o += c;
        }
    }
    return o;
}

static std::string __nexa_gfx_drop() {
#ifdef __EMSCRIPTEN__
    __nexa_gfx_poll();
#endif
    std::string p = __nexa_g.drop_path;
    __nexa_g.drop_path.clear();
    return p;
}

static std::string __nexa_gfx_opendialog(const std::string& spec) {
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var i = document.getElementById('nexa-file');
        if (i) i.click();
    }));
    return std::string();
#elif defined(_WIN32)
    char file[MAX_PATH];
    file[0] = 0;
    std::string filt = "Files";
    filt.push_back('\0');
    std::string pat = __nexa_gfx_filter_safe(spec);
    if (pat.empty()) pat = "*.*";
    filt += pat;
    filt.push_back('\0');
    filt += "All files";
    filt.push_back('\0');
    filt += "*.*";
    filt.push_back('\0');
    filt.push_back('\0');
    OPENFILENAMEA ofn;
    std::memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = __nexa_g.hwnd;
    ofn.lpstrFilter = filt.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameA(&ofn)) return std::string(file);
    return std::string();
#elif defined(__APPLE__)
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setTitle:@"Open"];
        if ([panel runModal] != NSModalResponseOK) return std::string();
        NSURL* url = [[panel URLs] firstObject];
        if (!url) return std::string();
        NSString* p = [url path];
        if (!p) return std::string();
        return std::string([p UTF8String]);
    }
#elif defined(__linux__)
    std::string pat = __nexa_gfx_filter_safe(spec);
    for (char& c : pat) if (c == ';') c = ' ';
    std::string cmd;
    if (std::system("command -v zenity >/dev/null 2>&1") == 0) {
        cmd = "zenity --file-selection --title='Open'";
        if (!pat.empty()) cmd += " --file-filter='Files | " + pat + "'";
    } else if (std::system("command -v kdialog >/dev/null 2>&1") == 0) {
        cmd = "kdialog --getopenfilename . '" + (pat.empty() ? std::string("*") : pat) + "'";
    } else {
        return std::string();
    }
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return std::string();
    char buf[4096];
    std::string out;
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
#else
    (void)spec;
    return std::string();
#endif
}
)NEXA_GFX";
    // gfx.audio / sample / audio_queued / audio_flush: one 16-bit mono PCM
    // stream, four backends. Every one of them is the same shape -- a staging
    // buffer of NEXA_PCM_LEN samples that gfx.sample fills and a submit that
    // hands it to the device when it is full -- so the only thing that differs
    // below the ladder is how a block of samples reaches a speaker.
    //
    // The whole block is bracketed by [nexa:audio-*] markers that are emitted
    // into the generated program on purpose (unlike the [nexa:rasterizers-*]
    // pairs above, which are lifted out of this header). No machine has all
    // four devices, and this one has none of them, so Tests/gfx_sound_cases.sh
    // cuts the marked range out of a generated file and drops in a speaker that
    // simply remembers -- which is how the mixer above gets executed and
    // checked here rather than merely compiled. Keep the markers on their own
    // comment lines and keep everything a backend needs between them.
    //
    // An `#include` inside a branch below is safe to repeat: the dedup pass
    // (dedupUnconditionalIncludes in PlatformEmit.hpp) only drops a duplicate
    // when an unguarded copy is already in the file, so it can never leave a
    // branch without a header that slicing then needs. It did once -- <thread>
    // in the AudioQueue branch swallowed std/thread's copy and broke every
    // non-Apple build that mixed gfx.play with thread.spawn -- which is what
    // that pass now exists to prevent.
    if (need.audio) out += R"NEXA_GFX(
// [nexa:audio-backend-begin]
#ifdef _WIN32
#define NEXA_PCM_BUFS 4
#define NEXA_PCM_LEN 2048
static HWAVEOUT __nexa_wo = NULL;
static WAVEHDR __nexa_wh[NEXA_PCM_BUFS];
static short __nexa_wb[NEXA_PCM_BUFS][NEXA_PCM_LEN];
static int __nexa_wf = 0;
static int __nexa_wn = 0;
static int __nexa_audio_rate = 0;

static void __nexa_gfx_audio_close() {
    if (!__nexa_wo) return;
    waveOutReset(__nexa_wo);
    for (int i = 0; i < NEXA_PCM_BUFS; i++) {
        if (__nexa_wh[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(__nexa_wo, &__nexa_wh[i], sizeof(WAVEHDR));
        std::memset(&__nexa_wh[i], 0, sizeof(WAVEHDR));
    }
    waveOutClose(__nexa_wo);
    __nexa_wo = NULL;
    __nexa_wf = 0;
    __nexa_wn = 0;
    __nexa_audio_rate = 0;
}

static int __nexa_audio_free_buf() {
    for (int n = 0; n < 80; n++) {
        int i = 0;
        while (i < NEXA_PCM_BUFS) {
            DWORD f = __nexa_wh[i].dwFlags;
            if (!(f & WHDR_INQUEUE)) return i;
            i++;
        }
        Sleep(1);
    }
    return -1;
}

static void __nexa_audio_submit() {
    if (!__nexa_wo || __nexa_wn < 1) return;
    int i = __nexa_audio_free_buf();
    if (i < 0) {
        __nexa_wn = 0;
        return;
    }
    if (i != __nexa_wf) {
        int n = 0;
        while (n < __nexa_wn) {
            __nexa_wb[i][n] = __nexa_wb[__nexa_wf][n];
            n++;
        }
        __nexa_wf = i;
    }
    __nexa_wh[i].lpData = (LPSTR)__nexa_wb[i];
    __nexa_wh[i].dwBufferLength = (DWORD)(__nexa_wn * (int)sizeof(short));
    __nexa_wh[i].dwFlags = WHDR_PREPARED;
    __nexa_wh[i].dwLoops = 0;
    waveOutWrite(__nexa_wo, &__nexa_wh[i], sizeof(WAVEHDR));
    __nexa_wn = 0;
    __nexa_wf = (__nexa_wf + 1) % NEXA_PCM_BUFS;
}

static int __nexa_gfx_audio(int rate) {
    if (rate < 8000 || rate > 96000) rate = 44100;
    if (__nexa_wo && __nexa_audio_rate == rate) return 1;
    __nexa_gfx_audio_close();
    WAVEFORMATEX fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = (DWORD)rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = (DWORD)(rate * 2);
    if (waveOutOpen(&__nexa_wo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        __nexa_wo = NULL;
        return 0;
    }
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        std::memset(&__nexa_wh[i], 0, sizeof(WAVEHDR));
        __nexa_wh[i].lpData = (LPSTR)__nexa_wb[i];
        __nexa_wh[i].dwBufferLength = (DWORD)(NEXA_PCM_LEN * (int)sizeof(short));
        waveOutPrepareHeader(__nexa_wo, &__nexa_wh[i], sizeof(WAVEHDR));
        i++;
    }
    __nexa_wf = 0;
    __nexa_wn = 0;
    __nexa_audio_rate = rate;
    return 1;
}

static int __nexa_gfx_sample(int s) {
    if (!__nexa_wo) return 0;
    if (s < -32768) s = -32768;
    if (s > 32767) s = 32767;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    if (__nexa_wn >= NEXA_PCM_LEN) return 0;
    __nexa_wb[__nexa_wf][__nexa_wn] = (short)s;
    __nexa_wn++;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    return 1;
}

static int __nexa_gfx_audio_queued() {
    if (!__nexa_wo) return 0;
    int n = __nexa_wn;
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        if (__nexa_wh[i].dwFlags & WHDR_INQUEUE) n += NEXA_PCM_LEN;
        i++;
    }
    return n;
}

static void __nexa_gfx_audio_flush() {
    if (__nexa_wn > 0) __nexa_audio_submit();
}
#elif defined(__EMSCRIPTEN__)
static int __nexa_audio_rate = 0;

static void __nexa_gfx_audio_close() {
    EM_ASM(({
        var ac = Module["nexaAC"];
        if (ac && ac.close) ac.close();
        Module["nexaAC"] = null;
        Module["nexaAQ"] = null;
        Module["nexaSP"] = null;
    }));
    __nexa_audio_rate = 0;
}

static int __nexa_gfx_audio(int rate) {
    if (rate < 8000 || rate > 96000) rate = 44100;
    if (__nexa_audio_rate == rate) {
        EM_ASM(({
            var ac = Module["nexaAC"];
            if (ac && ac.state === "suspended") ac.resume();
        }));
        return 1;
    }
    __nexa_gfx_audio_close();
    EM_ASM(({
        var r = $0;
        var AC = window.AudioContext || window.webkitAudioContext;
        if (!AC) return;
        var ac = new AC({sampleRate: r});
        Module["nexaAC"] = ac;
        Module["nexaAQ"] = [];
        var sp = ac.createScriptProcessor(2048, 0, 1);
        sp.onaudioprocess = function(ev) {
            var o = ev.outputBuffer.getChannelData(0);
            var q = Module["nexaAQ"];
            var i = 0;
            while (i < o.length) {
                if (q && q.length) o[i] = q.shift() / 32768.0;
                else o[i] = 0.0;
                i = i + 1;
            }
        };
        sp.connect(ac.destination);
        Module["nexaSP"] = sp;
        if (ac.state === "suspended") ac.resume();
    }), rate);
    __nexa_audio_rate = rate;
    return 1;
}

static int __nexa_gfx_sample(int s) {
    if (__nexa_audio_rate < 1) return 0;
    if (s < -32768) s = -32768;
    if (s > 32767) s = 32767;
    return EM_ASM_INT(({
        var q = Module["nexaAQ"];
        if (!q) return 0;
        if (q.length > 44100) return 0;
        q.push($0);
        return 1;
    }), s);
}

static int __nexa_gfx_audio_queued() {
    return EM_ASM_INT(({
        var q = Module["nexaAQ"];
        return q ? q.length : 0;
    }));
}

static void __nexa_gfx_audio_flush() {}
#elif defined(__APPLE__)
// AudioQueue: the same four-buffer ring as waveOut. AudioToolbox is a system
// framework, auto-linked next to Cocoa, so this costs the build nothing.
#include <AudioToolbox/AudioToolbox.h>
#include <atomic>
#include <chrono>
#include <thread>

#define NEXA_PCM_BUFS 4
#define NEXA_PCM_LEN 2048

static AudioQueueRef __nexa_aq = NULL;
static AudioQueueBufferRef __nexa_aqb[NEXA_PCM_BUFS];
// AudioQueue hands a buffer back on its own thread, so ownership of each buffer
// is the one thing here that crosses threads: the callback stores 0, the
// program stores 1 just before enqueueing, and nothing else is shared. The
// frame count next to it is written and read by the program only, and only
// consulted while busy is 1 -- which the program itself set, after writing it.
static std::atomic<int> __nexa_aq_busy[NEXA_PCM_BUFS];
static int __nexa_aq_frames[NEXA_PCM_BUFS];
static short __nexa_ab[NEXA_PCM_LEN];
static int __nexa_wn = 0;
static int __nexa_audio_rate = 0;

static void __nexa_aq_done(void* ud, AudioQueueRef q, AudioQueueBufferRef b) {
    (void)ud;
    (void)q;
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        // A buffer from a queue that has already been disposed matches nothing,
        // which is exactly the right answer.
        if (__nexa_aqb[i] == b) {
            __nexa_aq_busy[i].store(0, std::memory_order_release);
            return;
        }
        i++;
    }
}

static void __nexa_gfx_audio_close() {
    if (__nexa_aq) {
        // Synchronous, the way waveOutReset is: closing the stream drops
        // whatever had not been heard yet rather than blocking the program
        // until a tenth of a second of tail has played.
        AudioQueueStop(__nexa_aq, true);
        AudioQueueDispose(__nexa_aq, true);
        __nexa_aq = NULL;
    }
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        __nexa_aqb[i] = NULL;
        __nexa_aq_busy[i].store(0, std::memory_order_relaxed);
        __nexa_aq_frames[i] = 0;
        i++;
    }
    __nexa_wn = 0;
    __nexa_audio_rate = 0;
}

static int __nexa_audio_free_buf() {
    for (int n = 0; n < 80; n++) {
        int i = 0;
        while (i < NEXA_PCM_BUFS) {
            if (__nexa_aq_busy[i].load(std::memory_order_acquire) == 0) return i;
            i++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return -1;
}

static void __nexa_audio_submit() {
    if (!__nexa_aq || __nexa_wn < 1) return;
    int i = __nexa_audio_free_buf();
    if (i < 0) {
        __nexa_wn = 0;
        return;
    }
    std::memcpy(__nexa_aqb[i]->mAudioData, __nexa_ab, (size_t)__nexa_wn * sizeof(short));
    __nexa_aqb[i]->mAudioDataByteSize = (UInt32)(__nexa_wn * (int)sizeof(short));
    __nexa_aq_frames[i] = __nexa_wn;
    __nexa_aq_busy[i].store(1, std::memory_order_release);
    if (AudioQueueEnqueueBuffer(__nexa_aq, __nexa_aqb[i], 0, NULL) != noErr) {
        __nexa_aq_busy[i].store(0, std::memory_order_release);
    }
    __nexa_wn = 0;
}

static int __nexa_gfx_audio(int rate) {
    if (rate < 8000 || rate > 96000) rate = 44100;
    if (__nexa_aq && __nexa_audio_rate == rate) return 1;
    __nexa_gfx_audio_close();
    AudioStreamBasicDescription d;
    std::memset(&d, 0, sizeof(d));
    d.mSampleRate = (Float64)rate;
    d.mFormatID = kAudioFormatLinearPCM;
    d.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    d.mFramesPerPacket = 1;
    d.mChannelsPerFrame = 1;
    d.mBitsPerChannel = 16;
    d.mBytesPerFrame = 2;
    d.mBytesPerPacket = 2;
    // A NULL run loop means the callback runs on a thread AudioToolbox owns,
    // which is what lets a game that never returns to the main loop keep
    // playing.
    if (AudioQueueNewOutput(&d, __nexa_aq_done, NULL, NULL, NULL, 0, &__nexa_aq) != noErr) {
        __nexa_aq = NULL;
        return 0;
    }
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        if (AudioQueueAllocateBuffer(__nexa_aq, (UInt32)(NEXA_PCM_LEN * (int)sizeof(short)),
                                     &__nexa_aqb[i]) != noErr) {
            AudioQueueDispose(__nexa_aq, true);
            __nexa_aq = NULL;
            return 0;
        }
        __nexa_aq_busy[i].store(0, std::memory_order_relaxed);
        __nexa_aq_frames[i] = 0;
        i++;
    }
    if (AudioQueueStart(__nexa_aq, NULL) != noErr) {
        AudioQueueDispose(__nexa_aq, true);
        __nexa_aq = NULL;
        return 0;
    }
    __nexa_wn = 0;
    __nexa_audio_rate = rate;
    return 1;
}

static int __nexa_gfx_sample(int s) {
    if (!__nexa_aq) return 0;
    if (s < -32768) s = -32768;
    if (s > 32767) s = 32767;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    if (__nexa_wn >= NEXA_PCM_LEN) return 0;
    __nexa_ab[__nexa_wn] = (short)s;
    __nexa_wn++;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    return 1;
}

static int __nexa_gfx_audio_queued() {
    if (!__nexa_aq) return 0;
    int n = __nexa_wn;
    int i = 0;
    while (i < NEXA_PCM_BUFS) {
        if (__nexa_aq_busy[i].load(std::memory_order_acquire)) n += __nexa_aq_frames[i];
        i++;
    }
    return n;
}

static void __nexa_gfx_audio_flush() {
    if (__nexa_wn > 0) __nexa_audio_submit();
}
#elif defined(__linux__)
// One way to reach a speaker: the kernel's own PCM interface, driven straight
// through ioctls on /dev/snd. Nothing is linked and nothing has to be
// installed -- no libasound, no development package, no sound server. That is
// the same bargain the X11 backend makes by writing out the protocol rather
// than linking libX11, and it is what lets a Nexa binary built on one Linux
// play on any other, container and appliance image included.
//
// The price is that the card is held exclusively for as long as a program has
// the stream open. On a desktop running PipeWire that means the sound server
// does not get the device while a Nexa program is playing, which is the honest
// trade for owing nothing to a userspace nobody promised would be there.
//
// If no card opens, gfx.audio() returns 0 -- which is what everything above it
// is already written to cope with.
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

#define NEXA_PCM_LEN 2048

static short __nexa_ab[NEXA_PCM_LEN];
static int __nexa_wn = 0;
// The rate the stream is actually running at, which is what the mixer resamples
// every voice to, and separately the rate gfx.audio() was asked for. There is
// no plugin layer under this to resample for us, so a card that will only do
// 48000 gets fed 48000 and the mixer is told so. Keeping the asked-for rate is
// what stops the second gfx.audio(44100) on such a card from tearing the stream
// down and rebuilding it, over and over.
static int __nexa_audio_rate = 0;
static int __nexa_audio_asked = 0;

// --- the kernel's PCM interface ----------------------------------------------
//
// Everything from here to __nexa_snd_close is the uapi of sound/asound.h,
// hand-written because a build must not need a development package and kernel
// headers are one. It is safe to hand-write because the ioctl request number
// encodes the size of the struct it passes, so a kernel that changed either of
// them would break every ALSA program already compiled -- this is an ABI that
// cannot move.

// Everything between the [nexa:kernel-pcm-*] markers is declaration only, and
// Tests/gfx_pcm_cases.sh cuts that range out of a generated program and
// compiles it on its own to check the sizes and ioctl numbers against the
// kernel's. Getting one of them wrong does not fail to build and does not
// crash: it fails the ioctl, and the program is silently mute. That is the one
// mistake this file cannot notice on its own, so keep the markers where they
// are and keep everything a struct or a number between them.
// [nexa:kernel-pcm-begin]

// _IO/_IOR/_IOW/_IOWR come from <sys/ioctl.h> on both glibc and musl, and using
// the C library's copy is what keeps the encoding right on the architectures
// whose ioctl numbering is not the asm-generic one (powerpc, sparc, mips,
// parisc). The spelled-out version is the asm-generic encoding, which is what
// x86, arm, arm64 and riscv use, and is only reached on a C library that does
// not hand these out at all.
#if defined(_IO) && defined(_IOR) && defined(_IOW) && defined(_IOWR)
#define NEXA_SND_IO(nr) _IO('A', nr)
#define NEXA_SND_IOR(nr, t) _IOR('A', nr, t)
#define NEXA_SND_IOW(nr, t) _IOW('A', nr, t)
#define NEXA_SND_IOWR(nr, t) _IOWR('A', nr, t)
#else
#define NEXA_SND_IOC(dir, nr, sz) \
    (((unsigned long)(dir) << 30) | ((unsigned long)(sz) << 16) | (0x41UL << 8) | (unsigned long)(nr))
#define NEXA_SND_IO(nr) NEXA_SND_IOC(0, nr, 0)
#define NEXA_SND_IOR(nr, t) NEXA_SND_IOC(2, nr, sizeof(t))
#define NEXA_SND_IOW(nr, t) NEXA_SND_IOC(1, nr, sizeof(t))
#define NEXA_SND_IOWR(nr, t) NEXA_SND_IOC(3, nr, sizeof(t))
#endif

// struct snd_mask is 256 bits; struct snd_pcm_hw_params carries three of them
// (access, format, subformat) plus five reserved, and twelve intervals
// (sample_bits through tick_time) plus nine reserved. The reserved runs are
// what make the struct the size the ioctl number claims, so they are not
// padding to be trimmed.
struct __nexa_snd_mask {
    unsigned int bits[8];
};

struct __nexa_snd_interval {
    unsigned int min, max;
    unsigned int openmin : 1, openmax : 1, integer : 1, empty : 1;
};

struct __nexa_snd_hw_params {
    unsigned int flags;
    struct __nexa_snd_mask masks[3];
    struct __nexa_snd_mask mres[5];
    struct __nexa_snd_interval intervals[12];
    struct __nexa_snd_interval ires[9];
    unsigned int rmask, cmask, info, msbits, rate_num, rate_den;
    unsigned long fifo_size;
    unsigned char reserved[64];
};

struct __nexa_snd_sw_params {
    int tstamp_mode;
    unsigned int period_step;
    unsigned int sleep_min;
    unsigned long avail_min;
    unsigned long xfer_align;
    unsigned long start_threshold;
    unsigned long stop_threshold;
    unsigned long silence_threshold;
    unsigned long silence_size;
    unsigned long boundary;
    unsigned int proto;
    unsigned int tstamp_type;
    unsigned char reserved[56];
};

struct __nexa_snd_xferi {
    long result;
    const void* buf;
    unsigned long frames;
};

// Masks are indexed from ACCESS, intervals from SAMPLE_BITS; the numbers are
// the SNDRV_PCM_HW_PARAM_* the kernel uses, kept whole rather than pre-shifted
// so they read like the header they came from.
#define NEXA_SND_PARAM_ACCESS 0
#define NEXA_SND_PARAM_FORMAT 1
#define NEXA_SND_PARAM_SUBFORMAT 2
#define NEXA_SND_PARAM_FIRST_INTERVAL 8
#define NEXA_SND_PARAM_CHANNELS 10
#define NEXA_SND_PARAM_RATE 11
#define NEXA_SND_PARAM_PERIOD_SIZE 13
#define NEXA_SND_PARAM_PERIODS 15
#define NEXA_SND_PARAM_BUFFER_SIZE 17

#define NEXA_SND_ACCESS_RW_INTERLEAVED 3
#define NEXA_SND_FORMAT_S16_LE 2
#define NEXA_SND_SUBFORMAT_STD 0

#define NEXA_SND_IOCTL_HW_PARAMS NEXA_SND_IOWR(0x11, struct __nexa_snd_hw_params)
#define NEXA_SND_IOCTL_SW_PARAMS NEXA_SND_IOWR(0x13, struct __nexa_snd_sw_params)
#define NEXA_SND_IOCTL_DELAY NEXA_SND_IOR(0x21, long)
#define NEXA_SND_IOCTL_PREPARE NEXA_SND_IO(0x40)
#define NEXA_SND_IOCTL_DROP NEXA_SND_IO(0x43)
#define NEXA_SND_IOCTL_DRAIN NEXA_SND_IO(0x44)
#define NEXA_SND_IOCTL_RESUME NEXA_SND_IO(0x47)
#define NEXA_SND_IOCTL_WRITEI NEXA_SND_IOW(0x50, struct __nexa_snd_xferi)
// [nexa:kernel-pcm-end]

static int __nexa_snd_fd = -1;
static unsigned long __nexa_snd_period = 0;
// One or two. The mixer above produces mono, but a raw device has no plugin
// layer under it to widen that and a great many cards -- every HDA codec this
// was tried on, and every HDMI output -- will only take a stereo stream. Where
// that is so the sample goes to both ears through this scratch buffer; a frame
// is still one mixer sample either way, so nothing above has to know which
// happened.
static int __nexa_snd_channels = 1;
static short __nexa_snd_lr[NEXA_PCM_LEN * 2];

// Every ioctl below reports the same way: 0 or a frame count on success, a
// negative errno on failure, so one recover path serves all of them.
static int __nexa_snd_ctl(unsigned long req, void* arg) {
    while (::ioctl(__nexa_snd_fd, req, arg) < 0) {
        if (errno == EINTR) continue;
        return -errno;
    }
    return 0;
}

// "Any value the card will take", which is what a hw_params starts as before
// the wanted ones are narrowed into it.
static void __nexa_snd_params_any(struct __nexa_snd_hw_params* p) {
    std::memset(p, 0, sizeof(*p));
    int i = 0;
    while (i < 3) {
        std::memset(p->masks[i].bits, 0xff, sizeof(p->masks[i].bits));
        i++;
    }
    i = 0;
    while (i < 12) {
        p->intervals[i].min = 0;
        p->intervals[i].max = ~0u;
        i++;
    }
    p->rmask = ~0u;
    p->info = ~0u;
}

static void __nexa_snd_mask_one(struct __nexa_snd_hw_params* p, int which, unsigned int bit) {
    std::memset(p->masks[which].bits, 0, sizeof(p->masks[which].bits));
    p->masks[which].bits[bit >> 5] |= 1u << (bit & 31);
}

static void __nexa_snd_want(struct __nexa_snd_hw_params* p, int param, unsigned int lo, unsigned int hi) {
    struct __nexa_snd_interval* iv = &p->intervals[param - NEXA_SND_PARAM_FIRST_INTERVAL];
    iv->min = lo;
    iv->max = hi;
    iv->integer = 1;
}

static unsigned int __nexa_snd_got(const struct __nexa_snd_hw_params* p, int param) {
    return p->intervals[param - NEXA_SND_PARAM_FIRST_INTERVAL].min;
}

// The software half: when the card starts playing, when it stops, and where the
// ring pointers wrap.
static int __nexa_snd_sw_params(unsigned long buffer) {
    struct __nexa_snd_sw_params sw;
    std::memset(&sw, 0, sizeof(sw));
    sw.period_step = 1;
    sw.avail_min = __nexa_snd_period ? __nexa_snd_period : 1;
    // Start on the first full period rather than waiting for an explicit START,
    // so a program that writes a short sound and then goes quiet still hears it.
    sw.start_threshold = sw.avail_min;
    // Never stop on an underrun: a game that is late with its samples should
    // hear a gap and carry on rather than lose the stream. The kernel reads a
    // threshold of a whole buffer as "do not stop".
    sw.stop_threshold = buffer;
    // The pointers wrap at a power-of-two multiple of the buffer, kept inside
    // 31 bits so the arithmetic is the same on a 32-bit kernel.
    unsigned long boundary = buffer;
    while (boundary * 2 <= 0x7fffffffUL) boundary *= 2;
    sw.boundary = boundary;
    if (__nexa_snd_ctl(NEXA_SND_IOCTL_SW_PARAMS, &sw) < 0) return 0;
    return __nexa_snd_ctl(NEXA_SND_IOCTL_PREPARE, NULL) == 0;
}

// Configures the already-open device for 16-bit PCM at one rate. Four goes at
// it, mono before stereo and the wanted buffer before whatever the driver
// would rather have. The buffer wanted is five twenty-millisecond periods -- a
// tenth of a second, the same depth the mixer aims to keep queued and the same
// order as the four 2048-sample waveOut buffers on Windows -- and the second
// attempt bounds the buffer from above rather than leaving it free, because
// the kernel resolves a buffer size to the top of its range and an unbounded
// one hands back every frame the card can hold, which is seconds of latency on
// some of them.
static int __nexa_snd_configure(int rate) {
    unsigned int period = (unsigned int)(rate / 50);
    if (period < 32) period = 32;
    int channels = 1;
    while (channels < 3) {
        int attempt = 0;
        while (attempt < 2) {
            struct __nexa_snd_hw_params hw;
            __nexa_snd_params_any(&hw);
            __nexa_snd_mask_one(&hw, NEXA_SND_PARAM_ACCESS, NEXA_SND_ACCESS_RW_INTERLEAVED);
            __nexa_snd_mask_one(&hw, NEXA_SND_PARAM_FORMAT, NEXA_SND_FORMAT_S16_LE);
            __nexa_snd_mask_one(&hw, NEXA_SND_PARAM_SUBFORMAT, NEXA_SND_SUBFORMAT_STD);
            __nexa_snd_want(&hw, NEXA_SND_PARAM_CHANNELS, (unsigned int)channels, (unsigned int)channels);
            __nexa_snd_want(&hw, NEXA_SND_PARAM_RATE, (unsigned int)rate, (unsigned int)rate);
            if (attempt == 0) {
                __nexa_snd_want(&hw, NEXA_SND_PARAM_PERIOD_SIZE, period, period);
                __nexa_snd_want(&hw, NEXA_SND_PARAM_PERIODS, 5, 5);
            } else {
                __nexa_snd_want(&hw, NEXA_SND_PARAM_BUFFER_SIZE, 32, (unsigned int)(rate / 5));
            }
            if (__nexa_snd_ctl(NEXA_SND_IOCTL_HW_PARAMS, &hw) == 0) {
                __nexa_snd_channels = channels;
                __nexa_snd_period = __nexa_snd_got(&hw, NEXA_SND_PARAM_PERIOD_SIZE);
                return __nexa_snd_sw_params(__nexa_snd_got(&hw, NEXA_SND_PARAM_BUFFER_SIZE));
            }
            attempt++;
        }
        channels++;
    }
    return 0;
}

// One device node, at whichever rate it will take. A card that refuses the rate
// asked for is offered the two every card does before it is given up on, and
// whichever one it took becomes the rate the mixer resamples to -- there is no
// plugin layer down here to do that for us, and a USB speaker that only speaks
// 48000 should still make noise.
static int __nexa_snd_try(const char* path, int rate) {
    int fd = ::open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return 0;
    __nexa_snd_fd = fd;
    const int rates[3] = {rate, 48000, 44100};
    int i = 0;
    while (i < 3) {
        if (i == 0 || rates[i] != rate) {
            if (__nexa_snd_configure(rates[i])) {
                __nexa_audio_rate = rates[i];
                __nexa_wn = 0;
                return 1;
            }
        }
        i++;
    }
    ::close(fd);
    __nexa_snd_fd = -1;
    __nexa_snd_period = 0;
    __nexa_snd_channels = 1;
    return 0;
}

// Whether a card is one of the display outputs. The lowest-numbered card is
// very often the HDMI audio on the graphics chip, which is a real playback
// device that accepts everything asked of it and is wired to a monitor that is
// usually not plugged in and has no speakers when it is -- so a scan that takes
// the first node that works takes the silent one. /proc/asound/card<N>/id is
// the name the driver registered ("HDMI", "NVidia", "sofhdadsp"), and reading
// it costs an open and a read of a dozen bytes. A machine with no /proc says
// nothing, and nothing is the answer that keeps the card in the running.
static int __nexa_snd_card_is_display(int card) {
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/asound/card%d/id", card);
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char id[32];
    long n = (long)::read(fd, id, sizeof(id) - 1);
    ::close(fd);
    if (n < 1) return 0;
    id[n] = '\0';
    int i = 0;
    while (id[i]) {
        // Upper-cased in place: the names are ASCII and this is the only thing
        // the buffer is used for.
        if (id[i] >= 'a' && id[i] <= 'z') id[i] = (char)(id[i] - 'a' + 'A');
        i++;
    }
    return std::strstr(id, "HDMI") != NULL || std::strstr(id, "NVIDIA") != NULL ||
           std::strstr(id, "DISPLAYPORT") != NULL;
}

// /dev/snd/pcmC<card>D<device>p is the playback node the kernel makes for every
// PCM a sound driver registers. There is no dependency-free way to ask which
// one is wired to a speaker -- that is exactly the judgement the sound server
// we do not have would be making -- so take the first that accepts mono 16-bit,
// display outputs last, and let NEXA_PCM_DEVICE name a node outright for the
// machine where that is still the wrong one. The scan is sixty-four open()
// calls that fail with ENOENT on a machine with no sound card, which together
// cost less than opening one device that works.
static int __nexa_snd_open(int rate) {
    const char* forced = std::getenv("NEXA_PCM_DEVICE");
    if (forced && *forced) return __nexa_snd_try(forced, rate);
    int pass = 0;
    while (pass < 2) {
        int card = 0;
        while (card < 8) {
            if (pass == 0 && __nexa_snd_card_is_display(card)) {
                card++;
                continue;
            }
            int dev = 0;
            while (dev < 4) {
                char path[64];
                std::snprintf(path, sizeof(path), "/dev/snd/pcmC%dD%dp", card, dev);
                if (__nexa_snd_try(path, rate)) return 1;
                dev++;
            }
            card++;
        }
        pass++;
    }
    return 0;
}

// One frame per mixer sample whether the card took mono or stereo, so the frame
// counts this returns -- and the ones __nexa_snd_delay hands back -- are in the
// same unit gfx.audio_queued() reports and no caller has to convert.
static long __nexa_snd_writei(const short* buf, unsigned long frames) {
    struct __nexa_snd_xferi x;
    if (frames > NEXA_PCM_LEN) frames = NEXA_PCM_LEN;
    if (__nexa_snd_channels == 2) {
        unsigned long i = 0;
        while (i < frames) {
            __nexa_snd_lr[i * 2] = buf[i];
            __nexa_snd_lr[i * 2 + 1] = buf[i];
            i++;
        }
        x.buf = (const void*)__nexa_snd_lr;
    } else {
        x.buf = (const void*)buf;
    }
    x.result = 0;
    x.frames = frames;
    int r = __nexa_snd_ctl(NEXA_SND_IOCTL_WRITEI, &x);
    return r < 0 ? (long)r : x.result;
}

// Putting a broken stream back: an underrun is prepared away, and a stream the
// kernel suspended under us is resumed and then prepared. A card
// that will not resume is dropped and prepared instead, which loses the tail
// but gets the next sample out of a speaker.
static int __nexa_snd_recover(int err) {
    if (err == -EPIPE) return __nexa_snd_ctl(NEXA_SND_IOCTL_PREPARE, NULL);
    if (err == -ESTRPIPE) {
        if (__nexa_snd_ctl(NEXA_SND_IOCTL_RESUME, NULL) < 0) {
            __nexa_snd_ctl(NEXA_SND_IOCTL_DROP, NULL);
        }
        return __nexa_snd_ctl(NEXA_SND_IOCTL_PREPARE, NULL);
    }
    return err;
}

// 1 when there is room, 0 on timeout, negative on a stream that needs
// recovering. An underrun reaches a poll as POLLERR rather
// than as an error from poll itself, which is why it is turned back into the
// errno the recover path already knows.
static int __nexa_snd_wait(int ms) {
    struct pollfd p;
    p.fd = __nexa_snd_fd;
    p.events = POLLOUT;
    p.revents = 0;
    int r = ::poll(&p, 1, ms);
    if (r < 0) return errno == EINTR ? 0 : -errno;
    if (r > 0 && (p.revents & (POLLERR | POLLNVAL))) return -EPIPE;
    return r > 0 ? 1 : 0;
}

static long __nexa_snd_delay() {
    long d = 0;
    if (__nexa_snd_ctl(NEXA_SND_IOCTL_DELAY, &d) < 0 || d < 0) return 0;
    return d;
}

static void __nexa_snd_close() {
    if (__nexa_snd_fd < 0) return;
    // The handle is non-blocking, so this hands the tail to the card and
    // returns rather than holding the program until it has played -- the same
    // bargain waveOutReset makes on Windows.
    __nexa_snd_ctl(NEXA_SND_IOCTL_DRAIN, NULL);
    ::close(__nexa_snd_fd);
    __nexa_snd_fd = -1;
    __nexa_snd_period = 0;
    __nexa_snd_channels = 1;
}

// --- the stream, as gfx.audio() and the mixer see it -------------------------

static void __nexa_gfx_audio_close() {
    __nexa_snd_close();
    __nexa_wn = 0;
    __nexa_audio_rate = 0;
    __nexa_audio_asked = 0;
}

// Hands the staged samples to the device. A stream that has fallen behind is
// recovered in place -- __nexa_snd_recover covers both underrun and resume from
// suspend -- and a stream that is merely full is waited on for a bounded time
// and then the rest is dropped, which is what the waveOut path does when all
// four of its headers are in flight. A game that stops calling gfx.sample for a
// second must not find the next call blocked on a second of backlog.
static void __nexa_audio_submit() {
    if (__nexa_snd_fd < 0 || __nexa_wn < 1) return;
    int done = 0;
    int tries = 0;
    while (done < __nexa_wn && tries < 40) {
        tries++;
        long n = __nexa_snd_writei(__nexa_ab + done, (unsigned long)(__nexa_wn - done));
        if (n > 0) {
            done += (int)n;
            continue;
        }
        if (n == 0 || n == -EAGAIN) {
            // Full. Wait on the device rather than spinning -- but the forty
            // two-millisecond turns this loop gets are a ceiling, not a
            // promise, so a stream nobody is draining costs the caller the
            // same eighty milliseconds the waveOut path spends before it
            // gives up on its four headers.
            int w = __nexa_snd_wait(2);
            if (w < 0 && __nexa_snd_recover(w) < 0) break;
            continue;
        }
        if (__nexa_snd_recover((int)n) < 0) break;
    }
    __nexa_wn = 0;
}

static int __nexa_gfx_audio(int rate) {
    if (rate < 8000 || rate > 96000) rate = 44100;
    if (__nexa_snd_fd >= 0 && __nexa_audio_asked == rate) return 1;
    __nexa_gfx_audio_close();
    if (!__nexa_snd_open(rate)) return 0;
    __nexa_audio_asked = rate;
    return 1;
}

static int __nexa_gfx_sample(int s) {
    if (__nexa_snd_fd < 0) return 0;
    if (s < -32768) s = -32768;
    if (s > 32767) s = 32767;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    if (__nexa_wn >= NEXA_PCM_LEN) return 0;
    __nexa_ab[__nexa_wn] = (short)s;
    __nexa_wn++;
    if (__nexa_wn >= NEXA_PCM_LEN) __nexa_audio_submit();
    return 1;
}

static int __nexa_gfx_audio_queued() {
    if (__nexa_snd_fd < 0) return 0;
    // The delay is how far ahead of the speaker the next written sample would
    // be, which is the count this asks for; the staged samples have not been
    // written yet, so they are added on top.
    return __nexa_wn + (int)__nexa_snd_delay();
}

static void __nexa_gfx_audio_flush() {
    if (__nexa_wn > 0) __nexa_audio_submit();
}
#else
// Some other Unix: the stream never opens, so the rate stays 0 and everything
// built on top of it -- gfx.play included -- says so.
[[maybe_unused]] static int __nexa_audio_rate = 0;
static void __nexa_gfx_audio_close() {}
static int __nexa_gfx_audio(int rate) { (void)rate; return 0; }
static int __nexa_gfx_sample(int s) { (void)s; return 0; }
static int __nexa_gfx_audio_queued() { return 0; }
static void __nexa_gfx_audio_flush() {}
#endif
// [nexa:audio-backend-end]
)NEXA_GFX";
    if (need.sound) out += soundRuntimeCpp();

    return out;
}

}  // namespace nexa
