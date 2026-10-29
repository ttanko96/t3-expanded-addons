/* TMST core: our drawing over the game (tmst_overlay.h).
 *
 * Where the picture is: the upper VRAM half from x 0; a picture up to 256
 * rows tall at y 768..1023, a taller one from y 512. Texture pages of 256 x 256
 * 15-bit texels: one sprite or quad per 256 x 256 tile, each naming its page
 * with the upper-half bit 11. What the area held is kept before our upload
 * and given back on hide (others' pixels that the stage engine does not
 * report live there too); while the stage engine says the area is its own,
 * nothing is drawn unless the picture borrows it (TMST_OVERLAY_BORROW).
 *
 * When: the runtime calls gpu_ws_prepass_linked_list() at the start of every
 * linked-list GPU DMA, before the walk (psxrecomp/runtime/src/dma.c). Our
 * wrapper walks each list once, following the drawing mode the GPU will have
 * (E1, and textured polygons' texture page), lets the mod's visitor see the
 * game's nodes, and, on the frame's first long list (the ordering table,
 * hundreds of nodes), links our chain after its last node. Our chain ends the
 * list (0xFFFFFF), so a list that is not cleared never loops; its last word
 * puts back the drawing mode the list had before it, so the game's later
 * primitives (and the upper-half sprite page the runtime keeps) are theirs
 * again. Hidden, the chain goes out of any list it is found in.
 *
 * The chain lives in GPU-DMA memory named as a trusted upper-half source
 * (psx_mod_gpu_high_vram_source), the only packets the runtime lets sample
 * the upper half. */
#include "tmst_overlay.h"
#include "tmst_core.h"
#include "tmst_compat.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { VX = 0, TILE = 256, BUF_WORDS = 4096, BUF_BYTES = BUF_WORDS * 4, NODE_MAX = 255,
       LONG_LIST = 256, MAX_WALK = 200000 };

static uint32_t packet;                 /* guest address of our chain's first node */
static int ready, failed;
static int shown, sx, sy, sw, sh, pw, ph, vy;
static unsigned show_flags;
static uint16_t pixels[TMST_OVERLAY_MAX_W * TMST_OVERLAY_MAX_H];
static int dirty;
static unsigned vram_generation;        /* the stage engine's, at our upload */
static uint16_t kept[TMST_OVERLAY_MAX_W * TMST_OVERLAY_MAX_H];
static int kept_x, kept_y, kept_w, kept_h;
static unsigned frame, linked_frame = ~0u, uploaded_frame;
static TmstPrimsBuilder builder;
static TmstNodeVisitor visitor;

void tmst_overlay_set_prims(TmstPrimsBuilder b) { builder = b; }
void tmst_overlay_set_visitor(TmstNodeVisitor v) { visitor = v; }

/* At the mod's activation (start-up): the OpenGL renderer builds its upper-
 * half sampling only when the half exists before its context does. */
int tmst_overlay_prepare(void) { return psx_mod_gpu_high_vram_enable(); }

static int setup(void) {
    if (ready || failed) return ready;
    if (!psx_mod_gpu_high_vram_enable()) { failed = 1; tmst_log("overlay: no upper VRAM half"); return 0; }
    packet = psx_mod_alloc_gpu_dma_memory(BUF_BYTES, 16);
    if (!packet || !psx_mod_gpu_high_vram_source(packet, BUF_BYTES)) {
        failed = 1; tmst_log("overlay: no packet memory"); return 0;
    }
    psx_mod_write_word(packet, 0x00FFFFFFu);    /* empty, end of list */
    ready = 1;
    return 1;
}

static uint16_t texel(uint32_t argb) {
    unsigned a = argb >> 24;
    if (a < 128) return 0;                      /* 0: see-through */
    unsigned r = (argb >> 19) & 31, g = (argb >> 11) & 31, b = (argb >> 3) & 31;
    uint16_t c = (uint16_t)(b << 10 | g << 5 | r);
    if (a < 255) return (uint16_t)(0x8000u | c);   /* STP: half see-through */
    return c ? c : 0x0421u;                     /* opaque; black must not read as see-through */
}

static void give_back(void) {
    if (!kept_w) return;
    psx_mod_gpu_high_vram_upload(kept_x, kept_y, kept_w, kept_h, kept);
    kept_w = 0;
}

static void keep(int x, int y, int w, int h) {
    if (tmst_compat_high_vram_read(x, y, w, h, kept)) { kept_x = x; kept_y = y; kept_w = w; kept_h = h; }
}

int tmst_overlay_show_scaled(const uint32_t *argb, int w, int h, int x, int y, int dw, int dh, unsigned flags) {
    if (!argb || w <= 0 || h <= 0 || dw <= 0 || dh <= 0 || w > TMST_OVERLAY_MAX_W || h > TMST_OVERLAY_MAX_H || !setup())
        return 0;
    int changed = w != pw || h != ph;
    if (changed && kept_w) give_back();        /* a new size: a new area */
    for (int row = 0; row < h; row++) {
        uint16_t *line = pixels + row * w;
        for (int col = 0; col < w; col++) {
            uint16_t t = texel(argb[row * w + col]);
            if (line[col] != t) { line[col] = t; changed = 1; }
        }
    }
    pw = w; ph = h; sx = x; sy = y; sw = dw; sh = dh; shown = 1; show_flags = flags;
    vy = h <= TILE ? 1024 - TILE : 512;
    if (changed) dirty = 1;
    return 1;
}
int tmst_overlay_show_ex(const uint32_t *argb, int w, int h, int x, int y, unsigned flags) {
    return tmst_overlay_show_scaled(argb, w, h, x, y, w, h, flags);
}
int tmst_overlay_show(const uint32_t *argb, int w, int h, int x, int y) {
    return tmst_overlay_show_scaled(argb, w, h, x, y, w, h, 0);
}

void tmst_overlay_move(int x, int y) { sx = x; sy = y; }
/* The area goes back to what it held: our pixels are no longer there, so
 * the next show uploads them again. */
void tmst_overlay_hide(void) { shown = 0; give_back(); dirty = 1; }
int tmst_overlay_visible(void) { return shown; }

/* ---- the chain's builder ---- */
struct TmstPrims { uint32_t head, at, end; unsigned words; };
static void prims_begin(TmstPrims *k) { k->head = packet; k->at = packet + 4; k->end = packet + BUF_BYTES; k->words = 0; }
void tmst_prim_room(TmstPrims *k, unsigned n) {
    if (k->words + n <= NODE_MAX || k->at + 4 + n * 4 > k->end) return;
    psx_mod_write_word(k->head, (uint32_t)k->words << 24 | (k->at & 0x00FFFFFFu));
    k->head = k->at; k->at += 4; k->words = 0;
}
void tmst_prim_word(TmstPrims *k, uint32_t w) {
    if (k->at + 4 >= k->end || k->words >= NODE_MAX) return;   /* the last word stays free */
    psx_mod_write_word(k->at, w); k->at += 4; k->words++;
}
static void prims_finish(TmstPrims *k) {
    psx_mod_write_word(k->head, (uint32_t)k->words << 24 | 0x00FFFFFFu);
}

/* The picture's tiles: 1:1 as sprites (an E1 for each page), scaled as
 * textured quads (each names its page). Raw texture, the half see-through
 * texels blended. */
static void picture_prims(TmstPrims *k) {
    int one = sw == pw && sh == ph;
    for (int ty = 0; ty * TILE < ph; ty++)
        for (int tx = 0; tx * TILE < pw; tx++) {
            int w = pw - tx * TILE < TILE ? pw - tx * TILE : TILE;
            int h = ph - ty * TILE < TILE ? ph - ty * TILE : TILE;
            unsigned page = (unsigned)(VX + tx * TILE) / 64u;
            unsigned ybit = (unsigned)(vy + ty * TILE - 512) / 256u & 1u;
            uint32_t tpage = page | ybit << 4 | 2u << 7 /* 15-bit */ | 1u << 11;
            if (one) {
                int x = sx + tx * TILE, y = sy + ty * TILE;
                tmst_prim_room(k, 5);
                tmst_prim_word(k, 0xE1000000u | tpage | 1u << 10);
                tmst_prim_word(k, 0x67808080u);                       /* raw sprite, half see-through texels */
                tmst_prim_word(k, tmst_xy(x, y));
                tmst_prim_word(k, 0u);
                tmst_prim_word(k, (uint32_t)h << 16 | (uint32_t)w);
            } else {
                int x0 = sx + tx * TILE * sw / pw, x1 = sx + (tx * TILE + w) * sw / pw;
                int y0 = sy + ty * TILE * sh / ph, y1 = sy + (ty * TILE + h) * sh / ph;
                uint32_t u1 = (uint32_t)(w > 255 ? 255 : w), v1 = (uint32_t)(h > 255 ? 255 : h);
                tmst_prim_room(k, 9);
                tmst_prim_word(k, 0x2E808080u);                       /* textured quad (x 1.0), half see-through texels */
                tmst_prim_word(k, tmst_xy(x0, y0)); tmst_prim_word(k, 0u);
                tmst_prim_word(k, tmst_xy(x1, y0)); tmst_prim_word(k, u1 | tpage << 16);
                tmst_prim_word(k, tmst_xy(x0, y1)); tmst_prim_word(k, v1 << 8);
                tmst_prim_word(k, tmst_xy(x1, y1)); tmst_prim_word(k, u1 | v1 << 8);
            }
        }
}

/* This frame's chain: empty while hidden; the picture left out while its
 * area is the stage engine's (unless borrowed). */
static void write_chain(uint32_t e1_before) {
    TmstPrims k; prims_begin(&k);
    if (shown) {
        int taken = tmst_compat_high_vram_taken(VX, vy, pw, ph);
        if (builder) builder(&k);
        if (!taken || (show_flags & TMST_OVERLAY_BORROW)) {
            unsigned gen = tmst_compat_high_vram_generation();
            /* Again every half second too: an upload of someone else's into
             * the area (an effect, a stage's animated texture) is undone. */
            if (dirty || gen != vram_generation || frame - uploaded_frame >= 30) {
                if (!kept_w || gen != vram_generation) keep(VX, vy, pw, ph);
                psx_mod_gpu_high_vram_upload(VX, vy, pw, ph, pixels);
                dirty = 0; vram_generation = gen; uploaded_frame = frame;
            }
            picture_prims(&k);
        }
        tmst_prim_room(&k, 1);
        tmst_prim_word(&k, e1_before);
    }
    prims_finish(&k);
}


/* The drawing mode as the GPU will have it: E1, which textured polygons set
 * too (their texture page word: bits 0-8 and 11), followed through every
 * list drawn. */
static uint32_t gpu_e1 = 0xE1000400u;

/* Words of the GP0 command at word i of a node of n words. */
static unsigned command_words(uint32_t node, unsigned i, unsigned n) {
    uint32_t w = psx_mod_read_word(node + 4 + i * 4);
    unsigned op = w >> 24;
    if (op >= 0x20 && op < 0x40) {                      /* polygons */
        unsigned verts = (op & 8) ? 4 : 3, tex = (op & 4) ? 1 : 0, shaded = (op & 0x10) ? 1 : 0;
        unsigned len = 1 + verts * (1 + tex) + shaded * (verts - 1);
        if (tex && i + len <= n) {
            unsigned at = shaded ? 5 : 4;               /* the second vertex's UV word: the texture page */
            uint32_t page = psx_mod_read_word(node + 4 + (i + at) * 4) >> 16;
            gpu_e1 = (gpu_e1 & ~0x9FFu) | (page & 0x9FFu);
        }
        return len;
    }
    if (op >= 0x40 && op < 0x60) {                      /* lines */
        unsigned shaded = (op & 0x10) ? 1 : 0;
        if (!(op & 8)) return 3 + shaded;
        unsigned k = i + 1;                            /* polyline: to its terminator */
        while (k < n && (psx_mod_read_word(node + 4 + k * 4) & 0xF000F000u) != 0x50005000u) k++;
        return k - i + 1;
    }
    if (op >= 0x60 && op < 0x80) return 2 + ((op & 4) ? 1 : 0) + ((op & 0x18) == 0 ? 1 : 0);
    if (op >= 0x80 && op < 0xA0) return 4;
    if (op >= 0xA0 && op < 0xC0) {
        uint32_t size = i + 2 < n ? psx_mod_read_word(node + 4 + (i + 2) * 4) : 0;
        return 3 + (((size & 0xFFFF) * (size >> 16) + 1) / 2);
    }
    if (op >= 0xC0 && op < 0xE0) return 3;
    if (op == 0x02) return 3;
    if (op == 0xE1) gpu_e1 = w;
    return 1;
}
static void follow(uint32_t node) {
    unsigned n = psx_mod_read_word(node) >> 24;
    for (unsigned i = 0; i < n;) i += command_words(node, i, n);
}

static int ours(uint32_t at) {
    uint32_t lo = packet & 0x00FFFFFFu;
    return at >= lo && at < lo + BUF_BYTES;
}

/* One walk of the list about to be drawn. Our chain already in it (a list
 * not cleared since) gets this frame's content, or goes out when hidden;
 * otherwise, shown, the frame's first long list gets it after its last node. */
static void link_into(uint32_t start) {
    uint32_t at = start & 0x00FFFFFFu, last = 0;
    unsigned n = 0;
    while (at != 0x00FFFFFFu && n < MAX_WALK) {
        uint32_t node = 0x80000000u | at;
        if (ours(at)) {
            if (!shown) {                               /* hidden: out of the list */
                if (last) {
                    uint32_t tag = psx_mod_read_word(0x80000000u | last);
                    psx_mod_write_word(0x80000000u | last, (tag & 0xFF000000u) | 0x00FFFFFFu);
                }
                psx_mod_write_word(packet, 0x00FFFFFFu);
            } else if (linked_frame != frame) {
                linked_frame = frame;
                write_chain(gpu_e1);
            }
            return;
        }
        if (shown && visitor) visitor(node);
        follow(node);
        last = at;
        at = psx_mod_read_word(node) & 0x00FFFFFFu;
        n++;
    }
    if (!shown || n < LONG_LIST || n >= MAX_WALK || linked_frame == frame) return;
    linked_frame = frame;
    write_chain(gpu_e1);
    uint32_t tag = psx_mod_read_word(0x80000000u | last);
    psx_mod_write_word(0x80000000u | last, (tag & 0xFF000000u) | (packet & 0x00FFFFFFu));
}

extern void __real_gpu_ws_prepass_linked_list(uint32_t start_addr);
void __wrap_gpu_ws_prepass_linked_list(uint32_t start_addr) {
    if (ready && psx_mod_game_started()) link_into(start_addr);
    __real_gpu_ws_prepass_linked_list(start_addr);
}

void tmst_overlay_vblank(void) { frame++; }
