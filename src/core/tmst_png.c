/* TMST core: PNG writer (tmst_png.h). */
#include "tmst_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- growing output ---- */
typedef struct { uint8_t *p; size_t n, cap; int bad; } Buf;
static void put(Buf *b, const void *d, size_t n) {
    if (b->bad) return;
    if (b->n + n > b->cap) {
        size_t cap = b->cap ? b->cap : 65536;
        while (cap < b->n + n) cap *= 2;
        uint8_t *p = realloc(b->p, cap);
        if (!p) { b->bad = 1; return; }
        b->p = p; b->cap = cap;
    }
    memcpy(b->p + b->n, d, n); b->n += n;
}
static void put32(Buf *b, uint32_t v) { uint8_t q[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; put(b, q, 4); }

/* ---- deflate, one fixed-Huffman block ---- */
typedef struct { Buf *out; uint32_t bits; int count; } Bits;
static void bits(Bits *w, uint32_t v, int n) {            /* LSB first */
    w->bits |= v << w->count; w->count += n;
    while (w->count >= 8) { uint8_t c = (uint8_t)w->bits; put(w->out, &c, 1); w->bits >>= 8; w->count -= 8; }
}
static uint32_t reverse(uint32_t v, int n) { uint32_t r = 0; while (n--) { r = r << 1 | (v & 1); v >>= 1; } return r; }
/* A literal / length symbol in the fixed code (Huffman codes go MSB first). */
static void symbol(Bits *w, int s) {
    if (s < 144) bits(w, reverse(0x30 + s, 8), 8);
    else if (s < 256) bits(w, reverse(0x190 + s - 144, 9), 9);
    else if (s < 280) bits(w, reverse(s - 256, 7), 7);
    else bits(w, reverse(0xC0 + s - 280, 8), 8);
}
static const uint16_t len_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115,
                                      131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
                                       2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static void match(Bits *w, int len, int dist) {
    int i = 28;
    while (len_base[i] > len) i--;
    symbol(w, 257 + i);
    if (len_extra[i]) bits(w, (uint32_t)(len - len_base[i]), len_extra[i]);
    int d = 29;
    while (dist_base[d] > dist) d--;
    bits(w, reverse((uint32_t)d, 5), 5);
    if (dist_extra[d]) bits(w, (uint32_t)(dist - dist_base[d]), dist_extra[d]);
}

enum { WINDOW = 32768, HASH_BITS = 15, MAX_CHAIN = 24, MIN_MATCH = 3, MAX_MATCH = 258, GOOD = 64 };
static unsigned hash3(const uint8_t *p) { return ((unsigned)p[0] << 10 ^ (unsigned)p[1] << 5 ^ p[2]) & ((1u << HASH_BITS) - 1); }

static int deflate_fixed(const uint8_t *d, size_t n, Buf *out) {
    int32_t *head = malloc(sizeof(int32_t) << HASH_BITS), *prev = malloc(sizeof(int32_t) * WINDOW);
    if (!head || !prev) { free(head); free(prev); return 0; }
    for (size_t i = 0; i < (1u << HASH_BITS); i++) head[i] = -1;
    Bits w = {out, 0, 0};
    bits(&w, 1, 1); bits(&w, 1, 2);                    /* the last block, fixed Huffman */
    size_t i = 0;
    while (i < n) {
        int best = 0, best_d = 0;
        if (i + MIN_MATCH <= n) {
            unsigned h = hash3(d + i);
            int32_t c = head[h];
            size_t limit = n - i < MAX_MATCH ? n - i : MAX_MATCH;
            for (int chain = 0; c >= 0 && i - (size_t)c <= WINDOW && chain < MAX_CHAIN; chain++) {
                const uint8_t *a = d + c, *b = d + i;
                if (a[best] == b[best]) {
                    size_t l = 0;
                    while (l < limit && a[l] == b[l]) l++;
                    if ((int)l > best) { best = (int)l; best_d = (int)(i - (size_t)c); if (l >= GOOD || l == limit) break; }
                }
                int32_t nx = prev[c % WINDOW];
                if (nx >= c) break;
                c = nx;
            }
        }
        size_t step = best >= MIN_MATCH ? (size_t)best : 1;
        if (best >= MIN_MATCH) match(&w, best, best_d); else symbol(&w, d[i]);
        for (size_t k = 0; k < step; k++, i++)
            if (i + MIN_MATCH <= n) { unsigned h = hash3(d + i); prev[i % WINDOW] = head[h]; head[h] = (int32_t)i; }
    }
    symbol(&w, 256);
    if (w.count) bits(&w, 0, 8 - w.count);
    free(head); free(prev);
    return !out->bad;
}

/* ---- checksums ---- */
static uint32_t crc_table[256];
static void crc_init(void) {
    if (crc_table[1]) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n) {
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}
static uint32_t adler(const uint8_t *p, size_t n) {
    uint32_t a = 1, b = 0;
    while (n) {
        size_t k = n < 5552 ? n : 5552;
        n -= k;
        while (k--) { a += *p++; b += a; }
        a %= 65521; b %= 65521;
    }
    return b << 16 | a;
}
static void chunk(Buf *b, const char *type, const uint8_t *d, size_t n) {
    put32(b, (uint32_t)n);
    size_t at = b->n;
    put(b, type, 4); put(b, d, n);
    if (!b->bad) put32(b, crc(0, b->p + at, n + 4));
}

/* ---- rows ---- */
static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

int tmst_png_encode(const uint32_t *argb, int w, int h, int pitch, uint8_t **out, size_t *size) {
    *out = NULL; *size = 0;
    if (!argb || w <= 0 || h <= 0 || pitch < w) return 0;
    size_t row = (size_t)w * 3, n = (size_t)h * (row + 1);
    uint8_t *raw = malloc(n), *line = malloc(row * 2), *trial = malloc(row);
    if (!raw || !line || !trial) { free(raw); free(line); free(trial); return 0; }
    uint8_t *cur = line, *up = line + row;
    memset(up, 0, row);
    for (int y = 0; y < h; y++) {
        const uint32_t *src = argb + (size_t)y * (size_t)pitch;
        for (int x = 0; x < w; x++) { cur[3 * x] = (uint8_t)(src[x] >> 16); cur[3 * x + 1] = (uint8_t)(src[x] >> 8); cur[3 * x + 2] = (uint8_t)src[x]; }
        uint8_t *dst = raw + (size_t)y * (row + 1);
        unsigned long best_sum = ~0ul;
        for (int f = 0; f < 5; f++) {
            if (f == 3) continue;                         /* Average: rarely the best, left out */
            unsigned long sum = 0;
            for (size_t i = 0; i < row; i++) {
                int a = i >= 3 ? cur[i - 3] : 0, b = up[i], c = i >= 3 ? up[i - 3] : 0, v = cur[i];
                int p = f == 0 ? v : f == 1 ? v - a : f == 2 ? v - b : v - paeth(a, b, c);
                trial[i] = (uint8_t)p;
                sum += (unsigned long)abs((int8_t)trial[i]);
            }
            if (sum < best_sum) { best_sum = sum; dst[0] = (uint8_t)f; memcpy(dst + 1, trial, row); }
        }
        uint8_t *t = up; up = cur; cur = t;
    }
    free(line); free(trial);
    crc_init();
    Buf z = {0}, png = {0};
    uint8_t zh[2] = {0x78, 0x01};
    put(&z, zh, 2);
    int ok = deflate_fixed(raw, n, &z);
    put32(&z, adler(raw, n));
    free(raw);
    if (!ok || z.bad) { free(z.p); return 0; }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    uint8_t ihdr[13] = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                        (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0};
    put(&png, sig, 8);
    chunk(&png, "IHDR", ihdr, 13);
    chunk(&png, "IDAT", z.p, z.n);
    chunk(&png, "IEND", NULL, 0);
    free(z.p);
    if (png.bad) { free(png.p); return 0; }
    *out = png.p; *size = png.n;
    return 1;
}

int tmst_png_write(const char *path, const uint32_t *argb, int w, int h, int pitch) {
    uint8_t *data; size_t n;
    if (!tmst_png_encode(argb, w, h, pitch, &data, &n)) return 0;
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(data, 1, n, f) == n;
    if (f && fclose(f)) ok = 0;
    free(data);
    if (!ok) remove(path);
    return ok;
}
