// Semantics cover for the gfx sound engine (BOB-39): gfx.sound, gfx.play,
// gfx.loop, gfx.stop, gfx.volume.
//
// None of this can be tested by a gfx program on this machine. The mixer only
// runs once the audio stream is open, and the only backends that open one are
// winmm and Web Audio -- on macOS and Linux gfx.audio() returns 0, so gfx.play
// returns 0 and not one sample is ever mixed. A test that only ran the public
// API here would therefore assert nothing about the mixing at all.
//
// So this driver does what Tests/gfx_input_semantics.cpp does for input: it
// includes the C++ that NexaC generates and drives the runtime underneath the
// language. Tests/gfx_sound_cases.sh first swaps the three-line macOS/Linux
// audio stub in that generated file for one that opens a fake stream and keeps
// every sample it is handed, which turns the mixer from something the suite
// merely compiles into something it executes and checks sample by sample.
//
// Everything below is backend-independent. The WAV reader, the voice table and
// the mixing arithmetic are one copy of the code shared by all four backends;
// the only thing the swap stands in for is the device at the end of the wire.
//
// Built and run by Tests/gfx_sound_cases.sh. NEXA_GEN is the generated .cpp
// and NEXA_WAV_DIR is where the .wav fixtures live.

#include <cmath>
#include <cstdio>
#include <string>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif
#ifndef NEXA_WAV_DIR
#error "define NEXA_WAV_DIR to the directory holding the .wav fixtures"
#endif

// The generated program has its own main(); this driver supplies the real one.
#define main __nexa_program_main
#include NEXA_GEN
#undef main

static int failures = 0;

static void check(const char* label, bool ok) {
    if (ok) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s\n", label);
        failures++;
    }
}

static void eq(const char* label, long long got, long long want) {
    if (got == want) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: got %lld, wanted %lld\n", label, got, want);
        failures++;
    }
}

static std::string wav(const char* name) {
    return std::string(NEXA_WAV_DIR) + "/" + name;
}

// Peak absolute value of everything the mixer has pushed since the last clear.
static int peak() {
    int p = 0;
    for (size_t i = 0; i < __nexa_cap.size(); i++) {
        int a = __nexa_cap[i] < 0 ? -__nexa_cap[i] : __nexa_cap[i];
        if (a > p) p = a;
    }
    return p;
}

static int nonzero() {
    int n = 0;
    for (size_t i = 0; i < __nexa_cap.size(); i++) {
        if (__nexa_cap[i] != 0) n++;
    }
    return n;
}

static int live_voices() {
    int n = 0;
    for (int i = 0; i < NEXA_VOICES; i++) {
        if (__nexa_voices[i].id != 0) n++;
    }
    return n;
}

int main() {
    // --- the WAV reader ---------------------------------------------------
    // Tests/gfx_beep.wav is 400 samples of a 440 Hz sine at 20000 peak, mono
    // 16-bit at 8000 Hz, so every sample it decodes to is known arithmetic.
    int beep = __nexa_gfx_sound(wav("gfx_beep.wav"));
    eq("mono 16-bit loads", beep, 1);
    eq("keeps the file's rate", __nexa_snds[1].rate, 8000);
    eq("one sample per frame", (long long)__nexa_snds[1].pcm.size(), 400);
    bool exact = true;
    for (int i = 0; i < 400; i++) {
        int want = (int)(20000.0 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 8000.0));
        if (__nexa_snds[1].pcm[(size_t)i] != want) exact = false;
    }
    check("16-bit samples decode exactly", exact);

    // Cached by path, exactly like gfx.image.
    eq("the same path is the same handle", __nexa_gfx_sound(wav("gfx_beep.wav")), beep);

    // Tests/gfx_stereo8.wav is unsigned 8-bit stereo at 11025 Hz whose two
    // channels are mirrored, and it carries a LIST chunk ahead of its data --
    // so this one call covers the 8-bit conversion, the (L+R)/2 fold and
    // walking over a chunk the reader does not know.
    int st = __nexa_gfx_sound(wav("gfx_stereo8.wav"));
    eq("stereo 8-bit loads", st, 2);
    eq("a second sound is a second handle", st, beep + 1);
    eq("stereo keeps its own rate", __nexa_snds[2].rate, 11025);
    eq("a stereo frame is one mono sample", (long long)__nexa_snds[2].pcm.size(), 200);
    int worst = 0;
    for (size_t i = 0; i < __nexa_snds[2].pcm.size(); i++) {
        int a = __nexa_snds[2].pcm[i] < 0 ? -__nexa_snds[2].pcm[i] : __nexa_snds[2].pcm[i];
        if (a > worst) worst = a;
    }
    check("mirrored channels fold to silence", worst <= 128);

    // --- what gets refused ------------------------------------------------
    // Tests/gfx_float.wav is a well-formed RIFF file that is not PCM.
    eq("non-PCM is refused", __nexa_gfx_sound(wav("gfx_float.wav")), 0);
    eq("a missing file is 0", __nexa_gfx_sound(wav("nexa_no_such_sound.wav")), 0);
    eq("an empty path is 0", __nexa_gfx_sound(""), 0);
    eq("a file that is not RIFF is 0", __nexa_gfx_sound(wav("gfx_2x2.png")), 0);
    eq("a refusal allocated no handle", (long long)__nexa_snds.size(), 3);

    // --- starting voices --------------------------------------------------
    eq("handle 0 plays nothing", __nexa_gfx_voice_start(0, 255, 0), 0);
    eq("a negative handle plays nothing", __nexa_gfx_voice_start(-1, 255, 0), 0);
    eq("an unused handle plays nothing", __nexa_gfx_voice_start(99, 255, 0), 0);

    int v1 = __nexa_gfx_voice_start(beep, 255, 0);
    check("gfx.play hands back a voice", v1 >= 1);
    eq("playing opened the stream itself", __nexa_audio_rate, 44100);
    // 8000 Hz into a 44100 Hz stream, as 16.16 fixed point.
    eq("the voice resamples to the stream", __nexa_voices[0].step,
       ((long long)8000 << 16) / 44100);

    eq("stopping a voice that is not playing is 0", __nexa_gfx_stop(v1 + 1000), 0);
    eq("stopping the voice is 1", __nexa_gfx_stop(v1), 1);
    eq("stopping it twice is 0", __nexa_gfx_stop(v1), 0);
    eq("stopping nothing at all is 0", __nexa_gfx_stop(0), 0);

    // Sixteen voices at once, and a seventeenth steals the oldest rather than
    // being refused.
    int oldest = __nexa_gfx_voice_start(beep, 255, 0);
    for (int i = 1; i < NEXA_VOICES; i++) __nexa_gfx_voice_start(beep, 255, 0);
    eq("sixteen voices play at once", live_voices(), NEXA_VOICES);
    int stolen = __nexa_gfx_voice_start(beep, 255, 0);
    check("the seventeenth still gets a voice", stolen >= 1);
    eq("and there are still sixteen", live_voices(), NEXA_VOICES);
    bool oldestGone = true;
    for (int i = 0; i < NEXA_VOICES; i++) {
        if (__nexa_voices[i].id == oldest) oldestGone = false;
    }
    check("the one it took was the oldest", oldestGone);
    eq("gfx.stop() stops all of them", __nexa_gfx_stop(0), 1);
    eq("and leaves none playing", live_voices(), 0);

    // --- the pump ---------------------------------------------------------
    __nexa_cap.clear();
    __nexa_gfx_mix_pump();
    eq("silence is never pumped", (long long)__nexa_cap.size(), 0);

    __nexa_gfx_voice_start(beep, 255, 0);
    __nexa_gfx_mix_pump();
    eq("the pump queues a tenth of a second", (long long)__nexa_cap.size(), 44100 / 10);
    check("nothing left the 16-bit range", peak() <= 32767);
    // 400 samples at 8000 Hz is 0.05 s, so the sound covers about half of the
    // tenth and the rest is the silence after it ended.
    int sounded = nonzero();
    check("about half the pump carried the sound", sounded > 2000 && sounded < 2300);
    eq("a one-shot frees its voice at the end", live_voices(), 0);

    // Already a tenth ahead: a second pump has nothing to add.
    long long before = (long long)__nexa_cap.size();
    __nexa_gfx_voice_start(beep, 255, 0);
    __nexa_gfx_mix_pump();
    eq("the pump stops at the target", (long long)__nexa_cap.size(), before);
    __nexa_gfx_stop(0);

    // --- volume -----------------------------------------------------------
    eq("the master volume starts at full", __nexa_gfx_volume_get(), 255);
    eq("setting it returns what was set", __nexa_gfx_volume_set(200), 200);
    eq("and reading it agrees", __nexa_gfx_volume_get(), 200);
    eq("too high clamps to 255", __nexa_gfx_volume_set(300), 255);
    eq("below zero clamps to 0", __nexa_gfx_volume_set(-5), 0);

    __nexa_cap.clear();
    __nexa_gfx_voice_start(beep, 255, 0);
    __nexa_gfx_mix_pump();
    eq("a master volume of 0 is silence", nonzero(), 0);
    __nexa_gfx_stop(0);
    eq("full volume restored", __nexa_gfx_volume_set(255), 255);

    __nexa_cap.clear();
    __nexa_gfx_voice_start(beep, 255, 0);
    __nexa_gfx_mix_pump();
    int full = peak();
    __nexa_gfx_stop(0);
    eq("a full voice keeps the file's peak", full, 20000);

    __nexa_cap.clear();
    __nexa_gfx_voice_start(beep, 128, 0);
    __nexa_gfx_mix_pump();
    int half = peak();
    __nexa_gfx_stop(0);
    check("half the voice volume is half the peak",
          half * 2 >= full - 400 && half * 2 <= full + 400);

    // The two gains multiply: half master times half voice is a quarter.
    __nexa_gfx_volume_set(128);
    __nexa_cap.clear();
    __nexa_gfx_voice_start(beep, 128, 0);
    __nexa_gfx_mix_pump();
    int quarter = peak();
    __nexa_gfx_stop(0);
    __nexa_gfx_volume_set(255);
    check("master and voice volume multiply",
          quarter * 4 >= full - 800 && quarter * 4 <= full + 800);

    // --- looping ----------------------------------------------------------
    __nexa_cap.clear();
    int lv = __nexa_gfx_voice_start(beep, 255, 1);
    __nexa_gfx_mix_pump();
    eq("a loop is still playing after its end", live_voices(), 1);
    eq("and it is the same voice", __nexa_voices[0].id, lv);
    // A loop never runs out, so only the zero crossings of the sine are silent
    // -- against the ~2100 of the one-shot above.
    check("a loop fills the whole pump", nonzero() > 4300);
    eq("gfx.stop(voice) stops a loop too", __nexa_gfx_stop(lv), 1);
    eq("and the slot is free again", live_voices(), 0);

    // gfx.close() closes the stream, so it takes the voices with it.
    __nexa_gfx_voice_start(beep, 255, 1);
    __nexa_gfx_sound_reset();
    eq("closing the window stops every voice", live_voices(), 0);

    // --- mixing -----------------------------------------------------------
    // Two voices of the same sound started together sum. At half volume each
    // that lands back on the full peak, which keeps the sum clear of the
    // saturation the next case is about.
    __nexa_cap.clear();
    __nexa_gfx_voice_start(beep, 128, 0);
    __nexa_gfx_voice_start(beep, 128, 0);
    __nexa_gfx_mix_pump();
    int two = peak();
    __nexa_gfx_stop(0);
    check("two voices sum", two >= half * 2 - 400 && two <= half * 2 + 400);

    // Sixteen of them in phase reach 320000, which must clip rather than wrap.
    __nexa_cap.clear();
    for (int i = 0; i < NEXA_VOICES; i++) __nexa_gfx_voice_start(beep, 255, 0);
    __nexa_gfx_mix_pump();
    bool hitTop = false, hitBottom = false, escaped = false;
    for (size_t i = 0; i < __nexa_cap.size(); i++) {
        if (__nexa_cap[i] == 32767) hitTop = true;
        if (__nexa_cap[i] == -32768) hitBottom = true;
        if (__nexa_cap[i] > 32767 || __nexa_cap[i] < -32768) escaped = true;
    }
    check("a loud mix saturates at the top", hitTop);
    check("a loud mix saturates at the bottom", hitBottom);
    check("and never leaves the 16-bit range", !escaped);
    __nexa_gfx_stop(0);

    // A sound at a different rate plays for the length it should: 200 frames
    // at 11025 Hz is 0.0181 s, which is 800 samples of a 44100 Hz stream.
    __nexa_cap.clear();
    __nexa_gfx_voice_start(st, 255, 0);
    __nexa_gfx_mix_pump();
    long long walked = __nexa_voices[0].id == 0 ? 0 : 1;
    eq("a short sound ends inside one pump", walked, 0);
    __nexa_gfx_stop(0);

    if (failures == 0) {
        std::printf("gfx sound semantics ok\n");
        return 0;
    }
    std::printf("gfx sound semantics: %d failure(s)\n", failures);
    return 1;
}
