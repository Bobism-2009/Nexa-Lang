"""Packs std/ui's two fonts into include/UiFontData.hpp, and unpacks them again.

    python tools/pack_ui_fonts.py pack <Regular.ttf> <SemiBold.ttf>
    python tools/pack_ui_fonts.py unpack <dir>      (writes regular.ttf, semibold.ttf)
    python tools/pack_ui_fonts.py check             (unpack, repack, compare)

The fonts go into the program as one raw DEFLATE stream that the runtime
inflates on first use (include/InflateRuntime.hpp). Before compressing, each
font's flat 'kern' pair list -- most of its bytes -- is turned on its side:
the left glyphs, then the right glyphs, then the values, each as a difference
from the pair before (the right glyph restarting from 0 whenever the left one
changes). The list is sorted, so the differences are small and repeat, and
DEFLATE takes 29KB of pairs down to under 3KB. The runtime turns it back, so
stb_truetype reads exactly the bytes it would have read from the .ttf.

Unpacking gives back the same .ttf files, byte for byte.
"""
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(HERE, '..', 'include', 'UiFontData.hpp')
NAMES = ['regular', 'semibold']


def kern_pairs(ttf):
    """(offset of the first pair, number of pairs) of the font's format-0 kern table."""
    ntab = struct.unpack('>H', ttf[4:6])[0]
    for i in range(ntab):
        tag, _, off, ln = struct.unpack('>4sIII', ttf[12 + 16 * i:28 + 16 * i])
        if tag == b'kern':
            ver, nsub = struct.unpack('>HH', ttf[off:off + 4])
            fmt_cov = struct.unpack('>H', ttf[off + 8:off + 10])[0]
            assert ver == 0 and nsub == 1 and (fmt_cov >> 8) == 0, 'one format-0 kern subtable'
            n = struct.unpack('>H', ttf[off + 10:off + 12])[0]
            assert off + 18 + 6 * n <= off + ln
            return off + 18, n
    return 0, 0


def to_columns(ttf):
    at, n = kern_pairs(ttf)
    pairs = [struct.unpack('>HHh', ttf[at + 6 * i:at + 6 * i + 6]) for i in range(n)]
    L = bytearray(); R = bytearray(); V = bytearray()
    pl = pr = 0
    for l, r, v in pairs:
        if l != pl:
            pr = 0
        L += struct.pack('<H', (l - pl) & 0xffff)
        R += struct.pack('<H', (r - pr) & 0xffff)
        V += struct.pack('<h', v)
        pl, pr = l, r
    return ttf[:at] + bytes(L + R + V) + ttf[at + 6 * n:], at, n


def from_columns(data, at, n):
    col = data[at:at + 6 * n]
    out = bytearray()
    l = r = 0
    for i in range(n):
        dl = struct.unpack('<H', col[2 * i:2 * i + 2])[0]
        dr = struct.unpack('<H', col[2 * n + 2 * i:2 * n + 2 * i + 2])[0]
        v = col[4 * n + 2 * i:4 * n + 2 * i + 2]
        if dl:
            r = 0
        l = (l + dl) & 0xffff
        r = (r + dr) & 0xffff
        out += struct.pack('>HH', l, r) + v[1:2] + v[0:1]
    return data[:at] + bytes(out) + data[at + 6 * n:]


def deflate(b):
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return c.compress(b) + c.flush()


def c_string(b, per_line=40):
    lines = []
    for i in range(0, len(b), per_line):
        lines.append('"' + ''.join('\\x%02x' % x for x in b[i:i + per_line]) + '"')
    return '\n'.join(lines)


def read_header():
    return open(HEADER, 'rb').read().decode('utf-8')


def pack(ttfs):
    text = read_header()
    start = text.index('inline std::string uiFontDataCpp()')
    prefix = text[:text.index('#include <string>')]
    blobs, meta = b'', []
    for t in ttfs:
        cols, at, n = to_columns(t)
        assert from_columns(cols, at, n) == t
        blobs += cols
        meta.append((len(t), at, n))
    z = deflate(blobs)
    assert zlib.decompress(z, -15) == blobs
    body = '''#include <string>

#include "InflateRuntime.hpp"

namespace nexa {

// The two fonts as C++ source, for a generated program that uses std/ui:
// one compressed stream, and the call that opens it. Written by
// tools/pack_ui_fonts.py -- edit the fonts there, not here.
inline std::string uiFontDataCpp() {
    std::string out = inflateRuntimeCpp();
    out += R"NEXAFONT(
// Inter 4.1, SIL Open Font License 1.1 -- Copyright (c) 2016 The Inter Project
// Authors (https://github.com/rsms/inter). Regular and SemiBold, subset.
static const unsigned char __nexa_ui_font_z[%d + 1] =
%s;
)NEXAFONT";
    out += R"NEXAFONT(
// Regular then SemiBold: size, where the kern pairs start, how many there are.
static const unsigned __nexa_ui_font_meta[2][3] = {{%d, %d, %d}, {%d, %d, %d}};

// Both fonts, inflated into one block that lives as long as the program, with
// each kern list turned back from columns into the pairs stb_truetype reads.
// Null when the block cannot be had, and then ui draws no text.
static const unsigned char* __nexa_ui_fonts_unpack(const unsigned char** semibold) {
    const size_t total = (size_t)__nexa_ui_font_meta[0][0] + __nexa_ui_font_meta[1][0];
    unsigned char* all = (unsigned char*)malloc(total);
    if (!all || !__nexa_inflate(__nexa_ui_font_z, sizeof(__nexa_ui_font_z) - 1, all, total)) {
        free(all);
        return nullptr;
    }
    unsigned char* font = all;
    for (int f = 0; f < 2; f++) {
        const unsigned n = __nexa_ui_font_meta[f][2];
        unsigned char* p = font + __nexa_ui_font_meta[f][1];
        unsigned char* col = (unsigned char*)malloc((size_t)n * 6);
        if (!col) { free(all); return nullptr; }
        memcpy(col, p, (size_t)n * 6);
        unsigned l = 0, r = 0;
        for (unsigned i = 0; i < n; i++) {
            const unsigned dl = col[2 * i] | (col[2 * i + 1] << 8);
            const unsigned dr = col[2 * n + 2 * i] | (col[2 * n + 2 * i + 1] << 8);
            if (dl) r = 0;
            l = (l + dl) & 0xffff;
            r = (r + dr) & 0xffff;
            unsigned char* o = p + 6 * i;
            o[0] = (unsigned char)(l >> 8); o[1] = (unsigned char)l;
            o[2] = (unsigned char)(r >> 8); o[3] = (unsigned char)r;
            o[4] = col[4 * n + 2 * i + 1]; o[5] = col[4 * n + 2 * i];
        }
        free(col);
        font += __nexa_ui_font_meta[f][0];
    }
    *semibold = all + __nexa_ui_font_meta[0][0];
    return all;
}
)NEXAFONT";
    return out;
}

}  // namespace nexa
''' % (len(z), c_string(z), *meta[0], *meta[1])
    out = prefix + body
    open(HEADER, 'wb').write(out.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8'))
    print('packed %d + %d bytes of font into %d' % (meta[0][0], meta[1][0], len(z)))


def unpack_header():
    text = read_header()
    m = re.search(r'__nexa_ui_font_z\[(\d+) \+ 1\] =\s*(.*?);', text, re.S)
    z = bytes(int(h, 16) for h in re.findall(r'\\x([0-9a-f]{2})', m.group(2)))
    assert len(z) == int(m.group(1))
    meta = [tuple(int(x) for x in g) for g in re.findall(r'\{(\d+), (\d+), (\d+)\}', text[text.index('__nexa_ui_font_meta[2][3]'):])[:2]]
    blobs = zlib.decompress(z, -15)
    fonts, at = [], 0
    for size, kat, n in meta:
        fonts.append(from_columns(blobs[at:at + size], kat, n))
        at += size
    return fonts


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'pack' and len(sys.argv) == 4:
        pack([open(p, 'rb').read() for p in sys.argv[2:4]])
    elif cmd == 'unpack' and len(sys.argv) == 3:
        for name, b in zip(NAMES, unpack_header()):
            open(os.path.join(sys.argv[2], name + '.ttf'), 'wb').write(b)
    elif cmd == 'check':
        fonts = unpack_header()
        before = read_header()
        pack(fonts)
        assert read_header() == before, 'repacking changed the header'
        print('ok: the header unpacks and repacks to itself')
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == '__main__':
    main()
