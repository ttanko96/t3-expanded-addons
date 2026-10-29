/* TMST compat (tmst_compat.h): the features found at build time. */
#include "tmst_compat.h"
#include <stdio.h>
#include <string.h>

/* Weak: a release without them still links, and nothing counts as taken. */
extern int tekken3_stage_high_vram_used(int x, int y, int w, int h) __attribute__((weak));
extern unsigned tekken3_stage_high_vram_generation(void) __attribute__((weak));

int tmst_compat_high_vram_taken(int x, int y, int w, int h) {
    return tekken3_stage_high_vram_used ? tekken3_stage_high_vram_used(x, y, w, h) : 0;
}
unsigned tmst_compat_high_vram_generation(void) {
    return tekken3_stage_high_vram_generation ? tekken3_stage_high_vram_generation() : 0;
}

/* psxrecomp/runtime/src/gpu.c: the half's 1024 x 512 pixels, NULL before
 * psx_mod_gpu_high_vram_enable(). */
extern const uint16_t *gpu_high_vram_pixels(void) __attribute__((weak));

int tmst_compat_high_vram_read(int x, int y, int w, int h, uint16_t *out) {
    const uint16_t *half = gpu_high_vram_pixels ? gpu_high_vram_pixels() : NULL;
    if (!half || x < 0 || y < 512 || w <= 0 || h <= 0 || x + w > 1024 || y + h > 1024) return 0;
    for (int row = 0; row < h; row++)
        memcpy(out + (size_t)row * (size_t)w, half + (size_t)(y - 512 + row) * 1024u + (size_t)x, (size_t)w * 2);
    return 1;
}

unsigned tmst_compat_available(void) {
    unsigned m = 0;
#ifdef TMST_HAS_ACTIVATION_PLUGIN
    m |= TMST_NEED_ACTIVATION;
#endif
#ifdef TMST_HAS_VBLANK_PLUGIN
    m |= TMST_NEED_VBLANK;
#endif
#ifdef TMST_HAS_GUEST_MEMORY
    m |= TMST_NEED_GUEST_MEMORY;
#endif
#ifdef TMST_HAS_HIGH_VRAM_UPLOAD
    m |= TMST_NEED_HIGH_VRAM;
#endif
#ifdef TMST_HAS_GAME_OPTIONS
    m |= TMST_NEED_GAME_OPTIONS;
#endif
    return m;
}

const char *tmst_compat_describe(unsigned mask) {
    static const char *const names[] = { "activation plugins", "VBlank plugins", "guest memory",
                                         "upper VRAM uploads", "GAME OPTION rows" };
    static char out[160];
    out[0] = 0;
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (!(mask >> i & 1)) continue;
        if (out[0]) strncat(out, ", ", sizeof out - strlen(out) - 1);
        strncat(out, names[i], sizeof out - strlen(out) - 1);
    }
    return out[0] ? out : "nothing";
}
