// A fake libasound.so.2 (BOB-41), so that the ALSA backend behind gfx.audio()
// can be run and not merely compiled.
//
// The backend in include/GfxRuntime.hpp reaches ALSA through dlopen and
// hand-declared function pointers, which is what makes this possible: there is
// no libasound-dev to match and no header to agree with, so a shared object
// exporting these eight names by these eight signatures IS libasound as far as
// a generated program can tell. Tests/gfx_alsa_cases.sh builds this, puts it on
// LD_LIBRARY_PATH under the name the backend asks for, and runs real gfx
// programs against it.
//
// Two things come out of a run. The program's own output says what the public
// API returned -- gfx.audio() 1/0, the sample counts gfx.audio_queued()
// reports. The trace file named by NEXA_ALSA_STUB_TRACE says what the backend
// actually asked the device for, one line per call, which is the only way to
// check the things the API cannot show: that the stream is opened non-blocking
// on "default", that it is configured S16_LE mono at the requested rate with a
// tenth of a second of latency, how the sample buffer is cut into writes, and
// that an underrun is recovered rather than dropped.
//
// Behaviour is steered by environment variables so one build covers every case:
//
//   NEXA_ALSA_STUB_TRACE=<path>   write the call trace here
//   NEXA_ALSA_STUB_FAIL_OPEN=1    snd_pcm_open fails (no device)
//   NEXA_ALSA_STUB_FAIL_PARAMS=1  snd_pcm_open works, snd_pcm_set_params fails
//   NEXA_ALSA_STUB_CAP=<frames>   how many frames the device holds before it
//                                 says EAGAIN (default: effectively unbounded)
//   NEXA_ALSA_STUB_XRUN=1         the first snd_pcm_writei underruns (-EPIPE)
//
// Building with -DNEXA_ALSA_STUB_PARTIAL leaves snd_pcm_wait out, which is a
// library calling itself libasound that is not one -- the case where the
// backend has to refuse the whole thing rather than call half of it.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Errno values as ALSA returns them: negated, and these three are the same on
// every Linux architecture.
#define STUB_EAGAIN 11
#define STUB_EINVAL 22
#define STUB_ENODEV 19
#define STUB_EPIPE 32

namespace {

struct Pcm {
    unsigned magic;
    // The device's own buffer: what has been accepted and not yet played. In a
    // test nothing plays, so this only ever grows -- which is what makes
    // snd_pcm_delay a running total of everything written, and therefore
    // exactly what gfx.audio_queued() should be reporting.
    std::vector<short> queued;
    size_t capacity;
    bool xrun_pending;
};

const unsigned kMagic = 0x4e455841u;  // "NEXA"

FILE* traceFile() {
    static FILE* f = NULL;
    static bool opened = false;
    if (!opened) {
        opened = true;
        const char* path = getenv("NEXA_ALSA_STUB_TRACE");
        if (path && *path) f = fopen(path, "w");
    }
    return f;
}

void trace(const char* fmt, ...) {
    FILE* f = traceFile();
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

bool envOn(const char* name) {
    const char* v = getenv(name);
    return v && *v && v[0] != '0';
}

// A handle that is not one of ours means the backend passed something it did
// not get from snd_pcm_open, which is worth failing loudly over.
Pcm* handle(void* p) {
    Pcm* pcm = static_cast<Pcm*>(p);
    if (!pcm || pcm->magic != kMagic) {
        trace("BAD-HANDLE");
        return NULL;
    }
    return pcm;
}

}  // namespace

extern "C" {

int snd_pcm_open(void** pcmp, const char* name, int stream, int mode) {
    trace("open name=%s stream=%d mode=%d", name ? name : "(null)", stream, mode);
    if (!pcmp) return -STUB_EINVAL;
    if (envOn("NEXA_ALSA_STUB_FAIL_OPEN")) {
        trace("open failed");
        *pcmp = NULL;
        return -STUB_ENODEV;
    }
    Pcm* pcm = new Pcm();
    pcm->magic = kMagic;
    pcm->capacity = (size_t)1 << 30;
    const char* cap = getenv("NEXA_ALSA_STUB_CAP");
    if (cap && *cap) pcm->capacity = (size_t)strtoul(cap, NULL, 10);
    pcm->xrun_pending = envOn("NEXA_ALSA_STUB_XRUN");
    *pcmp = pcm;
    return 0;
}

int snd_pcm_set_params(void* p, int format, int access, unsigned int channels,
                       unsigned int rate, int soft_resample, unsigned int latency) {
    trace("set_params format=%d access=%d channels=%u rate=%u resample=%d latency=%u",
          format, access, channels, rate, soft_resample, latency);
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    if (envOn("NEXA_ALSA_STUB_FAIL_PARAMS")) {
        trace("set_params failed");
        return -STUB_EINVAL;
    }
    return 0;
}

long snd_pcm_writei(void* p, const void* buf, unsigned long frames) {
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    if (pcm->xrun_pending) {
        pcm->xrun_pending = false;
        trace("writei frames=%lu -> underrun", frames);
        return -STUB_EPIPE;
    }
    size_t room = pcm->capacity > pcm->queued.size() ? pcm->capacity - pcm->queued.size() : 0;
    if (room == 0) {
        trace("writei frames=%lu -> full", frames);
        return -STUB_EAGAIN;
    }
    size_t take = frames < room ? (size_t)frames : room;
    const short* s = static_cast<const short*>(buf);
    pcm->queued.insert(pcm->queued.end(), s, s + take);
    trace("writei frames=%lu -> %lu", frames, (unsigned long)take);
    return (long)take;
}

#ifndef NEXA_ALSA_STUB_PARTIAL
int snd_pcm_wait(void* p, int timeout) {
    trace("wait timeout=%d", timeout);
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    // Nothing is draining this device, so waiting for room always times out.
    // That is the case worth covering: the backend must give up and move on
    // rather than hold the program until the device it is waiting for frees
    // space it never will.
    return 0;
}
#endif

int snd_pcm_delay(void* p, long* delayp) {
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    if (delayp) *delayp = (long)pcm->queued.size();
    trace("delay -> %ld", (long)pcm->queued.size());
    return 0;
}

int snd_pcm_recover(void* p, int err, int silent) {
    trace("recover err=%d silent=%d", err, silent);
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    return 0;
}

int snd_pcm_drain(void* p) {
    trace("drain");
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    return 0;
}

int snd_pcm_close(void* p) {
    trace("close");
    Pcm* pcm = handle(p);
    if (!pcm) return -STUB_EINVAL;
    delete pcm;
    return 0;
}

}  // extern "C"
