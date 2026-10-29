/* TMST core: the picture over the game (tmst_overlay.h).
 *
 * Where: the upper VRAM half at x 0..511, y 768..1023 (two 256-texel strips
 * of 15-bit texels, texture pages 0 and 4 with the upper-half bit). The
 * Expanded's stage engine may use the same half; while it says the area is
 * taken (src/compat), nothing is drawn rather than drawing its pixels.
 *
 * When: the runtime calls gpu_ws_prepass_linked_list() at the start of every
 * linked-list GPU DMA, before the walk (psxrecomp/runtime/src/dma.c). Our
 * wrapper walks the list once and, on the frame's long list (the ordering
 * table, hundreds of nodes), links our packet after its last node: drawn
 * last, so on top. Our node always ends the list (0xFFFFFF), so a list
 * that is not cleared never loops; hidden, the node stays linked but empty.
 *
 * The packet: one node, per strip an E1 (texture page, 15-bit, upper-half bit
 * 11) and a raw-texture sprite (0x65). Its memory is GPU-DMA memory named as
 * a trusted upper-half source (psx_mod_gpu_high_vram_source), the only
 * packets the runtime lets sample the upper half. */
#include "tmst_overlay.h"
#include "tmst_core.h"
#include "tmst_compat.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { VX = 0, VY = 768, STRIP = 256, STRIPS = TMST_OVERLAY_MAX_W / STRIP,
       NODE_WORDS = 1 + STRIPS * 5, LONG_LIST = 256, MAX_WALK = 200000 };

static uint32_t packet;                 /* guest address of our node */
static int ready, failed;
static int shown, sx, sy, pw, ph;
static uint16_t pixels[TMST_OVERLAY_MAX_W * TMST_OVERLAY_MAX_H];
static int dirty;
static unsigned vram_generation;        /* the stage engine's, at our upload */

static int setup(void) {
    if (ready || failed) return ready;
    if (!psx_mod_gpu_high_vram_enable()) { failed = 1; tmst_log("overlay: no upper VRAM half"); return 0; }
    packet = psx_mod_alloc_gpu_dma_memory(NODE_WORDS * 4, 16);
    if (!packet || !psx_mod_gpu_high_vram_source(packet, NODE_WORDS * 4)) {
        failed = 1; tmst_log("overlay: no packet memory"); return 0;
    }
    psx_mod_write_word(packet, 0x00FFFFFFu);    /* empty, end of list */
    ready = 1;
    return 1;
}

static uint16_t texel(uint32_t argb) {
    if ((argb >> 24) < 128) return 0;           /* 0: see-through */
    unsigned r = (argb >> 19) & 31, g = (argb >> 11) & 31, b = (argb >> 3) & 31;
    return (uint16_t)(0x8000u | b << 10 | g << 5 | r);
}

int tmst_overlay_show(const uint32_t *argb, int w, int h, int x, int y) {
    if (!argb || w <= 0 || h <= 0 || w > TMST_OVERLAY_MAX_W || h > TMST_OVERLAY_MAX_H || !setup()) return 0;
    uint16_t next[TMST_OVERLAY_MAX_W];
    int changed = w != pw || h != ph;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) next[col] = texel(argb[row * w + col]);
        if (memcmp(pixels + row * w, next, (size_t)w * 2)) { memcpy(pixels + row * w, next, (size_t)w * 2); changed = 1; }
    }
    pw = w; ph = h; sx = x; sy = y; shown = 1;
    if (changed) dirty = 1;
    return 1;
}

void tmst_overlay_move(int x, int y) { sx = x; sy = y; }
void tmst_overlay_hide(void) { shown = 0; }
int tmst_overlay_visible(void) { return shown; }

/* The node for this frame: empty while hidden or while the area is taken. */
static void write_node(uint32_t next) {
    unsigned words = 0;
    if (shown && !tmst_compat_high_vram_taken(VX, VY, pw, ph)) {
        unsigned gen = tmst_compat_high_vram_generation();
        if (dirty || gen != vram_generation) {
            psx_mod_gpu_high_vram_upload(VX, VY, pw, ph, pixels);
            dirty = 0; vram_generation = gen;
        }
        for (int s = 0; s < STRIPS && s * STRIP < pw; s++) {
            int w = pw - s * STRIP < STRIP ? pw - s * STRIP : STRIP;
            unsigned page = (unsigned)(VX + s * STRIP) / 64u;
            uint32_t e1 = 0xE1000000u | page | 1u << 4 /* y 256 */ | 2u << 7 /* 15-bit */ | 1u << 10 | 1u << 11;
            uint32_t at = packet + 4 + words * 4;
            int x = sx + s * STRIP;
            psx_mod_write_word(at, e1);
            psx_mod_write_word(at + 4, 0x65808080u);
            psx_mod_write_word(at + 8, (uint32_t)(sy & 0xFFFF) << 16 | (uint32_t)(x & 0xFFFF));
            psx_mod_write_word(at + 12, 0u);                         /* u 0, v 0 (rows 768..) */
            psx_mod_write_word(at + 16, (uint32_t)ph << 16 | (uint32_t)w);
            words += 5;
        }
    }
    psx_mod_write_word(packet, words << 24 | (next & 0x00FFFFFFu));
}

static unsigned frame, linked_frame = ~0u;

/* One walk of the list about to be drawn: our node already in it (a list not
 * cleared since) gets this frame's content; otherwise the frame's first long
 * list gets our node after its last one. */
static void link_into(uint32_t start) {
    uint32_t mine = packet & 0x00FFFFFFu, at = start & 0x00FFFFFFu, last = 0;
    unsigned n = 0;
    while (at != 0x00FFFFFFu && n < MAX_WALK) {
        if (at == mine) {
            if (linked_frame != frame) { linked_frame = frame; write_node(0x00FFFFFFu); }
            return;
        }
        last = at;
        at = psx_mod_read_word(0x80000000u | at) & 0x00FFFFFFu;
        n++;
    }
    if (n < LONG_LIST || n >= MAX_WALK || linked_frame == frame) return;
    linked_frame = frame;
    write_node(0x00FFFFFFu);
    uint32_t tag = psx_mod_read_word(0x80000000u | last);
    psx_mod_write_word(0x80000000u | last, (tag & 0xFF000000u) | mine);
}

extern void __real_gpu_ws_prepass_linked_list(uint32_t start_addr);
void __wrap_gpu_ws_prepass_linked_list(uint32_t start_addr) {
    if (ready && psx_mod_game_started()) link_into(start_addr);
    __real_gpu_ws_prepass_linked_list(start_addr);
}

void tmst_overlay_vblank(void) { frame++; }
