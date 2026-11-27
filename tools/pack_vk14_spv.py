"""Packs the Vulkan renderer's SPIR-V into include/nexa_vk14_spv.inc, and back.

    python tools/pack_vk14_spv.py pack <dir>     (reads <dir>/<name>.spv for every name below)
    python tools/pack_vk14_spv.py unpack <dir>   (writes them out again)
    python tools/pack_vk14_spv.py check          (unpack, repack, compare)

Compile the GLSL in include/vk14 to .spv (glslc or glslangValidator, Vulkan
1.4 target), name the files as below, and pack. Then rewrite the embed:
sh Tests/vk14_embed_cases.sh --write.

What goes in: every shader with its debug instructions taken out (OpSource,
OpName, OpMemberName, OpString, OpLine and the like -- nothing a driver reads),
all seventeen end to end, the words split into four planes of bytes (every
lowest byte, then every second byte, ...), and that DEFLATEd. SPIR-V's high
bytes are mostly zero and its opcodes repeat, so a plane at a time compresses
much better than word by word: 54KB becomes under 12KB. The renderer inflates
it once, on the first shader it makes (spv() in the .inc).

Unpacking gives the shaders back as packed: without their debug names.
"""
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
INC = os.path.join(HERE, '..', 'include', 'nexa_vk14_spv.inc')
# The order is the renderer's: the scene's thirteen are made in a loop over
# the first thirteen; the last four are the swapchain's own two pipelines.
NAMES = ['kSpvMeshVs', 'kSpvMeshFs', 'kSpvDepthVs', 'kSpvMatVs', 'kSpvMatFs', 'kSpvSkyVs',
         'kSpvSkyFs', 'kSpvPartVs', 'kSpvPartFs', 'kSpvBlitVs', 'kSpvBlitFs', 'kSpvTexVs',
         'kSpvTexFs', 'kVs', 'kFs', 'kOvVs', 'kOvFs']
DEBUG_OPS = {2, 3, 4, 5, 6, 7, 8, 317, 330}  # SourceContinued, Source, SourceExtension,
                                             # Name, MemberName, String, Line, NoLine, ModuleProcessed


def strip(words):
    assert words[0] == 0x07230203, 'not SPIR-V'
    out, i = list(words[:5]), 5
    while i < len(words):
        op, n = words[i] & 0xffff, words[i] >> 16
        assert n > 0 and i + n <= len(words), 'bad instruction'
        if op not in DEBUG_OPS:
            out += words[i:i + n]
        i += n
    return out


def deflate(b):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return c.compress(b) + c.flush()


def c_string(b, per_line=40):
    return '\n'.join('"' + ''.join('\\x%02x' % x for x in b[i:i + per_line]) + '"'
                     for i in range(0, len(b), per_line))


def pack(shaders):
    at, words = [0], []
    for w in shaders:
        words += w
        at.append(len(words))
    raw = struct.pack('<%dI' % len(words), *words)
    planes = b''.join(raw[k::4] for k in range(4))
    z = deflate(planes)
    enum = ',\n    '.join(', '.join(NAMES[i:i + 6]) for i in range(0, len(NAMES), 6))
    ats = ',\n    '.join(', '.join(str(a) for a in at[i:i + 10]) for i in range(0, len(at), 10))
    text = '''// The renderer's seventeen shaders, compiled from include/vk14 and packed by
// tools/pack_vk14_spv.py: debug names out, the words split into byte planes,
// DEFLATEd. %d words of SPIR-V in %d bytes. Edit the GLSL and repack; this
// file is written, not edited. Needs __nexa_inflate (InflateRuntime.hpp),
// which NexaC writes ahead of the renderer.
static const unsigned char kSpvZ[%d + 1] =
%s;

enum {
    %s,
    kSpvCount
};

// Where each shader starts, in words; the last is the end of the last one.
static const uint32_t kSpvAt[kSpvCount + 1] = {
    %s
};

// All seventeen, inflated on first use and kept: the scene is made again when
// the device is, and each time it needs the same words. Null if they cannot
// be had, and then no shader module is made and the renderer stays closed.
inline const uint32_t* spv_all() {
    static uint32_t* words = nullptr;
    if (words) return words;
    const size_t n = kSpvAt[kSpvCount];
    unsigned char* planes = (unsigned char*)malloc(n * 4);
    uint32_t* w = (uint32_t*)malloc(n * 4);
    if (planes && w && __nexa_inflate(kSpvZ, sizeof(kSpvZ) - 1, planes, n * 4)) {
        for (size_t i = 0; i < n; ++i)
            w[i] = (uint32_t)planes[i] | ((uint32_t)planes[n + i] << 8) |
                   ((uint32_t)planes[2 * n + i] << 16) | ((uint32_t)planes[3 * n + i] << 24);
        words = w;
        w = nullptr;
    }
    free(planes);
    free(w);
    return words;
}

inline const uint32_t* spv(int which) {
    const uint32_t* all = spv_all();
    return all ? all + kSpvAt[which] : nullptr;
}

inline size_t spv_words(int which) { return kSpvAt[which + 1] - kSpvAt[which]; }
''' % (len(words), len(z), len(z), c_string(z), enum, ats)
    open(INC, 'wb').write(text.replace('\n', '\r\n').encode('ascii'))
    print('packed %d shaders, %d bytes of SPIR-V, into %d' % (len(shaders), len(raw), len(z)))


def unpack_inc():
    text = open(INC, 'rb').read().decode('ascii')
    m = re.search(r'kSpvZ\[(\d+) \+ 1\] =\s*(.*?);', text, re.S)
    z = bytes(int(h, 16) for h in re.findall(r'\\x([0-9a-f]{2})', m.group(2)))
    assert len(z) == int(m.group(1))
    body = text[text.index('kSpvAt[kSpvCount + 1] = {'):]
    at = [int(x) for x in re.findall(r'\d+', body[body.index('{'):body.index('}')])]
    planes = zlib.decompress(z, -15)
    n = len(planes) // 4
    words = [planes[i] | planes[n + i] << 8 | planes[2 * n + i] << 16 | planes[3 * n + i] << 24
             for i in range(n)]
    return [words[at[i]:at[i + 1]] for i in range(len(NAMES))]


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'pack' and len(sys.argv) == 3:
        shaders = []
        for name in NAMES:
            b = open(os.path.join(sys.argv[2], name + '.spv'), 'rb').read()
            shaders.append(strip(list(struct.unpack('<%dI' % (len(b) // 4), b))))
        pack(shaders)
    elif cmd == 'unpack' and len(sys.argv) == 3:
        for name, w in zip(NAMES, unpack_inc()):
            open(os.path.join(sys.argv[2], name + '.spv'), 'wb').write(struct.pack('<%dI' % len(w), *w))
    elif cmd == 'check':
        before = open(INC, 'rb').read()
        pack([strip(w) for w in unpack_inc()])
        assert open(INC, 'rb').read() == before, 'repacking changed the file'
        print('ok: the .inc unpacks and repacks to itself')
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == '__main__':
    main()
