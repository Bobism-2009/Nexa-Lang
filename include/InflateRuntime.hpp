#pragma once
// A raw DEFLATE (RFC 1951) decoder for the data NexaC bakes into a program in
// compressed form: std/ui's two fonts, std/gfx3d's SPIR-V. It is the small
// canonical-Huffman decoder of zlib's contrib/puff, cut to what trusted,
// compiled-in data needs: every read and write is still bounds-checked, so a
// damaged stream fails rather than overruns, but nothing is reported beyond
// "it did not decode".
//
// Emitted by whichever runtime needs it, inside a guard, so a program that
// carries both the fonts and the shaders compiles it once.

#include <string>

namespace nexa {

inline std::string inflateRuntimeCpp() {
    return R"NEXA_INFLATE(
#ifndef NEXA_INFLATE_DEFINED
#define NEXA_INFLATE_DEFINED
struct __nexa_inf_s {
    const unsigned char* in; size_t inlen, inpos;
    unsigned char* out; size_t outlen, outpos;
    unsigned bitbuf, bitcnt; int bad;
};
struct __nexa_inf_huff { short count[16]; short symbol[288]; };

static int __nexa_inf_bits(__nexa_inf_s* s, unsigned need) {
    unsigned long val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) { s->bad = 1; return 0; }
        val |= (unsigned long)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = (unsigned)(val >> need);
    s->bitcnt -= need;
    return (int)(val & ((1ul << need) - 1));
}

static int __nexa_inf_decode(__nexa_inf_s* s, const __nexa_inf_huff* h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= __nexa_inf_bits(s, 1);
        const int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
        if (s->bad) break;
    }
    return -1;
}

static void __nexa_inf_build(__nexa_inf_huff* h, const short* length, int n) {
    short offs[16];
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[length[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = (short)(offs[i] + h->count[i]);
    for (int i = 0; i < n; i++) if (length[i]) h->symbol[offs[length[i]]++] = (short)i;
}

static int __nexa_inf_codes(__nexa_inf_s* s, const __nexa_inf_huff* lc, const __nexa_inf_huff* dc) {
    static const short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const unsigned char lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                           3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const unsigned short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                             257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                             8193, 12289, 16385, 24577};
    static const unsigned char dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                           7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = __nexa_inf_decode(s, lc);
        if (sym < 0 || s->bad) return 0;
        if (sym == 256) return 1;
        if (sym < 256) {
            if (s->outpos >= s->outlen) return 0;
            s->out[s->outpos++] = (unsigned char)sym;
            continue;
        }
        sym -= 257;
        if (sym >= 29) return 0;
        size_t len = (size_t)lbase[sym] + (size_t)__nexa_inf_bits(s, lext[sym]);
        sym = __nexa_inf_decode(s, dc);
        if (sym < 0 || sym >= 30 || s->bad) return 0;
        const size_t dist = (size_t)dbase[sym] + (size_t)__nexa_inf_bits(s, dext[sym]);
        if (s->bad || dist > s->outpos || len > s->outlen - s->outpos) return 0;
        unsigned char* o = s->out + s->outpos;
        s->outpos += len;
        while (len--) { *o = *(o - dist); o++; }
    }
}

// Decodes a raw DEFLATE stream of exactly outlen bytes into out. 1 on success.
static int __nexa_inflate(const unsigned char* in, size_t inlen, unsigned char* out, size_t outlen) {
    __nexa_inf_s s = {in, inlen, 0, out, outlen, 0, 0, 0, 0};
    __nexa_inf_huff lc, dc;
    short lengths[320];
    int last;
    do {
        last = __nexa_inf_bits(&s, 1);
        const int type = __nexa_inf_bits(&s, 2);
        if (s.bad) return 0;
        if (type == 0) {
            s.bitbuf = 0; s.bitcnt = 0;
            if (s.inlen - s.inpos < 4) return 0;
            const size_t len = (size_t)s.in[s.inpos] | ((size_t)s.in[s.inpos + 1] << 8);
            s.inpos += 4;
            if (len > s.inlen - s.inpos || len > s.outlen - s.outpos) return 0;
            for (size_t i = 0; i < len; i++) s.out[s.outpos++] = s.in[s.inpos++];
            continue;
        }
        if (type == 1) {
            int i = 0;
            for (; i < 144; i++) lengths[i] = 8;
            for (; i < 256; i++) lengths[i] = 9;
            for (; i < 280; i++) lengths[i] = 7;
            for (; i < 288; i++) lengths[i] = 8;
            __nexa_inf_build(&lc, lengths, 288);
            for (i = 0; i < 30; i++) lengths[i] = 5;
            __nexa_inf_build(&dc, lengths, 30);
        } else if (type == 2) {
            static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            const int nlen = __nexa_inf_bits(&s, 5) + 257;
            const int ndist = __nexa_inf_bits(&s, 5) + 1;
            const int ncode = __nexa_inf_bits(&s, 4) + 4;
            if (nlen > 286 || ndist > 30 || s.bad) return 0;
            int i = 0;
            for (; i < ncode; i++) lengths[order[i]] = (short)__nexa_inf_bits(&s, 3);
            for (; i < 19; i++) lengths[order[i]] = 0;
            __nexa_inf_build(&lc, lengths, 19);
            for (i = 0; i < nlen + ndist;) {
                int sym = __nexa_inf_decode(&s, &lc);
                if (sym < 0 || s.bad) return 0;
                if (sym < 16) { lengths[i++] = (short)sym; continue; }
                short v = 0;
                int rep;
                if (sym == 16) {
                    if (i == 0) return 0;
                    v = lengths[i - 1];
                    rep = 3 + __nexa_inf_bits(&s, 2);
                } else if (sym == 17) {
                    rep = 3 + __nexa_inf_bits(&s, 3);
                } else {
                    rep = 11 + __nexa_inf_bits(&s, 7);
                }
                if (i + rep > nlen + ndist) return 0;
                while (rep--) lengths[i++] = v;
            }
            __nexa_inf_build(&lc, lengths, nlen);
            __nexa_inf_build(&dc, lengths + nlen, ndist);
        } else {
            return 0;
        }
        if (!__nexa_inf_codes(&s, &lc, &dc)) return 0;
    } while (!last);
    return s.outpos == s.outlen;
}
#endif
)NEXA_INFLATE";
}

}  // namespace nexa
