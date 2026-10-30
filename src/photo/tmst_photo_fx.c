/* tmst.photo-mode: filters, frames and depth of field (tmst_photo_fx.h).
 * Screen units: the fight's 368 x 480 (a pixel about 1.7 times as wide as
 * tall), the wide screen's sides from -margin to 368 + margin. Colours of
 * the primitives 0xBBGGRR (the GPU's), of the pixels 0xAARRGGBB. */
#include "tmst_photo_fx.h"
#include "tmst_compat.h"
#include "tmst_font.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char *const tmst_photo_filter_names[FX_FILTERS] = {
    "NONE", "BLACK & WHITE", "SEPIA", "NOIR", "VINTAGE", "HIGH CONTRAST", "COLD", "WARM", "TEAL & ORANGE",
    "DUOTONE", "POSTER", "NIGHT VISION", "SCANLINES", "NEGATIVE"};
const char *const tmst_photo_frame_names[FR_FRAMES] = {"NONE", "LETTERBOX", "POLAROID", "FILM STRIP", "VIGNETTE", "TEKKEN",
                                                       "CRT TV", "COMIC"};

int tmst_photo_filter_cpu(int f) {
    return f == FX_BW || f == FX_SEPIA || f == FX_NOIR || f == FX_TEAL_ORANGE || f == FX_DUOTONE || f == FX_POSTER ||
           f == FX_NIGHT || f == FX_NEGATIVE;
}

enum { W = 368, H = 480, OPAQUE = -1, HALF = 0, ADD = 1, SUB = 2, QUARTER = 3 };

/* ---- the canvas: the GPU's primitives, or the CPU's pixels -------------------------- */
typedef struct {
    TmstPrims *k;                      /* live */
    uint32_t *px; int w, h;            /* the photo: pixels ... */
    double s;                          /* ... `s` to a screen unit each way */
    int left, right;                   /* the screen's extent */
} Canvas;

static Canvas live(TmstPrims *k) {
    Canvas c = {k, NULL, 0, 0, 1.0, -tmst_compat_widescreen_side(), W + tmst_compat_widescreen_side()};
    return c;
}
static Canvas pixels(uint32_t *px, int w, int h, int scale, int side) {
    double s = scale > 0 ? scale : 1;
    int m = (int)lround(side / s);
    Canvas c = {NULL, px, w, h, s, -m, W + m};
    return c;
}

static int chan(uint32_t v, int sh) { return (int)(v >> sh & 0xFF); }
static int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static uint32_t rgb(int r, int g, int b) { return 0xFF000000u | (uint32_t)clamp8(r) << 16 | (uint32_t)clamp8(g) << 8 | (uint32_t)clamp8(b); }
/* A pixel under a primitive's colour (0xBBGGRR) in a blend mode, as the GPU. */
static uint32_t blend(uint32_t d, uint32_t bgr, int m) {
    int fr = chan(bgr, 0), fg = chan(bgr, 8), fb = chan(bgr, 16), br = chan(d, 16), bg = chan(d, 8), bb = chan(d, 0);
    switch (m) {
    case HALF: return rgb((br + fr) / 2, (bg + fg) / 2, (bb + fb) / 2);
    case ADD: return rgb(br + fr, bg + fg, bb + fb);
    case SUB: return rgb(br - fr, bg - fg, bb - fb);
    case QUARTER: return rgb(br + fr / 4, bg + fg / 4, bb + fb / 4);
    default: return rgb(fr, fg, fb);
    }
}
/* Screen x / y to pixel columns / rows (where the GPU at that scale puts the
 * primitive's edge). */
static int col(const Canvas *c, double x) { int v = (int)lround((x - c->left) * c->s); return v < 0 ? 0 : v > c->w ? c->w : v; }
static int row(const Canvas *c, double y) { int v = (int)lround(y * c->s); return v < 0 ? 0 : v > c->h ? c->h : v; }

static void mode(TmstPrims *k, int m) {   /* dithered, drawing to the display */
    tmst_prim_room(k, 1);
    tmst_prim_word(k, 0xE1000600u | (uint32_t)(m < 0 ? 0 : m) << 5);
}
static void rect(Canvas *c, int x, int y, int w, int h, uint32_t bgr, int m) {
    if (w <= 0 || h <= 0) return;
    if (c->k) {
        if (m >= 0) mode(c->k, m);
        tmst_prim_room(c->k, 3);
        tmst_prim_word(c->k, (m < 0 ? 0x60000000u : 0x62000000u) | (bgr & 0xFFFFFFu));
        tmst_prim_word(c->k, tmst_xy(x, y));
        tmst_prim_word(c->k, (uint32_t)h << 16 | (uint32_t)w);
        return;
    }
    int x0 = col(c, x), x1 = col(c, x + w), y0 = row(c, y), y1 = row(c, y + h);
    for (int py = y0; py < y1; py++)
        for (uint32_t *p = c->px + (size_t)py * c->w + x0, *e = p + (x1 - x0); p < e; p++) *p = blend(*p, bgr, m);
}
static void whole(Canvas *c, uint32_t bgr, int m) { rect(c, c->left - 8, 0, c->right - c->left + 16, H, bgr, m); }

/* A Gouraud quad (corners 0 1 on one edge, 2 3 on the other: triangles 0 1 2
 * and 1 2 3), half see-through in mode `m` (OPAQUE: not). */
static void triangle(Canvas *c, const double x[3], const double y[3], const uint32_t k[3], int m) {
    double ax = (x[0] - c->left) * c->s, ay = y[0] * c->s, bx = (x[1] - c->left) * c->s, by = y[1] * c->s;
    double cx = (x[2] - c->left) * c->s, cy = y[2] * c->s;
    double area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (fabs(area) < 1e-9) return;
    int x0 = (int)floor(fmin(ax, fmin(bx, cx))), x1 = (int)ceil(fmax(ax, fmax(bx, cx)));
    int y0 = (int)floor(fmin(ay, fmin(by, cy))), y1 = (int)ceil(fmax(ay, fmax(by, cy)));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > c->w) x1 = c->w; if (y1 > c->h) y1 = c->h;
    for (int py = y0; py < y1; py++)
        for (int qx = x0; qx < x1; qx++) {
            double px = qx + 0.5, pyc = py + 0.5;
            double w0 = ((bx - px) * (cy - pyc) - (cx - px) * (by - pyc)) / area;
            double w1 = ((cx - px) * (ay - pyc) - (ax - px) * (cy - pyc)) / area;
            double w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            int r = (int)lround(w0 * chan(k[0], 0) + w1 * chan(k[1], 0) + w2 * chan(k[2], 0));
            int g = (int)lround(w0 * chan(k[0], 8) + w1 * chan(k[1], 8) + w2 * chan(k[2], 8));
            int b = (int)lround(w0 * chan(k[0], 16) + w1 * chan(k[1], 16) + w2 * chan(k[2], 16));
            uint32_t *p = c->px + (size_t)py * c->w + qx;
            *p = blend(*p, (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r, m);
        }
}
static void quad(Canvas *c, const int x[4], const int y[4], const uint32_t k[4], int m) {
    if (c->k) {
        if (m >= 0) mode(c->k, m);
        tmst_prim_room(c->k, 8);
        for (int i = 0; i < 4; i++) {
            tmst_prim_word(c->k, (i ? 0u : m < 0 ? 0x38000000u : 0x3A000000u) | (k[i] & 0xFFFFFFu));
            tmst_prim_word(c->k, tmst_xy(x[i], y[i]));
        }
        return;
    }
    double ax[3] = {x[0], x[1], x[2]}, ay[3] = {y[0], y[1], y[2]}, bx[3] = {x[1], x[2], x[3]}, by[3] = {y[1], y[2], y[3]};
    uint32_t ak[3] = {k[0], k[1], k[2]}, bk[3] = {k[1], k[2], k[3]};
    triangle(c, ax, ay, ak, m);
    triangle(c, bx, by, bk, m);
}
/* The core's font: each glyph's 5 x 7 dots in a cell of cw x ch, white
 * times tint / 128. */
static void text(Canvas *c, int x, int y, const char *s, int cw, int ch, uint32_t tint) {
    if (c->k) { tmst_prim_text(c->k, x, y, s, cw, ch, tint); return; }
    uint32_t colour = rgb(255 * chan(tint, 0) / 128, 255 * chan(tint, 8) / 128, 255 * chan(tint, 16) / 128);
    for (; *s; s++, x += cw) {
        if (*s == ' ') continue;
        const uint8_t *rows = tmst_font_glyph((unsigned char)*s);
        for (int r = 0; r < 7; r++)
            for (int q = 0; q < 5; q++) {
                if (!(rows[r] >> (4 - q) & 1)) continue;
                int x0 = col(c, x + q * cw / 6.0), x1 = col(c, x + (q + 1) * cw / 6.0);
                int y0 = row(c, y + r * ch / 8.0), y1 = row(c, y + (r + 1) * ch / 8.0);
                for (int py = y0; py < y1; py++)
                    for (int px = x0; px < x1; px++) c->px[(size_t)py * c->w + px] = colour;
            }
    }
}
/* The frame plus itself times tint / 128 (the 368-pixel screen only, as the
 * GPU's copy). */
static void boost(Canvas *c, int tint) {
    if (c->k) { tmst_prim_screen_copy(c->k, 0, 0, W, H, 0, 0, (uint32_t)tint * 0x010101u, ADD); return; }
    int x0 = col(c, 0), x1 = col(c, W);
    for (int py = 0; py < c->h; py++)
        for (uint32_t *p = c->px + (size_t)py * c->w + x0, *e = p + (x1 - x0); p < e; p++) {
            int r = chan(*p, 16), g = chan(*p, 8), b = chan(*p, 0);
            *p = rgb(r + r * tint / 128, g + g * tint / 128, b + b * tint / 128);
        }
}

/* Darker towards the edges: two rings, from an ellipse at 55 % of the
 * screen to one at 90 % (nothing to `mid` taken off), and from there to past
 * the screen's border (`mid` to `edge`), subtracted. */
static void vignette(Canvas *c, uint32_t mid, uint32_t edge) {
    enum { N = 24 };
    double cx = (c->left + c->right) / 2.0, rx = (c->right - c->left) / 2.0, cy = H / 2.0, ry = H / 2.0;
    for (int i = 0; i < N; i++) {
        int px[2][3], py[2][3];
        for (int j = 0; j < 2; j++) {
            double a = (i + j) * 6.283185307179586 / N, co = cos(a), si = sin(a);
            double out = fmin(fabs(co) > 1e-6 ? (rx + 6) / (rx * fabs(co)) : 9, fabs(si) > 1e-6 ? (ry + 6) / (ry * fabs(si)) : 9);
            const double scale[3] = {0.55, 0.9, out};
            for (int r = 0; r < 3; r++) { px[j][r] = (int)lround(cx + co * rx * scale[r]); py[j][r] = (int)lround(cy + si * ry * scale[r]); }
        }
        for (int r = 0; r < 2; r++) {
            int x[4] = {px[0][r], px[1][r], px[0][r + 1], px[1][r + 1]};
            int y[4] = {py[0][r], py[1][r], py[0][r + 1], py[1][r + 1]};
            uint32_t a = r ? mid : 0, b = r ? edge : mid;
            uint32_t k[4] = {a, a, b, b};
            quad(c, x, y, k, SUB);
        }
    }
}
/* Every other line of the screen darker (its native lines). */
static void scanlines(Canvas *c, uint32_t dark) {
    for (int y = 1; y < H; y += 2) rect(c, c->left - 8, y, c->right - c->left + 16, 1, dark, SUB);
}

/* ---- filters ---------------------------------------------------------------------- */
/* The GPU's part of filter f; `close`: its close look too (a CPU filter
 * live while the camera moves). */
static void filter_ops(Canvas *c, int f, int close) {
    switch (f) {
    case FX_BW:
        if (close) { whole(c, 0x606060, HALF); whole(c, 0x0c0c0c, SUB); boost(c, 0x20); }
        break;
    case FX_SEPIA:
        if (close) { whole(c, 0x3c6484, HALF); whole(c, 0x100602, SUB); boost(c, 0x18); }
        break;
    case FX_NOIR:
        if (close) { whole(c, 0x606060, HALF); whole(c, 0x1c1c1c, SUB); boost(c, 0x38); }
        vignette(c, 0x141414, 0x585858);
        break;
    case FX_VINTAGE:                               /* less blue, a warm lift, darker corners */
        whole(c, 0x2c0c04, SUB);
        whole(c, 0x4070a0, QUARTER);
        vignette(c, 0x101010, 0x404040);
        break;
    case FX_CONTRAST:                              /* (x - c) * 1.375 */
        whole(c, 0x161616, SUB);
        boost(c, 0x30);
        break;
    case FX_COLD:
        whole(c, 0x1c0800, ADD);
        whole(c, 0x000a16, SUB);
        break;
    case FX_WARM:
        whole(c, 0x000a1c, ADD);
        whole(c, 0x160000, SUB);
        break;
    case FX_TEAL_ORANGE:
        if (close) { whole(c, 0x041430, ADD); whole(c, 0x160406, SUB); boost(c, 0x20); }
        break;
    case FX_DUOTONE:
        if (close) { whole(c, 0x3038b0, HALF); boost(c, 0x20); }
        break;
    case FX_POSTER:
        if (close) { whole(c, 0x101010, SUB); boost(c, 0x30); }
        break;
    case FX_NIGHT:
        if (close) { whole(c, 0x30a018, HALF); whole(c, 0x200820, SUB); boost(c, 0x30); }
        scanlines(c, 0x202020);
        vignette(c, 0x181818, 0x686868);
        break;
    case FX_SCANLINES:
        scanlines(c, 0x303030);
        break;
    default:                                       /* NONE; NEGATIVE: nothing close */
        break;
    }
}

void tmst_photo_filter_prims(TmstPrims *k, int f) {
    Canvas c = live(k);
    filter_ops(&c, f, tmst_photo_filter_cpu(f));
}

static int luma(uint32_t p) { return (299 * chan(p, 16) + 587 * chan(p, 8) + 114 * chan(p, 0) + 500) / 1000; }
static int mix(int a, int b, int t, int n) { return a + (b - a) * t / n; }
static unsigned grain(unsigned x, unsigned y, unsigned seed) {
    unsigned h = x * 374761393u + y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void filter_pixels(uint32_t *px, int w, int h, int f, unsigned seed) {
    if (!tmst_photo_filter_cpu(f)) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t *p = px + (size_t)y * w + x;
            int r = chan(*p, 16), g = chan(*p, 8), b = chan(*p, 0), l = luma(*p);
            switch (f) {
            case FX_BW: *p = rgb(l, l, l); break;
            case FX_SEPIA:
                *p = rgb((393 * r + 769 * g + 189 * b) / 1000, (349 * r + 686 * g + 168 * b) / 1000,
                         (272 * r + 534 * g + 131 * b) / 1000);
                break;
            case FX_NOIR: { int v = (l - 128) * 150 / 100 + 118; *p = rgb(v, v, v); break; }
            case FX_TEAL_ORANGE: {
                /* the blockbuster grade: more colour and contrast, the shadows
                 * pushed to teal, the lights (skin) to orange */
                int c = clamp8((l - 128) * 118 / 100 + 128);
                int sr = c + (r - l) * 140 / 100, sg = c + (g - l) * 140 / 100, sb = c + (b - l) * 140 / 100;
                if (c < 140) { int t = 140 - c; sr -= 70 * t / 140; sg += 18 * t / 140; sb += 60 * t / 140; }
                else { int t = c - 140; sr += 62 * t / 115; sg += 18 * t / 115; sb -= 64 * t / 115; }
                *p = rgb(sr, sg, sb);
                break;
            }
            case FX_DUOTONE: {
                /* deep crimson shadows, Tekken red middle, cream lights */
                int v = clamp8((l - 128) * 115 / 100 + 128);
                if (v < 128) *p = rgb(mix(16, 176, v, 128), mix(4, 22, v, 128), mix(10, 34, v, 128));
                else *p = rgb(mix(176, 255, v - 128, 127), mix(22, 238, v - 128, 127), mix(34, 204, v - 128, 127));
                break;
            }
            case FX_POSTER: {
                /* five steps a channel, a little more colour first */
                int q[3] = {l + (r - l) * 120 / 100, l + (g - l) * 120 / 100, l + (b - l) * 120 / 100};
                for (int i = 0; i < 3; i++) q[i] = (clamp8(q[i]) * 4 + 127) / 255 * 255 / 4;
                *p = rgb(q[0], q[1], q[2]);
                break;
            }
            case FX_NIGHT: {
                int v = l * 130 / 100 + 12 + (int)(grain((unsigned)x, (unsigned)y, seed) % 25) - 12;
                *p = rgb(v * 30 / 100, v, v * 35 / 100);
                break;
            }
            case FX_NEGATIVE: *p = rgb(255 - r, 255 - g, 255 - b); break;
            }
        }
}

/* ---- frames ---------------------------------------------------------------------- */
static void letterbox(Canvas *c) {
    /* bars to 2.35 : 1 on the screen's own aspect (4:3, wider with the sides) */
    double aspect = 4.0 / 3.0 * (c->right - c->left) / (double)W;
    int bar = (int)lround(H * (1.0 - aspect / 2.35) / 2.0);
    if (bar < 8) bar = 8;
    rect(c, c->left - 8, 0, c->right - c->left + 16, bar, 0x000000, OPAQUE);
    rect(c, c->left - 8, H - bar, c->right - c->left + 16, bar, 0x000000, OPAQUE);
}

static void polaroid(Canvas *c) {
    enum { SIDE = 20, TOP = 32, BOTTOM = 108, PAPER = 0xd0dce4, SHADE = 0x282828 };
    int l = c->left, r = c->right;
    rect(c, l - 8, 0, SIDE + 8, H, PAPER, OPAQUE);
    rect(c, r - SIDE, 0, SIDE + 8, H, PAPER, OPAQUE);
    rect(c, l + SIDE, 0, r - l - 2 * SIDE, TOP, PAPER, OPAQUE);
    rect(c, l + SIDE, H - BOTTOM, r - l - 2 * SIDE, BOTTOM, PAPER, OPAQUE);
    /* the print's edge, a little sunk */
    rect(c, l + SIDE, TOP, r - l - 2 * SIDE, 2, SHADE, OPAQUE);
    rect(c, l + SIDE, TOP + 2, 1, H - TOP - BOTTOM - 2, SHADE, OPAQUE);
}

static void film(Canvas *c) {
    enum { BAND = 30, HOLE_W = 12, HOLE_H = 18, PITCH = 44, BASE = 0x101418, HOLE = 0xb0c4cc, EDGE = 0x404850 };
    int l = c->left, r = c->right;
    rect(c, l - 8, 0, BAND + 8, H, BASE, OPAQUE);
    rect(c, r - BAND, 0, BAND + 8, H, BASE, OPAQUE);
    rect(c, l + BAND - 2, 0, 2, H, EDGE, OPAQUE);
    rect(c, r - BAND, 0, 2, H, EDGE, OPAQUE);
    for (int y = 10; y + HOLE_H <= H; y += PITCH) {
        rect(c, l + (BAND - HOLE_W) / 2, y, HOLE_W, HOLE_H, HOLE, OPAQUE);
        rect(c, r - BAND + (BAND - HOLE_W) / 2, y, HOLE_W, HOLE_H, HOLE, OPAQUE);
    }
}

static void tekken(Canvas *c) {
    enum { SIDE = 10, TOP = 18, BOTTOM = 30, DARK = 0x180840, GOLD = 0x30a8d8, ARM_X = 28, ARM_Y = 44, T = 3 };
    int l = c->left, r = c->right;
    rect(c, l - 8, 0, r - l + 16, TOP, DARK, OPAQUE);
    rect(c, l - 8, H - BOTTOM, r - l + 16, BOTTOM, DARK, OPAQUE);
    rect(c, l - 8, TOP, SIDE + 8, H - TOP - BOTTOM, DARK, OPAQUE);
    rect(c, r - SIDE, TOP, SIDE + 8, H - TOP - BOTTOM, DARK, OPAQUE);
    /* a gold line inside, and gold corners over it */
    int x0 = l + SIDE, x1 = r - SIDE, y0 = TOP, y1 = H - BOTTOM;
    rect(c, x0, y0, x1 - x0, 1, GOLD, OPAQUE);
    rect(c, x0, y1 - 1, x1 - x0, 1, GOLD, OPAQUE);
    rect(c, x0, y0, 1, y1 - y0, GOLD, OPAQUE);
    rect(c, x1 - 1, y0, 1, y1 - y0, GOLD, OPAQUE);
    rect(c, x0, y0, ARM_X, T, GOLD, OPAQUE);            rect(c, x0, y0, T, ARM_Y, GOLD, OPAQUE);
    rect(c, x1 - ARM_X, y0, ARM_X, T, GOLD, OPAQUE);   rect(c, x1 - T, y0, T, ARM_Y, GOLD, OPAQUE);
    rect(c, x0, y1 - T, ARM_X, T, GOLD, OPAQUE);       rect(c, x0, y1 - ARM_Y, T, ARM_Y, GOLD, OPAQUE);
    rect(c, x1 - ARM_X, y1 - T, ARM_X, T, GOLD, OPAQUE); rect(c, x1 - T, y1 - ARM_Y, T, ARM_Y, GOLD, OPAQUE);
    /* the name in the bottom bar, gold, with red marks either side */
    static const char name[] = "TEKKEN 3";
    int cw = 8, w = (int)(sizeof name - 1) * cw, tx = (l + r) / 2 - w / 2, ty = H - BOTTOM + 7;
    rect(c, tx - 22, ty + 6, 12, 4, 0x2020c0, OPAQUE);
    rect(c, tx + w + 10, ty + 6, 12, 4, 0x2020c0, OPAQUE);
    text(c, tx, ty, name, cw, 16, 0x205878);
}

/* A quarter disc of `bgr` filling the corner (cx, cy) outside the circle of
 * radius r (rx, ry in screen units: the pixels are not square) about the
 * corner's inner centre, (dx, dy) the way into the screen. */
static void round_corner(Canvas *c, int cx, int cy, int dx, int dy, int rx, int ry, uint32_t bgr) {
    enum { STEPS = 8 };
    int ox = cx + dx * rx, oy = cy + dy * ry;          /* the circle's centre */
    for (int i = 0; i < STEPS; i++) {
        double a0 = i * 1.5707963267948966 / STEPS, a1 = (i + 1) * 1.5707963267948966 / STEPS;
        int x[4] = {cx, ox - (int)lround(dx * rx * cos(a0)), cx, ox - (int)lround(dx * rx * cos(a1))};
        int y[4] = {cy, oy - (int)lround(dy * ry * sin(a0)), cy, oy - (int)lround(dy * ry * sin(a1))};
        uint32_t k[4] = {bgr, bgr, bgr, bgr};
        quad(c, x, y, k, OPAQUE);
    }
}

/* An old television's bezel: dark plastic, the glass's rounded corners, its
 * lines and darker edges, a red light under it. */
static void crt(Canvas *c) {
    enum { SIDE = 22, TOP = 26, BOTTOM = 40, PLASTIC = 0x181a1c, RIM = 0x383c40, RX = 20, RY = 34 };
    int l = c->left, r = c->right, x0 = l + SIDE, x1 = r - SIDE, y0 = TOP, y1 = H - BOTTOM;
    scanlines(c, 0x141414);
    vignette(c, 0x101010, 0x383838);
    rect(c, l - 8, 0, r - l + 16, TOP, PLASTIC, OPAQUE);
    rect(c, l - 8, y1, r - l + 16, BOTTOM, PLASTIC, OPAQUE);
    rect(c, l - 8, TOP, SIDE + 8, y1 - y0, PLASTIC, OPAQUE);
    rect(c, x1, TOP, SIDE + 8, y1 - y0, PLASTIC, OPAQUE);
    round_corner(c, x0, y0, 1, 1, RX, RY, PLASTIC);
    round_corner(c, x1, y0, -1, 1, RX, RY, PLASTIC);
    round_corner(c, x0, y1, 1, -1, RX, RY, PLASTIC);
    round_corner(c, x1, y1, -1, -1, RX, RY, PLASTIC);
    rect(c, x0 + RX, y0 - 2, x1 - x0 - 2 * RX, 2, RIM, OPAQUE);
    rect(c, x0 + RX, y1, x1 - x0 - 2 * RX, 2, RIM, OPAQUE);
    rect(c, x1 - 26, y1 + 18, 6, 6, 0x1010d0, OPAQUE);   /* the power light */
    rect(c, x1 - 60, y1 + 19, 28, 4, 0x2c3034, OPAQUE);
}

/* Halftone dots in the corner (cx, cy) of the panel, (dx, dy) the way into
 * it: rows of dots, every other row moved half a step, all but touching at
 * the corner and smaller towards the middle until they are gone. Screen
 * units; a pixel is about 1.7 times as wide as tall, so a step is 8 across
 * and 13 down (about the same length), a dot 0.6 as wide as tall. */
static void halftone(Canvas *c, int cx, int cy, int dx, int dy, uint32_t bgr) {
    enum { STEP_X = 8, STEP_Y = 13, ROWS = 10, COLS = 17, REACH = 13 };   /* REACH: in steps */
    for (int j = 0; j < ROWS; j++)
        for (int i = 0; i < COLS; i++) {
            double u = i + (j & 1) * 0.5 + 0.5, v = j + 0.5;          /* steps from the corner */
            double fade = 1.0 - sqrt(u * u + v * v) / REACH;
            if (fade <= 0.3) continue;                                /* smaller ones round to bars */
            double ry = 7.4 * fade, rx = ry * 0.6;                    /* the dot's half sizes */
            double px = cx + dx * u * STEP_X, py = cy + dy * v * STEP_Y;
            /* an octagon, as three quads across it: p0 p1 p7 p2, p7 p2 p6 p3, p6 p3 p5 p4 */
            int ox[8], oy[8];
            for (int n = 0; n < 8; n++) {
                double a = (n + 0.5) * 0.7853981633974483;
                ox[n] = (int)lround(px + rx * cos(a)); oy[n] = (int)lround(py + ry * sin(a));
            }
            static const int strip[3][4] = {{0, 1, 7, 2}, {7, 2, 6, 3}, {6, 3, 5, 4}};
            uint32_t k[4] = {bgr, bgr, bgr, bgr};
            for (int q = 0; q < 3; q++) {
                int x[4], y[4];
                for (int e = 0; e < 4; e++) { x[e] = ox[strip[q][e]]; y[e] = oy[strip[q][e]]; }
                quad(c, x, y, k, OPAQUE);
            }
        }
}

/* A comic book's panel: a white gutter, a heavy black line, a yellow caption
 * box top left and halftone dots in the bottom right corner. */
static void comic(Canvas *c) {
    enum { GUTTER = 12, GUTTER_Y = 18, LINE_X = 4, LINE_Y = 6, INK = 0x101010, PAPER = 0xe8f4f8, CAPTION = 0x60e8f8 };
    int l = c->left, r = c->right, x0 = l + GUTTER, x1 = r - GUTTER, y0 = GUTTER_Y, y1 = H - GUTTER_Y;
    rect(c, l - 8, 0, r - l + 16, y0, PAPER, OPAQUE);
    rect(c, l - 8, y1, r - l + 16, H - y1, PAPER, OPAQUE);
    rect(c, l - 8, y0, GUTTER + 8, y1 - y0, PAPER, OPAQUE);
    rect(c, x1, y0, GUTTER + 8, y1 - y0, PAPER, OPAQUE);
    rect(c, x0, y0, x1 - x0, LINE_Y, INK, OPAQUE);
    rect(c, x0, y1 - LINE_Y, x1 - x0, LINE_Y, INK, OPAQUE);
    rect(c, x0, y0, LINE_X, y1 - y0, INK, OPAQUE);
    rect(c, x1 - LINE_X, y0, LINE_X, y1 - y0, INK, OPAQUE);
    halftone(c, x1 - LINE_X, y1 - LINE_Y, -1, -1, INK);
    /* the caption, top left */
    static const char text_[] = "MEANWHILE...";
    int tw = (int)(sizeof text_ - 1) * 7, bx = x0 + LINE_X + 6, by = y0 + LINE_Y + 6;
    rect(c, bx, by, tw + 14, 24, INK, OPAQUE);
    rect(c, bx + 2, by + 3, tw + 10, 18, CAPTION, OPAQUE);
    text(c, bx + 7, by + 4, text_, 7, 16, 0x101010);
}

static void frame_ops(Canvas *c, int f) {
    switch (f) {
    case FR_LETTERBOX: letterbox(c); break;
    case FR_POLAROID: polaroid(c); break;
    case FR_FILM: film(c); break;
    case FR_VIGNETTE: vignette(c, 0x181818, 0x707070); break;
    case FR_TEKKEN: tekken(c); break;
    case FR_CRT: crt(c); break;
    case FR_COMIC: comic(c); break;
    default: break;
    }
}

void tmst_photo_frame_prims(TmstPrims *k, int f) {
    Canvas c = live(k);
    frame_ops(&c, f);
}

void tmst_photo_look(uint32_t *px, int w, int h, int scale, int side, int f, unsigned seed) {
    Canvas c = pixels(px, w, h, scale, side);
    filter_pixels(px, w, h, f, seed);
    filter_ops(&c, f, 0);
}

void tmst_photo_finish(uint32_t *px, int w, int h, int scale, int side, int f, int frame, unsigned seed) {
    Canvas c = pixels(px, w, h, scale, side);
    tmst_photo_look(px, w, h, scale, side, f, seed);
    frame_ops(&c, frame);
}

/* ---- depth of field ---------------------------------------------------------------- */
/* The CPU's: three box passes each way (close to a Gaussian), the box
 * `r` pixels to either side, r the live blur's reach at the level times
 * the scale. */
static int blur_reach(int level, int scale) {
    static const int reach[5] = {1, 2, 3, 4, 6};
    return reach[(level < 1 ? 1 : level > 5 ? 5 : level) - 1] * (scale > 0 ? scale : 1);
}
static void box_line(uint32_t *p, int n, int stride, int r, uint32_t *tmp) {
    for (int i = 0; i < n; i++) tmp[i] = p[(size_t)i * stride];
    int sr = 0, sg = 0, sb = 0, len = 2 * r + 1;
    for (int i = -r; i <= r; i++) {
        uint32_t v = tmp[i < 0 ? 0 : i >= n ? n - 1 : i];
        sr += chan(v, 16); sg += chan(v, 8); sb += chan(v, 0);
    }
    for (int i = 0; i < n; i++) {
        p[(size_t)i * stride] = rgb(sr / len, sg / len, sb / len);
        uint32_t out = tmp[i - r < 0 ? 0 : i - r], in = tmp[i + r + 1 >= n ? n - 1 : i + r + 1];
        sr += chan(in, 16) - chan(out, 16); sg += chan(in, 8) - chan(out, 8); sb += chan(in, 0) - chan(out, 0);
    }
}
static int blur_all(uint32_t *px, int w, int h, int r) {
    uint32_t *tmp = malloc(sizeof(uint32_t) * (size_t)(w > h ? w : h));
    if (!tmp) return 0;
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++) box_line(px + (size_t)y * w, w, 1, r, tmp);
        for (int x = 0; x < w; x++) box_line(px + x, h, w, r, tmp);
    }
    free(tmp);
    return 1;
}

int tmst_photo_blur_behind(uint32_t *px, const uint32_t *stage, int w, int h, int scale, int level) {
    size_t n = (size_t)w * (size_t)h;
    uint32_t *bg = malloc(sizeof(uint32_t) * n);
    if (!bg) return 0;
    memcpy(bg, stage, sizeof(uint32_t) * n);
    if (!blur_all(bg, w, h, blur_reach(level, scale))) { free(bg); return 0; }
    for (size_t i = 0; i < n; i++) {
        int d = abs(chan(px[i], 16) - chan(stage[i], 16)) + abs(chan(px[i], 8) - chan(stage[i], 8)) +
                abs(chan(px[i], 0) - chan(stage[i], 0));
        if (d <= 24) px[i] = bg[i];                    /* the stage's: blurred */
    }
    free(bg);
    return 1;
}

int tmst_photo_blur_band(uint32_t *px, int w, int h, int scale, int level, int band_top) {
    enum { BAND = 160, EASE_ROWS = 40 };
    size_t n = (size_t)w * (size_t)h;
    uint32_t *hard = malloc(sizeof(uint32_t) * n), *soft = malloc(sizeof(uint32_t) * n);
    if (!hard || !soft) { free(hard); free(soft); return 0; }
    memcpy(hard, px, sizeof(uint32_t) * n); memcpy(soft, px, sizeof(uint32_t) * n);
    int ok = blur_all(hard, w, h, blur_reach(level, scale)) && blur_all(soft, w, h, blur_reach((level + 1) / 2, scale));
    double s = scale > 0 ? scale : 1;
    for (int y = 0; ok && y < h; y++) {
        double sy = y / s;                              /* screen rows */
        const uint32_t *from = sy < band_top - EASE_ROWS || sy >= band_top + BAND + EASE_ROWS ? hard
                             : sy < band_top || sy >= band_top + BAND ? soft : NULL;
        if (from) memcpy(px + (size_t)y * w, from + (size_t)y * w, sizeof(uint32_t) * (size_t)w);
    }
    free(hard); free(soft);
    return ok;
}
int tmst_photo_blur_available(void) { return tmst_compat_renderer() != TMST_RENDERER_SOFTWARE; }

/* Each step: the rows averaged with themselves moved r pixels right, left,
 * down and up (B/2 + F/2 copies; the OpenGL renderer samples the frame as
 * it was before each copy). */
void tmst_photo_blur_prims(TmstPrims *k, int level, int y0, int y1) {
    static const int radii[5][4] = {{1, 0, 0, 0}, {1, 2, 0, 0}, {1, 2, 3, 0}, {1, 2, 4, 0}, {1, 3, 5, 7}};
    if (level < 1 || y1 <= y0 || !tmst_photo_blur_available()) return;
    if (level > 5) level = 5;
    const int *r = radii[level - 1];
    for (int i = 0; i < 4 && r[i]; i++) {
        tmst_prim_screen_copy(k, 0, y0, W, y1 - y0, r[i], 0, 0x808080, HALF);
        tmst_prim_screen_copy(k, 0, y0, W, y1 - y0, -r[i], 0, 0x808080, HALF);
        tmst_prim_screen_copy(k, 0, y0, W, y1 - y0, 0, r[i], 0x808080, HALF);
        tmst_prim_screen_copy(k, 0, y0, W, y1 - y0, 0, -r[i], 0x808080, HALF);
    }
}
