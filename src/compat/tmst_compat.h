/* TMST compat: the one place that knows how the Expanded is built inside.
 * Everything that is not a stable surface (retail guest addresses, the
 * manifest format) goes through here, so that a new Expanded release is
 * fixed in this folder only (docs/COMPAT.md). */
#ifndef TMST_COMPAT_H
#define TMST_COMPAT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* What a mod may need from the Expanded (tmst_guard's mask). */
enum {
    TMST_NEED_ACTIVATION = 1u << 0,      /* psx_mod_register_activation_plugin */
    TMST_NEED_VBLANK = 1u << 1,          /* psx_mod_register_vblank_plugin */
    TMST_NEED_GUEST_MEMORY = 1u << 2,    /* psx_mod_alloc_guest_memory */
    TMST_NEED_HIGH_VRAM = 1u << 3,       /* psx_mod_gpu_high_vram_upload */
    TMST_NEED_GAME_OPTIONS = 1u << 4,    /* tekken3_game_options_add */
};

/* The Expanded's stage engine in the upper VRAM half (tekken3_arcade_stages.c,
 * found at link time, absent = nothing taken): nonzero when the rectangle
 * holds its pixels now; its generation changes whenever it uploads. */
int tmst_compat_high_vram_taken(int x, int y, int w, int h);
unsigned tmst_compat_high_vram_generation(void);
/* The upper half's pixels of a rectangle (y 512..1023) into `out` (w x h);
 * 0 when this runtime cannot read them back (gpu_high_vram_pixels). */
int tmst_compat_high_vram_read(int x, int y, int w, int h, uint16_t *out);

/* The TMST_NEED_* this build found (src/compat/tmst_features.cmake). */
unsigned tmst_compat_available(void);
/* The names of the needs in `mask`, for the log. */
const char *tmst_compat_describe(unsigned mask);

#ifdef __cplusplus
}
#endif
#endif
