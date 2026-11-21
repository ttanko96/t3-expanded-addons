/* tmst.tekken1.stages: Tekken 1 Content's Stages, the drawing of Tekken 1's
 * background (its sky and layers) and its effects, over the floor the
 * Expanded's stage engine draws from the same pack.
 *
 * The packs: tools/tekken1_stages.py makes them at setup from the player's
 * Tekken disc (format 3, numbers 50 + Tekken's stage), staged beside the
 * game as mods/t1-stages/classic. The fight, the pack handed to the engine
 * (arena 9, as Tekken 2's), the STAGE line (TEKKEN 1) and GAME OPTION's
 * TEKKEN 1 STAGES row are src/tekken2/tmst_tekken2_stages.c's, which shares
 * Tekken 2's machinery: it hands us the fight's pack (load), and asks us
 * as the PS1's stage routine starts (tick) and once it has returned
 * (src/tekken2/tmst_tekken2_mirror.c: draw).
 *
 * Tekken 1 draws its background as rings of sprites round the camera, one
 * a layer (hrbg.c, TEKKEN.EXE 0x8005CAB4): 6144 pixels a turn of its
 * camera, its layers' bottoms a number of lines under its horizon. Here each
 * ring stands infinitely far, each cell a POLY_FT4 between the directions of
 * its edges, seen through the PS1's own camera for its stage (the GTE's
 * screen, tmst_tekken2_view): it turns and tilts with the camera as the
 * floor does. A Tekken 1 pixel is ANGLE radians each way, then drawn as
 * large as Tekken 1 draws it (its 512 x 240 screen ours, 368 x 480): round
 * the view's centre and the horizon, kx and ky times (from the GTE's
 * distance h, each frame; bench TMST_T1_BG_SCALE times more, 0: none), so
 * the ring still turns once a turn of the camera, faster than the floor, as
 * Tekken 1's does. Layers drawn first
 * to last, at rank 1022 of the ordering table (after the engine's far
 * background, under the floor), the sky before them: a band of its top
 * colour, its gradient (top to bottom colour, the globals' +12 .. +14
 * halved: from and to the horizon's lines), a second gradient (+24, +26:
 * the effect colours), the rest of the screen below it the second's last
 * colour (TEKKEN.EXE 0x8005B8D4); in slices round the view, its lines on
 * the layers' curve (sky_draw).
 *
 * The effects (T1LY's, the globals' +40):
 *   1 Monument Valley's sunset: in 1800 frames of the fight (not counted in
 *     the pause) the sky's colours to TEKKEN.EXE's (0x800B72AC: 88,44,140
 *     over 238,76,36; the second gradient's top 84,112,166 to 238,132,36),
 *     ours: the sky's bottom from the disc's to 238,132,36, the second
 *     gradient's top with it all along: one sky, no band where they meet;
 *     the sun, the first layer, down SUN_DROP lines as Tekken 1 takes it
 *     (0x8005C994), its disc under the horizon at about frame 1490; ours
 *     besides: the fighters' light with the sky, to warm and dark (DUSK);
 *   2 King George Island's snow: Tekken 1's 100 flakes (0x8005D394,
 *     0x8005D60C): falling 1..2.5 lines a frame, a pixel aside at random,
 *     round a 512 x 240 field that turns and rises with the camera, 1 x 1 or
 *     2 x 1 (the faster) white TILEs, in front of the fighters (rank 10);
 *   3 Stadium's big screen: the fight's picture, as Tekken 1 draws it there
 *     (0x8005D094): the last frame under the gauges drawn small into VRAM
 *     576,0 (the arena's own pictures, which the engine never draws, as the
 *     mirror's), at the end of rank 9 (after the fighters), the screen's
 *     cells taking it from there; 256 x 112 there, Tekken 1's 48 x 21 five
 *     times and a third as sharp, in one texture page.
 *
 * The floor, ours (TMST_T1_FLOOR=engine: the engine's, as before): Tekken
 * 1's 10 x 10 tiles again and again, as Tekken 1 lays them to its horizon,
 * a tile FLOOR_TILE world units (bench TMST_T1_FLOOR_TILE; the engine's
 * cell 2 500, its floor only 10 of them round the fighters, again every 10:
 * too few for Tekken 1's tiles larger). The tiles and their near quarters
 * (twice as large, cubic) are FLOR's, laid as the engine lays them (map
 * column (X + 5) mod 10, row (4 - Z) mod 10 of world tile X, Z; FLOR's turn
 * uvs), the engine handed the pack without its quads (hand_over:
 * tmst_tekken1_own_floor). Each piece on the screen through the camera
 * (0x800A85C0: its place and the PS1's rotation), cut in four while it is
 * large, much nearer at one side or behind the camera; out to FLOOR_FAR
 * tiles, the rest to the horizon the tiles' mean colour. At rank 1021,
 * where the engine's floor is: over the sky and the layers, under the
 * fighters' shadows. */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void tmst_tekken2_view(const CPUState *cpu, double r[3][3], double *ofx, double *ofy, double *h);
/* ... and the game's drawing area in the fight (E3, E4; issue #25). */
void tmst_tekken2_game_area(uint32_t *e3, uint32_t *e4);
/* src/tekken2/tmst_tekken2_extras.c: the packet memory the GPU lets read the
 * upper VRAM half (the Remake's extras', never drawn in our fights) */
uint32_t tmst_tekken2_trusted_memory(unsigned parity, uint32_t *size);
int tmst_tekken1_drawn(void);            /* src/tekken2/tmst_tekken2_stages.c: a Tekken 1 pack drawn now */

enum { DRAWENV = 0x800a8c54, FRAME_PARITY = 0x800adefc, PAUSED = 0x80095494, CAMERA = 0x800a85c0 };
enum { CELLS_MAX = 255, LAYERS_MAX = 16, RING_PIXELS = 6144, LAYER_RANK = 1022, SNOW_RANK = 10, COPY_RANK = 9,
       QUADS_MAX = 1200, QUAD_WORDS = 10, SKY_SLICES = 48, SKY_WORDS = SKY_SLICES * 5 * 9,
       FLAKES = 100, SNOW_FIELD_W = 512, SNOW_FIELD_H = 240, SUNSET_FRAMES = 1800,
       DIRECT = 0x100, SCREEN_X = 576, SCREEN_Y = 0, SCREEN_W = 256, SCREEN_H = 112,
       COPY_TOP = 96, COPY_BOTTOM = 468, COPY_PIECES = 4, COPY_ENV_WORDS = 7, COPY_PIECE_WORDS = 10, COPY_BACK_WORDS = 4,
       COPY_BYTES = (COPY_ENV_WORDS + COPY_PIECES * COPY_PIECE_WORDS + COPY_BACK_WORDS) * 4,
       FLOOR_RANK = 1021, FLOOR_PIECES = 2200, GROUND_WORDS = 6,
       FRAME_BYTES = (QUADS_MAX * QUAD_WORDS + SKY_WORDS + FLAKES * 4 + FLOOR_PIECES * QUAD_WORDS + GROUND_WORDS) * 4 };
static const double TAU = 6.283185307179586;
/* A Tekken 1 background pixel, either way: a turn is 6144 of them. */
static const double ANGLE = 6.283185307179586 / RING_PIXELS;
/* The sky's slices: this far round either side of the view's centre. */
static const double SKY_REACH = 1.3, SKY_UNDER = 2;

typedef struct { uint16_t page, clut; uint8_t u, v; } Cell;
typedef struct {
    int32_t distance;
    int16_t shift, bottom, width, height, places, sprites;
    const uint8_t *ring;
} Layer;
static uint8_t *s_data;                  /* T1LY's copy */
static Cell s_cells[CELLS_MAX];
static Layer s_layers[LAYERS_MAX];
static unsigned s_ncells, s_nlayers, s_effect;
static uint8_t s_sky[4][3];              /* top, bottom, the second gradient's top and bottom */
static int s_sky_at[4];                  /* lines from the horizon: gradient from, to; second from, to */
/* Lines the whole background goes down: Tekken 1's floor goes on to its
 * horizon (its 10 x 10 tiles again and again), the engine's ends 10 cells
 * round the fighters, its far edge FLOOR_EDGE of Tekken 1's lines under the
 * horizon at a level camera (headless, Fiji, 2026-11-19): the lowest
 * layer's bottom put there, as Tekken 2's panoramas meet its far edge. */
enum { FLOOR_EDGE = 59 };
/* Our floor's stages whose background goes up (tmst_tekken1_layers_load),
 * Tekken 1 lines a stage (its pack number PACK_NUMBER0 + it): King George
 * Island (5) 24, its icebergs' bottoms (14) and a strip of the sea under
 * them over the floor's edge, as on Tekken 1's screen (the user's
 * screenshot, headless 16 / 24 / 64, 2026-11-19); Fiji (6) 58; Windermere (0) 45, its
 * stones' bottoms (16, PARALLAX down 21) and a strip of its hedge row over
 * the floor (headless 45 / 69).
 * PARALLAX: Tekken 1 takes the layers of a distance down as its camera is
 * high (TEKKEN.EXE 0x8005CB30: 955 x (its height x 0.625) / the distance
 * lines, the infinitely far ones not), here PARALLAX / distance lines:
 * Fiji's hedge (80 000) 46 lines, its palms 36.8, the hedge's bottom 14 lines
 * under the floor's edge. SAND: lines the sky's second gradient goes lower
 * (Fiji's sand, the sky's lowest colour, 14 lines under the horizon on the
 * disc): Fiji 26, the sand a narrow strip over the hedge, the layer on it
 * (the sea: its bottom there) down with it, its waves into the sand. Fiji's
 * as the user asked (2026-11-19: the hedge lower, the sand a narrow strip,
 * the waves down to the sand; headless). Windermere's stones, trees and
 * hedge row (100 000) 21 lines, as on Tekken 1's screen (the user's). */
enum { PACK_NUMBER0 = 50 };
static const int8_t LIFT[11] = { 45, 0, 0, 0, 0, 24, 58, 0, 0, 0, 0 };
static const int32_t PARALLAX[11] = { 26 * 80000, 0, 0, 0, 0, 0, 46 * 80000, 0, 0, 0, 0 };
static const int8_t SAND[11] = { 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0 };
static int32_t s_parallax;
static int s_sand;
/* FAR: lines a stage's infinitely far layers (distance -1) go down, the
 * rest staying: Windermere 10, its clouds, mountains and lake (the
 * mountains over the stones lower: the user, headless +10 / +18; bench
 * TMST_T1_FAR=n) */
static const int8_t FAR[11] = { 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int s_far;
static int s_drop;
static int s_loaded;

/* The floor (ours): FLOR's tiles (w0, w1: texture with its turn, palette)
 * and their quarters, the four turns' corner uvs, the tiles' mean colour. */
enum { FLOOR_TILE = 7500, FLOOR_FAR = 16, FLOOR_QUARTERS = 3, FLOOR_NEAR_Z = 64, FLOOR_DEPTH = 6, FLOOR_PIXELS = 96 };
static uint32_t s_tiles[100][2], s_quarters[400][2];
static uint16_t s_turns[4][4];
static uint8_t s_ground[3];
static int s_floor;                      /* FLOR read: our floor drawn */

/* Ours unless TMST_T1_FLOOR=engine (src/tekken2/tmst_tekken2_stages.c:
 * the engine then gets FLOR's quads, the background goes down to them). */
int tmst_tekken1_own_floor(void) {
    static int own = -1;
    if (own < 0) { const char *e = getenv("TMST_T1_FLOOR"); own = !(e && !strcmp(e, "engine")); }
    return own;
}
static double floor_tile(void) {
    static double t = -1;
    if (t < 0) { const char *e = getenv("TMST_T1_FLOOR_TILE"); t = e && atof(e) >= 500 ? atof(e) : FLOOR_TILE; }
    return t;
}

/* ---- the pack ---- */

static const uint8_t *section(const uint8_t *b, long size, const char *id, uint32_t *len) {
    uint32_t n;
    if (size < 16) return NULL;
    memcpy(&n, b + 8, 4);
    for (uint32_t i = 0; i < n && 16 + 12 * (long)i + 12 <= size; i++) {
        if (memcmp(b + 16 + 12 * i, id, 4)) continue;
        uint32_t o, l; memcpy(&o, b + 20 + 12 * i, 4); memcpy(&l, b + 24 + 12 * i, 4);
        if ((long)o + (long)l > size) return NULL;
        *len = l;
        return b + o;
    }
    return NULL;
}

static void colour_of(const int16_t *g, int k, uint8_t out[3]) {
    uint16_t lo = (uint16_t)g[k], hi = (uint16_t)g[k + 1];
    out[0] = (uint8_t)lo; out[1] = (uint8_t)(lo >> 8); out[2] = (uint8_t)hi;
}

static int s_frames, s_snow_ready;
void tmst_tekken1_layers_clear(void) {
    free(s_data);
    s_data = NULL;
    s_ncells = s_nlayers = s_effect = 0;
    s_loaded = 0;
    s_frames = 0;
    s_snow_ready = 0;
    s_floor = 0;
}

/* The halfword at VRAM x, y of the pack's rectangles (VRAM: u32 count;
 * u16 x, y, w, h and w x h halfwords each); -1 where none holds it. */
static int vram_half(const uint8_t *v, uint32_t len, int x, int y) {
    uint32_t n, at = 4;
    if (len < 4) return -1;
    memcpy(&n, v, 4);
    for (uint32_t i = 0; i < n && at + 8 <= len; i++) {
        uint16_t r[4]; memcpy(r, v + at, 8);
        uint32_t size = 8u + 2u * r[2] * r[3];
        if (at + size > len) return -1;
        if (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) {
            uint16_t h; memcpy(&h, v + at + 8 + 2 * ((y - r[1]) * r[2] + (x - r[0])), 2);
            return h;
        }
        at += size;
    }
    return -1;
}

/* FLOR's tiles, quarters and turns (tools/tekken2_stages.py flor()), and
 * the tiles' mean colour from VRAM: 0 when it is not as made there. */
static int floor_load(const uint8_t *b, long size) {
    uint32_t len = 0, vlen = 0;
    const uint8_t *o = section(b, size, "FLOR", &len), *v = section(b, size, "VRAM", &vlen);
    if (!o || !v || len < 8 + 4000 + 100 + 4) return 0;
    memcpy(s_tiles, o + 8, sizeof s_tiles);
    memcpy(s_quarters, o + 8 + sizeof s_tiles, sizeof s_quarters);
    uint32_t at = 8 + 4000 + 100;
    uint16_t nv; memcpy(&nv, o + at, 2);
    at += (2u + 4u * nv + 3u) & ~3u;
    if (at + 100 > len) return 0;
    uint32_t quads = 0;
    for (int c = 0; c < 100; c++) quads += o[at + c];
    at += 100 + 4 * quads;
    if (at + sizeof s_turns > len) return 0;
    memcpy(s_turns, o + at, sizeof s_turns);
    /* the mean colour: each tile's 64 x 64 texels through its palette */
    double sum[3] = { 0, 0, 0 };
    long count = 0;
    for (int t = 0; t < 100; t++) {
        uint32_t w0 = s_tiles[t][0], page = w0 >> 16 & 0x9FFu, clut = s_tiles[t][1] >> 16;
        int x0 = (int)(page & 15) * 64 + (int)(w0 & 0xFF) / 4, y0 = (int)(page >> 4 & 1) * 256 + (int)(page >> 11 & 1) * 512
                 + (int)(w0 >> 8 & 0xFF);
        int cx = (int)(clut & 63) * 16, cy = (int)(clut >> 6 & 0x3FF);
        int colours[16];
        for (int k = 0; k < 16; k++) colours[k] = vram_half(v, vlen, cx + k, cy);
        for (int y = 0; y < 64; y += 2)
            for (int x = 0; x < 16; x++) {
                int h = vram_half(v, vlen, x0 + x, y0 + y);
                if (h < 0) return 0;
                for (int n = 0; n < 4; n++) {
                    int c = colours[h >> (4 * n) & 15];
                    if (c <= 0) continue;
                    sum[0] += c & 31; sum[1] += c >> 5 & 31; sum[2] += c >> 10 & 31;
                    count++;
                }
            }
    }
    for (int k = 0; k < 3 && count; k++) { double c = sum[k] / count * 8.25; s_ground[k] = (uint8_t)(c > 255 ? 255 : c); }
    return count > 0;
}

/* The fight's pack (src/tekken2/tmst_tekken2_stages.c fight_extras): T1LY, T1GL. */
void tmst_tekken1_layers_load(const uint8_t *b, long size) {
    tmst_tekken1_layers_clear();
    uint32_t len = 0, glen = 0;
    const uint8_t *o = section(b, size, "T1LY", &len), *g = section(b, size, "T1GL", &glen);
    if (!o || len < 8 || !g || glen < 44 || !(s_data = malloc(len))) return;
    memcpy(s_data, o, len);
    uint16_t h[4]; memcpy(h, s_data, 8);
    if (h[0] > CELLS_MAX || h[1] > LAYERS_MAX || 8u + 8u * h[0] > len) { tmst_tekken1_layers_clear(); return; }
    for (unsigned i = 0; i < h[0]; i++) memcpy(&s_cells[i], s_data + 8 + 8 * i, 6);
    uint32_t at = 8 + 8u * h[0];
    for (unsigned k = 0; k < h[1]; k++) {
        Layer *l = &s_layers[k];
        if (at + 16 > len) { tmst_tekken1_layers_clear(); return; }
        memcpy(&l->distance, s_data + at, 4);
        memcpy(&l->shift, s_data + at + 4, 12);
        at += 16;
        if (l->places <= 0 || l->width <= 0 || at + (uint32_t)l->places > len) { tmst_tekken1_layers_clear(); return; }
        l->ring = s_data + at;
        at += ((uint32_t)l->places + 3) & ~3u;
    }
    s_ncells = h[0]; s_nlayers = h[1]; s_effect = h[2];
    int16_t gl[22]; memcpy(gl, g, 44);
    colour_of(gl, 8, s_sky[0]); colour_of(gl, 10, s_sky[1]);
    colour_of(gl, 16, s_sky[2]); colour_of(gl, 18, s_sky[3]);
    /* 0x8005B8D4: the globals' +12, +14, +24, +26 halved: the gradient from
     * A to A + B lines of the horizon, the second from C over D */
    int a = gl[6] / 2, bb = gl[7] / 2, c = gl[12] / 2, d = gl[13] / 2;
    s_sky_at[0] = a; s_sky_at[1] = a + bb; s_sky_at[2] = c; s_sky_at[3] = c + d;
    if (s_sky_at[2] < s_sky_at[1]) s_sky_at[2] = s_sky_at[1];
    if (s_sky_at[3] < s_sky_at[2]) s_sky_at[3] = s_sky_at[2];
    int lowest = -1000;
    for (unsigned k = 0; k < s_nlayers; k++) if (s_layers[k].bottom > lowest) lowest = s_layers[k].bottom;
    s_floor = tmst_tekken1_own_floor() && floor_load(b, size);
    /* the engine's floor (10 cells round the fighters): the lowest layer to its far edge */
    s_drop = !s_floor && lowest > -1000 && lowest < FLOOR_EDGE ? FLOOR_EDGE - lowest : 0;
    /* our floor goes on to the horizon, over what Tekken 1 shows between
     * its floor's far edge and the horizon: the background LIFT lines up
     * (bench TMST_T1_LIFT=n: n lines, any stage) */
    uint16_t number = 0;
    if (size >= 8) memcpy(&number, b + 6, 2);
    int stage = (int)number - PACK_NUMBER0;
    const char *e = getenv("TMST_T1_LIFT");
    if (s_floor && e && *e) s_drop = -atoi(e);
    else if (s_floor && stage >= 0 && stage < (int)sizeof LIFT) s_drop = -LIFT[stage];
    s_parallax = stage >= 0 && stage < (int)sizeof LIFT ? PARALLAX[stage] : 0;
    /* bench: TMST_T1_PARALLAX=n (a layer 80 000 away n lines down), TMST_T1_SAND=n
     * (the second gradient n lines lower) */
    if ((e = getenv("TMST_T1_PARALLAX")) && *e) s_parallax = atoi(e) * 80000;
    s_sand = stage >= 0 && stage < (int)sizeof SAND ? SAND[stage] : 0;
    if ((e = getenv("TMST_T1_SAND")) && *e) s_sand = atoi(e);
    s_far = stage >= 0 && stage < (int)sizeof FAR ? FAR[stage] : 0;
    if ((e = getenv("TMST_T1_FAR")) && *e) s_far = atoi(e);
    s_loaded = 1;
    if (s_floor)
        tmst_log("tekken1: %u layers, %u cells, effect %u, our floor (tiles of %.0f, ground %d,%d,%d), "
                 "background %d lines up", s_nlayers, s_ncells, s_effect, floor_tile(), s_ground[0], s_ground[1],
                 s_ground[2], -s_drop);
    else
        tmst_log("tekken1: %u layers, %u cells, effect %u, %d lines down to the floor's edge", s_nlayers, s_ncells,
                 s_effect, s_drop);
}

int tmst_tekken1_layers_wanted(void) { return s_loaded && tmst_tekken1_drawn(); }
/* Stadium's big screen on (src/tekken2/tmst_tekken2_mirror.c: a model's
 * late packets, Devil's laser and the like, to its rank, as the mirror's) */
int tmst_tekken1_screen_on(void) { return s_effect == 3 && tmst_tekken1_layers_wanted(); }

/* ---- the view ---- */

typedef struct { double r[3][3], ofx, ofy, h, centre, kx, ky, cam[3]; int margin; } View;

/* A direction (angle round the camera, Tekken 1 lines under the horizon) on
 * the screen; 0 when behind the camera. */
static int screen_of(const View *v, double a, double lines, double *x, double *y) {
    double d[3] = { sin(a), tan(lines * ANGLE), cos(a) }, s[3];
    for (int i = 0; i < 3; i++) s[i] = v->r[i][0] * d[0] + v->r[i][1] * d[1] + v->r[i][2] * d[2];
    if (s[2] <= 0.05) return 0;
    *x = v->ofx + v->h * s[0] / s[2];
    *y = v->ofy + v->h * s[1] / s[2];
    return *x > -4096 && *x < 4096 && *y > -4096 && *y < 4096;
}

/* ---- the packets ---- */

static uint32_t s_cursor, s_end, s_ot;
static uint32_t take(unsigned words) {
    if (!s_cursor || s_cursor + 4 * words > s_end) return 0;
    uint32_t pk = s_cursor;
    s_cursor += 4 * words;
    return pk;
}
/* At the head of rank s_ot: the last put is drawn first. */
static void put(uint32_t pk, unsigned words) {
    uint32_t e = psx_mod_read_word(s_ot);
    psx_mod_write_word(pk, (uint32_t)(words - 1) << 24 | (e & 0xffffffu));
    psx_mod_write_word(s_ot, (e & 0xff000000u) | (pk & 0xffffffu));
}
static uint32_t xy(int x, int y) { return (uint32_t)(uint16_t)x | (uint32_t)(uint16_t)y << 16; }
static uint32_t rgb(const uint8_t c[3]) { return (uint32_t)c[0] | (uint32_t)c[1] << 8 | (uint32_t)c[2] << 16; }

/* ---- the effects ---- */

static const uint8_t SUNSET[4][3] = { { 88, 44, 140 }, { 238, 76, 36 }, { 84, 112, 166 }, { 238, 132, 36 } };
static void sky_now(uint8_t out[4][3]) {
    memcpy(out, s_sky, sizeof s_sky);
    if (s_effect != 1) return;
    /* the sky's top and bottom to TEKKEN.EXE's first two, the second
     * gradient's top from its third to its fourth */
    int f = s_frames < SUNSET_FRAMES ? s_frames : SUNSET_FRAMES;
    for (int ch = 0; ch < 3; ch++) {
        out[0][ch] = (uint8_t)(s_sky[0][ch] + (SUNSET[0][ch] - s_sky[0][ch]) * f / SUNSET_FRAMES);
        /* ours: the sky's bottom to the second gradient's last top (not
         * TEKKEN.EXE's second, 238,76,36: a band where the two met) */
        out[1][ch] = (uint8_t)(s_sky[1][ch] + (SUNSET[3][ch] - s_sky[1][ch]) * f / SUNSET_FRAMES);
    }
    /* the second gradient's top the sky's bottom all along (the disc's
     * globals start them so; TEKKEN.EXE's 84,112,166 a blue band there) */
    memcpy(out[2], out[1], sizeof out[2]);
}

/* The fighters' light with the sunset (ours): out of 128 a channel, 128 at
 * its start, DUSK at its end (warm and dark: red 75 %, green 50 %, blue
 * 40 %), as fast as the sky. Their ambient light through it in
 * src/tekken2/tmst_tekken2_stages.c's SetBackColor; their light's colour
 * (0x80039960's colour matrix columns 0 and 2, the arena's light entry
 * +8..+10, column 1 the hit flash's) here each stage frame, in RAM and the
 * GTE (as the Expanded's light_push), so a round's set-up and a hit flash
 * keep it. */
static const int DUSK[3] = { 96, 64, 51 };
enum { LIGHT_TABLE = 0x80096c78, LIGHT_ENTRY = 28, COLOUR_MATRIX = 0x8009e5c0, ARENA = 0x800adc84 };
int tmst_tekken1_dusk(int channel) {
    if (s_effect != 1 || !tmst_tekken1_layers_wanted() || channel < 0 || channel > 2) return 128;
    int f = s_frames < SUNSET_FRAMES ? s_frames : SUNSET_FRAMES;
    return 128 - (128 - DUSK[channel]) * f / SUNSET_FRAMES;
}
static void dusk_light(CPUState *cpu) {
    if (s_effect != 1) return;
    int arena = (int16_t)psx_mod_read_half(ARENA);
    if (arena < 0 || arena >= 18) return;
    uint32_t e = LIGHT_TABLE + LIGHT_ENTRY * (uint32_t)arena;
    for (int i = 0; i < 3; i++) {
        int c = psx_mod_read_byte(e + 10u - (uint32_t)i), m = tmst_tekken1_dusk(i);   /* r, g, b */
        psx_mod_write_half(COLOUR_MATRIX + 6u * i, (uint16_t)((c << 4) * m / 128));
        psx_mod_write_half(COLOUR_MATRIX + 6u * i + 4u, (uint16_t)(((c >> 1) << 4) * m / 128));
    }
    uint16_t mx[9];
    for (int i = 0; i < 9; i++) mx[i] = psx_mod_read_half(COLOUR_MATRIX + 2u * i);
    cpu->gte_ctrl[16] = mx[0] | (uint32_t)mx[1] << 16;           /* SetColorMatrix */
    cpu->gte_ctrl[17] = mx[2] | (uint32_t)mx[3] << 16;
    cpu->gte_ctrl[18] = mx[4] | (uint32_t)mx[5] << 16;
    cpu->gte_ctrl[19] = mx[6] | (uint32_t)mx[7] << 16;
    cpu->gte_ctrl[20] = (uint32_t)(int32_t)(int16_t)mx[8];
}

/* The sun (the sunset's first layer: Monument Valley's 64 x 32 sprite, its
 * disc rows 9 .. 22, the layer's bottom 20 lines over the horizon): lines it
 * has gone down, as Tekken 1 takes it down (TEKKEN.EXE 0x8005C994: 52 x the
 * frame / 1800, added to the first layer's lines at 0x8005CC6C): its disc
 * under the horizon at about frame 1490. */
enum { SUN_DROP = 52 };
static double sun_down(const Layer *l) {
    (void)l;
    if (s_effect != 1) return 0;
    int f = s_frames < SUNSET_FRAMES ? s_frames : SUNSET_FRAMES;
    return (double)SUN_DROP * f / SUNSET_FRAMES;
}

static struct { int32_t y, speed; int16_t x; uint8_t w; } s_flakes[FLAKES];
static uint32_t s_seed = 0x2545f491u;
static uint32_t lcg(void) { s_seed = s_seed * 1103515245u + 12345u; return s_seed; }
static void snow_init(void) {
    for (int i = 0; i < FLAKES; i++) {
        s_flakes[i].x = (int16_t)(lcg() >> 23);
        s_flakes[i].y = (int32_t)((lcg() >> 16) * 240u);
        int32_t speed = (int32_t)((lcg() >> 16) * 3u + 0x20000u);
        s_flakes[i].w = speed > 0x40000 ? 2 : 1;
        s_flakes[i].speed = speed / 2;
    }
    s_snow_ready = 1;
}
static void snow_step(void) {
    if (!s_snow_ready) snow_init();
    for (int i = 0; i < FLAKES; i++) {
        s_flakes[i].y += s_flakes[i].speed;
        if (s_flakes[i].y >= SNOW_FIELD_H << 16) s_flakes[i].y -= SNOW_FIELD_H << 16;
        s_flakes[i].x = (int16_t)((s_flakes[i].x + ((lcg() >> 16) > 0x7fff ? -1 : 1) + SNOW_FIELD_W) % SNOW_FIELD_W);
    }
}

/* As the PS1's stage routine starts, a frame of the stage, not in the pause. */
void tmst_tekken1_layers_tick(void) {
    if (!tmst_tekken1_layers_wanted() || psx_mod_read_word(PAUSED)) return;
    s_frames++;
    if (s_effect == 2) snow_step();
}

static void snow_draw(const View *v, uint32_t base, double turned, double horizon) {
    s_ot = base + 4u * SNOW_RANK;
    int left = -v->margin, width = 368 + 2 * v->margin;
    int shift = (int)fmod(fmod(turned, SNOW_FIELD_W) + SNOW_FIELD_W, SNOW_FIELD_W);
    int rise = (int)fmod(fmod(horizon / 2, SNOW_FIELD_H) + SNOW_FIELD_H, SNOW_FIELD_H);
    for (int i = 0; i < FLAKES; i++) {
        int fx = (s_flakes[i].x - shift + SNOW_FIELD_W) % SNOW_FIELD_W;
        int fy = ((s_flakes[i].y >> 16) + rise) % SNOW_FIELD_H;
        uint32_t pk = take(4);
        if (!pk) return;
        int x = left + fx * width / SNOW_FIELD_W, y = fy * 2;
        psx_mod_write_word(pk + 4, 0x60ffffffu);
        psx_mod_write_word(pk + 8, xy(x, y));
        psx_mod_write_word(pk + 12, 2u << 16 | (uint32_t)(s_flakes[i].w == 2 ? 2 : 1));
        put(pk, 4);
    }
}

/* The big screen: the last frame's picture under the gauges, drawn small
 * into SCREEN_X, SCREEN_Y at the end of rank COPY_RANK (after the fighters).
 * As src/tekken2/tmst_tekken2_mirror.c's copy. */
static uint32_t s_copy_block;
static void screen_copy(uint32_t base, unsigned parity) {
    if (!s_copy_block && !(s_copy_block = psx_mod_alloc_gpu_dma_memory(2 * COPY_BYTES, 16))) return;
    uint32_t pk = s_copy_block + parity * COPY_BYTES;
    uint32_t link = base + 4 * COPY_RANK, rank0 = (base + 4 * (COPY_RANK - 1)) & 0xffffffu;
    for (unsigned guard = 0;; guard++) {
        uint32_t next = psx_mod_read_word(link) & 0xffffffu;
        if (next == (pk & 0xffffffu)) return;
        if (next == rank0) break;
        if (next == 0xffffffu || guard >= 1u << 16) return;
        link = 0x80000000u | next;
    }
    uint32_t e = psx_mod_read_word(link);
    psx_mod_write_word(pk, (uint32_t)(COPY_ENV_WORDS - 1) << 24 | ((pk + COPY_ENV_WORDS * 4) & 0xffffffu));
    psx_mod_write_word(pk + 4, 0xe3000000u | (uint32_t)SCREEN_Y << 10 | SCREEN_X);
    psx_mod_write_word(pk + 8, 0xe4000000u | (uint32_t)(SCREEN_Y + SCREEN_H - 1) << 10 | (SCREEN_X + SCREEN_W - 1));
    psx_mod_write_word(pk + 12, 0xe5000000u);
    psx_mod_write_word(pk + 16, 0x02000000u);                     /* black first */
    psx_mod_write_word(pk + 20, xy(SCREEN_X, SCREEN_Y));
    psx_mod_write_word(pk + 24, (uint32_t)SCREEN_H << 16 | SCREEN_W);
    uint32_t q = pk + COPY_ENV_WORDS * 4;
    static const int xs[3] = { 0, 256, 368 }, ys[3] = { COPY_TOP, 256, COPY_BOTTOM };
    unsigned n = 0;
    for (int py = 0; py < 2; py++)
        for (int px = 0; px < 2; px++, n++) {
            int xa = xs[px], xb = xs[px + 1], ya = ys[py], yb = ys[py + 1];
            int dx0 = SCREEN_X + xa * SCREEN_W / 368, dx1 = SCREEN_X + xb * SCREEN_W / 368;
            int dy0 = SCREEN_Y + (ya - COPY_TOP) * SCREEN_H / (COPY_BOTTOM - COPY_TOP);
            int dy1 = SCREEN_Y + (yb - COPY_TOP) * SCREEN_H / (COPY_BOTTOM - COPY_TOP);
            int ox = xa / 256 * 256, oy = ya / 256 * 256;
            unsigned u0 = (unsigned)(xa - ox), u1 = (unsigned)(xb - ox > 255 ? 255 : xb - ox);
            unsigned v0 = (unsigned)(ya - oy), v1 = (unsigned)(yb - oy > 255 ? 255 : yb - oy);
            uint32_t page = 0x100u | (uint32_t)(ox / 64) | (oy ? 0x10u : 0);
            uint32_t w = q + n * COPY_PIECE_WORDS * 4;
            psx_mod_write_word(w, (uint32_t)(COPY_PIECE_WORDS - 1) << 24 | ((w + COPY_PIECE_WORDS * 4) & 0xffffffu));
            psx_mod_write_word(w + 4, 0x2d808080u);
            psx_mod_write_word(w + 8, xy(dx0, dy0));
            psx_mod_write_word(w + 12, v0 << 8 | u0);
            psx_mod_write_word(w + 16, xy(dx1, dy0));
            psx_mod_write_word(w + 20, page << 16 | v0 << 8 | u1);
            psx_mod_write_word(w + 24, xy(dx0, dy1));
            psx_mod_write_word(w + 28, v1 << 8 | u0);
            psx_mod_write_word(w + 32, xy(dx1, dy1));
            psx_mod_write_word(w + 36, v1 << 8 | u1);
        }
    /* back to the game's drawing area (its DRAWENV's, not the whole screen:
     * issue #25), its offset its DRAWENV's */
    uint32_t back = q + COPY_PIECES * COPY_PIECE_WORDS * 4, e3, e4;
    tmst_tekken2_game_area(&e3, &e4);
    int ox = (int16_t)psx_mod_read_half(0x800a8f14u + 8), oy = (int16_t)psx_mod_read_half(0x800a8f14u + 10);
    psx_mod_write_word(back, (uint32_t)(COPY_BACK_WORDS - 1) << 24 | (e & 0xffffffu));
    psx_mod_write_word(back + 4, e3);
    psx_mod_write_word(back + 8, e4);
    psx_mod_write_word(back + 12, 0xe5000000u | (uint32_t)(oy & 0x7ff) << 11 | (uint32_t)(ox & 0x7ff));
    psx_mod_write_word(link, (e & 0xff000000u) | (pk & 0xffffffu));
}

/* ---- the floor (ours) ---- */

/* World x, 0, z in the camera's space (c[2]: depth) and on the screen. */
static void floor_point(const View *v, double x, double z, double c[3], double s[2]) {
    double d[3] = { x - v->cam[0], -v->cam[1], z - v->cam[2] };
    for (int i = 0; i < 3; i++) c[i] = v->r[i][0] * d[0] + v->r[i][1] * d[1] + v->r[i][2] * d[2];
    double z1 = c[2] > 1 ? c[2] : 1;
    s[0] = v->ofx + v->h * c[0] / z1;
    s[1] = v->ofy + v->h * c[1] / z1;
}

static int s_pieces, s_precise;
static int32_t in_pixel(double p, int v) {
    double lo = (double)v * 65536.0, q = p * 65536.0;
    return (int32_t)(q < lo ? lo : q > lo + 65535 ? lo + 65535 : q);
}
/* One textured quad: corners (x-, z+), (x+, z+), (x-, z-), (x+, z-). */
static void floor_quad(const double s[4][2], const double depth[4], const double uv[4][2], uint32_t w0, uint32_t w1) {
    uint32_t pk = take(QUAD_WORDS);
    if (!pk) return;
    uint32_t base = w0 & 0xFFFFu, page = (w0 & 0x39FF0000u) >> 16, clut = w1 >> 16 & 0xFFFFu;
    psx_mod_write_word(pk + 4, 0x2cu << 24 | 0x808080u);
    for (int k = 0; k < 4; k++) {
        int u = (int)lround(uv[k][0]), vv = (int)lround(uv[k][1]);
        uint32_t t = (base + (uint32_t)(u | vv << 8)) & 0xFFFFu;
        if (k == 0) t |= clut << 16;
        if (k == 1) t |= page << 16;
        int x = (int)lround(s[k][0]), y = (int)lround(s[k][1]);
        uint32_t w = xy(x, y);
        psx_mod_write_word(pk + 8 + 8 * k, w);
        if (s_precise) {
            double z = depth[k];
            psx_mod_precise_word(pk + 8 + 8 * k, w, in_pixel(s[k][0], x), in_pixel(s[k][1], y),
                                 (uint32_t)(z < 1 ? 1 : z > 0xFFFF ? 0xFFFF : z));
        }
        psx_mod_write_word(pk + 12 + 8 * k, t);
    }
    put(pk, QUAD_WORDS);
    s_pieces++;
}

/* A piece of the floor, x0..x1, z0 (its z+ edge)..z1, its corners' uvs: on
 * the screen, or cut in four. */
static void floor_piece(const View *v, double x0, double x1, double z0, double z1, const double uv[4][2],
                        uint32_t w0, uint32_t w1, int depth) {
    const double xs[4] = { x0, x1, x0, x1 }, zs[4] = { z0, z0, z1, z1 };
    double c[4][3], s[4][2], dz[4];
    int behind = 0, ahead = 0, left = 1, right = 1, above = 1, below = 1;
    double zmin = 1e30, zmax = 0, sx0 = 1e30, sx1 = -1e30, sy0 = 1e30, sy1 = -1e30;
    for (int k = 0; k < 4; k++) {
        floor_point(v, xs[k], zs[k], c[k], s[k]);
        dz[k] = c[k][2];
        if (c[k][2] < FLOOR_NEAR_Z) { behind++; continue; }
        ahead++;
        if (c[k][2] < zmin) zmin = c[k][2];
        if (c[k][2] > zmax) zmax = c[k][2];
        if (s[k][0] < sx0) sx0 = s[k][0];
        if (s[k][0] > sx1) sx1 = s[k][0];
        if (s[k][1] < sy0) sy0 = s[k][1];
        if (s[k][1] > sy1) sy1 = s[k][1];
    }
    if (!ahead) return;
    if (!behind) {
        if (sx1 < -v->margin || sx0 >= 368 + v->margin || sy1 < 0 || sy0 >= 480) return;
        int big = sx1 - sx0 > FLOOR_PIXELS || sy1 - sy0 > FLOOR_PIXELS / 2;
        if (!big || depth >= FLOOR_DEPTH || (zmax < zmin * 1.25 && sx1 - sx0 < 600 && sy1 - sy0 < 400)) {
            if (sx1 - sx0 < 1000 && sy1 - sy0 < 500 && sx0 > -1000 && sx1 < 1000 && sy0 > -1000 && sy1 < 1000)
                floor_quad(s, dz, uv, w0, w1);
            return;
        }
    } else {
        /* wholly to one side of the view: dropped (the corners ahead and
         * the directions of those behind, all past one edge) */
        for (int k = 0; k < 4; k++) {
            double h = c[k][2] > 1 ? c[k][2] : 1;
            double px = c[k][0] / h * v->h, py = c[k][1] / h * v->h;
            if (c[k][2] < FLOOR_NEAR_Z) { px = c[k][0] * 1e6; py = c[k][1] * 1e6; }
            if (v->ofx + px >= -v->margin) left = 0;
            if (v->ofx + px < 368 + v->margin) right = 0;
            if (v->ofy + py >= 0) above = 0;
            if (v->ofy + py < 480) below = 0;
        }
        if (left || right || above || below || depth >= FLOOR_DEPTH) return;
    }
    if (s_pieces >= FLOOR_PIECES) return;
    double xm = (x0 + x1) / 2, zm = (z0 + z1) / 2, um[5][2];
    /* the uvs at the edges' middles and the centre: 01, 02, 13, 23, centre */
    for (int i = 0; i < 2; i++) {
        um[0][i] = (uv[0][i] + uv[1][i]) / 2; um[1][i] = (uv[0][i] + uv[2][i]) / 2;
        um[2][i] = (uv[1][i] + uv[3][i]) / 2; um[3][i] = (uv[2][i] + uv[3][i]) / 2;
        um[4][i] = (uv[0][i] + uv[1][i] + uv[2][i] + uv[3][i]) / 4;
    }
    const double q[4][4][2] = {
        { { uv[0][0], uv[0][1] }, { um[0][0], um[0][1] }, { um[1][0], um[1][1] }, { um[4][0], um[4][1] } },
        { { um[0][0], um[0][1] }, { uv[1][0], uv[1][1] }, { um[4][0], um[4][1] }, { um[2][0], um[2][1] } },
        { { um[1][0], um[1][1] }, { um[4][0], um[4][1] }, { uv[2][0], uv[2][1] }, { um[3][0], um[3][1] } },
        { { um[4][0], um[4][1] }, { um[2][0], um[2][1] }, { um[3][0], um[3][1] }, { uv[3][0], uv[3][1] } } };
    floor_piece(v, x0, xm, z0, zm, q[0], w0, w1, depth + 1);
    floor_piece(v, xm, x1, z0, zm, q[1], w0, w1, depth + 1);
    floor_piece(v, x0, xm, zm, z1, q[2], w0, w1, depth + 1);
    floor_piece(v, xm, x1, zm, z1, q[3], w0, w1, depth + 1);
}

static int pmod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

/* The turn's corner uvs (u, v) over the part (s0..s1 across x, t0..t1
 * from z+ to z-) of a tile or quarter. */
static void turn_uv(unsigned turn, double s0, double s1, double t0, double t1, double uv[4][2]) {
    const uint16_t *c = s_turns[turn & 3];
    const double ss[4] = { s0, s1, s0, s1 }, ts[4] = { t0, t0, t1, t1 };
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 2; i++) {
            double a = c[0] >> (8 * i) & 0xFF, b = c[1] >> (8 * i) & 0xFF, d = c[2] >> (8 * i) & 0xFF,
                   e = c[3] >> (8 * i) & 0xFF;
            uv[k][i] = (1 - ss[k]) * (1 - ts[k]) * a + ss[k] * (1 - ts[k]) * b + (1 - ss[k]) * ts[k] * d + ss[k] * ts[k] * e;
        }
}

static void floor_draw(const View *v, uint32_t base) {
    s_ot = base + 4u * FLOOR_RANK;
    s_pieces = 0;
    s_precise = psx_mod_precision_wanted();
    double t = floor_tile();
    int tx = (int)floor(v->cam[0] / t), tz = (int)floor(v->cam[2] / t);
    for (int dz = -FLOOR_FAR; dz <= FLOOR_FAR; dz++)
        for (int dx = -FLOOR_FAR; dx <= FLOOR_FAR; dx++) {
            if (dx * dx + dz * dz > FLOOR_FAR * FLOOR_FAR) continue;
            int X = tx + dx, Z = tz + dz;
            double x0 = X * t, x1 = x0 + t, z0 = (Z + 1) * t, z1 = Z * t;       /* z0: the z+ edge */
            /* behind the camera, the whole tile: dropped */
            double c[3], s[2], most = -1e30;
            const double xs[4] = { x0, x1, x0, x1 }, zs[4] = { z0, z0, z1, z1 };
            for (int k = 0; k < 4; k++) { floor_point(v, xs[k], zs[k], c, s); if (c[2] > most) most = c[2]; }
            if (most < FLOOR_NEAR_Z) continue;
            int col = pmod(X + 5, 10), row = pmod(4 - Z, 10);
            uint32_t w0 = s_tiles[10 * row + col][0], w1 = s_tiles[10 * row + col][1];
            unsigned turn = w0 >> 30;
            double uv[4][2];
            if (abs(dx) > FLOOR_QUARTERS || abs(dz) > FLOOR_QUARTERS) {
                turn_uv(turn, 0, 1, 0, 1, uv);
                floor_piece(v, x0, x1, z0, z1, uv, w0 & 0x3FFFFFFFu, w1, 0);
                continue;
            }
            for (int hr = 0; hr < 2; hr++)              /* the quarters: hr 0 the z+ half, hc 0 the x- */
                for (int hc = 0; hc < 2; hc++) {
                    const uint32_t *qw = s_quarters[(2 * row + hr) * 20 + 2 * col + hc];
                    turn_uv(turn, 0, 1, 0, 1, uv);
                    double xa = x0 + hc * t / 2, za = z0 - hr * t / 2;
                    floor_piece(v, xa, xa + t / 2, za, za - t / 2, uv, qw[0], qw[1], 0);
                }
        }
    /* beyond: the mean colour from the horizon down (put last: drawn first) */
    double ax, ay, bx, by;
    if (screen_of(v, v->centre - 0.4, 0, &ax, &ay) && screen_of(v, v->centre + 0.4, 0, &bx, &by) && fabs(bx - ax) > 1) {
        int x0 = -v->margin - 8, x1 = 368 + v->margin + 8;
        double yl = ay + (by - ay) * (x0 - ax) / (bx - ax), yr = ay + (by - ay) * (x1 - ax) / (bx - ax);
        if (yl < -1000) yl = -1000;
        if (yl > 480) yl = 480;
        if (yr < -1000) yr = -1000;
        if (yr > 480) yr = 480;
        uint32_t pk = take(GROUND_WORDS);
        if (pk && (yl < 480 || yr < 480)) {
            psx_mod_write_word(pk + 4, 0x28000000u | rgb(s_ground));
            psx_mod_write_word(pk + 8, xy(x0, (int)floor(yl)));
            psx_mod_write_word(pk + 12, xy(x1, (int)floor(yr)));
            psx_mod_write_word(pk + 16, xy(x0, 480));
            psx_mod_write_word(pk + 20, xy(x1, 480));
            put(pk, GROUND_WORDS);
        }
    }
}

/* ---- the frame ---- */

static void cell_quad(const View *v, const Layer *l, const Cell *c, double a0, double a1, double down) {
    double x[4], y[4];
    double top = (l->bottom - l->height + s_drop + down) * v->ky, bottom = (l->bottom + s_drop + down) * v->ky;
    if (!screen_of(v, a0, top, &x[0], &y[0]) || !screen_of(v, a1, top, &x[1], &y[1]) ||
        !screen_of(v, a0, bottom, &x[2], &y[2]) || !screen_of(v, a1, bottom, &x[3], &y[3]))
        return;
    int left = 1, right = 1, above = 1, below = 1;
    for (int k = 0; k < 4; k++) {
        if (x[k] >= -v->margin) left = 0;
        if (x[k] < 368 + v->margin) right = 0;
        if (y[k] >= 0) above = 0;
        if (y[k] < 480) below = 0;
    }
    if (left || right || above || below) return;
    uint32_t pk = take(QUAD_WORDS);
    if (!pk) return;
    unsigned u0 = c->u, u1 = c->u + (unsigned)l->width - 1, v0 = c->v, v1 = c->v + (unsigned)l->height - 1;
    uint32_t page = c->page, clut = c->clut;
    if (page & DIRECT) {                 /* the big screen: our copy of the fight */
        page = DIRECT | SCREEN_X / 64 | (SCREEN_Y >= 256 ? 0x10u : 0);
        u0 = SCREEN_X % 64; u1 = u0 + SCREEN_W - 1; v0 = SCREEN_Y % 256; v1 = v0 + SCREEN_H - 1; clut = 0;
    }
    if (u1 > 255) u1 = 255;
    if (v1 > 255) v1 = 255;
    const unsigned us[4] = { u0, u1, u0, u1 }, vs[4] = { v0, v0, v1, v1 };
    psx_mod_write_word(pk + 4, 0x2du << 24 | 0x808080u);
    for (int k = 0; k < 4; k++) {
        uint32_t uv = us[k] | vs[k] << 8;
        if (k == 0) uv |= clut << 16;
        if (k == 1) uv |= page << 16;
        psx_mod_write_word(pk + 8 + 8 * k, xy((int)lround(x[k]), (int)lround(y[k])));
        psx_mod_write_word(pk + 12 + 8 * k, uv);
    }
    put(pk, QUAD_WORDS);
}

/* The sky's bands in SKY_SLICES slices round the view, each slice's band
 * edges (lines under the horizon) through screen_of as the layers' are: a
 * line of the sky follows the same curve as a layer's (Fiji's sand under
 * its sea's waves at the screen's sides; flat bands parted from them, the
 * user's screenshot, 2026-11-19). Above the first edge the top colour, then
 * the gradient, the bottom colour, the second gradient, its last colour
 * down past the screen. */
static void sky_quad(const double x[4], const double y[4], const uint8_t *c0, const uint8_t *c1) {
    uint32_t pk = take(9);
    if (!pk) return;
    const uint8_t *c[4] = { c0, c0, c1, c1 };
    for (int k = 0; k < 4; k++) {
        double yy = y[k] < -16 ? -16 : y[k] > 496 ? 496 : y[k];
        psx_mod_write_word(pk + 4 + 8 * k, (k ? 0 : 0x38u << 24) | rgb(c[k]));
        psx_mod_write_word(pk + 8 + 8 * k, xy((int)lround(x[k]), (int)lround(yy)));
    }
    put(pk, 9);
}
static void sky_draw(const View *v, uint8_t sky[4][3]) {
    double lines[4];
    for (int k = 0; k < 4; k++) lines[k] = (s_sky_at[k] + s_drop + (k >= 2 ? s_sand : 0)) * v->ky;
    double left = -v->margin - 8, right = 368 + v->margin + 8;
    for (int i = 0; i < SKY_SLICES; i++) {
        double a[2] = { v->centre + SKY_REACH * (2.0 * i / SKY_SLICES - 1), v->centre + SKY_REACH * (2.0 * (i + 1) / SKY_SLICES - 1) };
        double x[2][4], y[2][4];
        int ok = 1;
        for (int s = 0; s < 2 && ok; s++)
            for (int k = 0; k < 4 && ok; k++) ok = screen_of(v, a[s], lines[k], &x[s][k], &y[s][k]);
        if (!ok) continue;
        double lo = fmin(fmin(x[0][0], x[0][3]), fmin(x[1][0], x[1][3])), hi = fmax(fmax(x[0][0], x[0][3]), fmax(x[1][0], x[1][3]));
        if (hi < left || lo > right || hi - lo > 1000) continue;
        /* the bands, bottom first (the last put drawn first: each over the
         * one above it), each from SKY_UNDER lines higher: a layer's edge
         * over a band's (Fiji's sea over its sand) is a chord of a wider
         * cell, a pixel off the slices' midway, the band above seen there
         * (the user's screenshot, 2026-11-19) */
        const double xb[4] = { x[0][3], x[1][3], x[0][3], x[1][3] };
        const double yb[4] = { y[0][3] - SKY_UNDER, y[1][3] - SKY_UNDER, 496, 496 };
        sky_quad(xb, yb, sky[3], sky[3]);
        for (int k = 2; k >= 0; k--) {
            const double xs[4] = { x[0][k], x[1][k], x[0][k + 1], x[1][k + 1] };
            const double ys[4] = { y[0][k] - SKY_UNDER, y[1][k] - SKY_UNDER, y[0][k + 1], y[1][k + 1] };
            sky_quad(xs, ys, k == 1 ? sky[1] : sky[k], k == 1 ? sky[1] : k == 0 ? sky[1] : sky[3]);
        }
        const double xt[4] = { x[0][0], x[1][0], x[0][0], x[1][0] }, yt[4] = { -16, -16, y[0][0], y[1][0] };
        sky_quad(xt, yt, sky[0], sky[0]);
    }
}

void tmst_tekken1_layers_draw(CPUState *cpu) {
    if (!s_loaded) return;
    dusk_light(cpu);
    uint32_t room = 0, mem = tmst_tekken2_trusted_memory(psx_mod_read_word(FRAME_PARITY) & 1u, &room);
    if (!mem || room < FRAME_BYTES) return;
    uint32_t base = psx_mod_read_word(psx_mod_read_word(DRAWENV) + 4);
    if (!base) return;
    unsigned parity = psx_mod_read_word(FRAME_PARITY) & 1u;
    View v;
    tmst_tekken2_view(cpu, v.r, &v.ofx, &v.ofy, &v.h);
    if (v.h <= 0) return;
    v.margin = tmst_compat_widescreen_margin();
    if (v.margin < 0) v.margin = 0;
    /* the view's centre: the direction the camera looks (its rotation's third row) */
    v.centre = atan2(v.r[2][0], v.r[2][2]);
    /* bench: TMST_T1_TURN degrees more round the rings (the Stadium's screen in view) */
    static double turn = -1, scale = -1;
    if (turn < 0) { const char *e = getenv("TMST_T1_TURN"); turn = e && *e ? atof(e) * TAU / 360 : 0; }
    if (scale < 0) { const char *e = getenv("TMST_T1_BG_SCALE"); scale = e && *e ? atof(e) : 1; if (scale < 0) scale = 0; }
    /* Tekken 1's pixel as large as Tekken 1 draws it: 368 / 512 of our
     * pixels across, 2 of our lines down (the view's rows: the second
     * 20 / 12 of the GTE's) */
    v.kx = v.ky = 1;
    if (scale > 0) {
        v.kx = scale * (368.0 / 512.0) / (v.h * ANGLE);
        v.ky = scale * 2.0 / (v.h * (20.0 / 12.0) * ANGLE);
        if (v.kx < 0.25) v.kx = 0.25;
        if (v.ky < 0.25) v.ky = 0.25;
    }
    v.cam[0] = (int32_t)psx_mod_read_word(CAMERA + 0x14);
    v.cam[1] = (int32_t)psx_mod_read_word(CAMERA + 0x18);
    v.cam[2] = (int32_t)psx_mod_read_word(CAMERA + 0x1C);
    s_cursor = mem;
    s_end = mem + FRAME_BYTES;
    s_ot = base + 4u * LAYER_RANK;
    /* the layers, last to first (the first put last: drawn first) */
    for (int k = (int)s_nlayers - 1; k >= 0; k--) {
        const Layer *l = &s_layers[k];
        double down = k == 0 ? sun_down(l) : 0;
        if (s_parallax && l->distance > 0) down += (double)s_parallax / l->distance;
        /* the layer on the sand (its bottom where the second gradient starts:
         * Fiji's sea, its waves) goes down with it: no blue between */
        if (s_sand && l->bottom == s_sky_at[2]) down += s_sand;
        if (s_far && l->distance < 0) down += s_far;
        for (int p = 0; p < l->places; p++) {
            unsigned c = l->ring[p];
            if (c >= s_ncells) continue;
            double x0 = (double)p * l->width - l->shift;
            double a0 = x0 * ANGLE - turn, a1 = (x0 + l->width) * ANGLE - turn;
            /* only round the view: within 90 degrees of its centre, kx times as far round */
            double d0 = remainder(a0 - v.centre, TAU), d1 = d0 + (a1 - a0);
            if (fabs((d0 + d1) / 2 * v.kx) > TAU / 4) continue;
            cell_quad(&v, l, &s_cells[c], v.centre + d0 * v.kx, v.centre + d1 * v.kx, down);
        }
    }
    /* the sky, put last: drawn first, over the engine's far background */
    uint8_t sky[4][3];
    sky_now(sky);
    double sx, horizon;
    if (!screen_of(&v, v.centre, 0, &sx, &horizon)) horizon = 240;
    sky_draw(&v, sky);
    if (s_floor) floor_draw(&v, base);
    if (s_effect == 2) snow_draw(&v, base, v.centre / ANGLE, horizon);
    if (s_effect == 3) screen_copy(base, parity);
}
