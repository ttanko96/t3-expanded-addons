/* TMST compat (tmst_compat.h): the features found at build time. */
#include "tmst_compat.h"
#include "gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Weak: a release without them still links, and nothing counts as taken. */
extern int tekken3_stage_high_vram_used(int x, int y, int w, int h) __attribute__((weak));
extern unsigned tekken3_stage_high_vram_generation(void) __attribute__((weak));
/* The upper half's other users in the Expanded: TTT Hitspark (its fixed
 * cells, the common sparks' cells around the stage and their palettes) and
 * Unknown's wolf (tekken3_ttt1_combat.c, tekken3_ttt1_wolf.c). */
extern int tekken3_hit_effect_high_vram_used(int x, int y, int w, int h) __attribute__((weak));
extern int tekken3_ttt1_wolf_high_vram_used(int x, int y, int w, int h) __attribute__((weak));

int tmst_compat_high_vram_effects(int x, int y, int w, int h) {
    return (tekken3_hit_effect_high_vram_used && tekken3_hit_effect_high_vram_used(x, y, w, h)) ||
           (tekken3_ttt1_wolf_high_vram_used && tekken3_ttt1_wolf_high_vram_used(x, y, w, h));
}
int tmst_compat_high_vram_taken(int x, int y, int w, int h) {
    return (tekken3_stage_high_vram_used && tekken3_stage_high_vram_used(x, y, w, h)) ||
           tmst_compat_high_vram_effects(x, y, w, h);
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

extern int tekken3_arcade_stages_t3_stage(void) __attribute__((weak));
int tmst_compat_arcade_stage(void) {
    int s = tekken3_arcade_stages_t3_stage ? tekken3_arcade_stages_t3_stage() : -1;
    return s >= 0 && s <= 12 ? s : -1;
}

/* mod_plugins.h; weak: a release without it has no wide screen. */
extern int32_t psx_mod_widescreen_x_margin(void) __attribute__((weak));
int tmst_compat_widescreen_margin(void) {
    int m = psx_mod_widescreen_x_margin ? (int)psx_mod_widescreen_x_margin() : 0;
    return m > 0 && m < 256 ? m : 0;
}

extern int ws_nw_extra(void) __attribute__((weak));
int tmst_compat_widescreen_side(void) {
    int e = ws_nw_extra ? ws_nw_extra() / 2 : -1;
    return e >= 0 && e < 256 ? e : tmst_compat_widescreen_margin();
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

/* The runtime's display readout (gpu.h, gpu_render.h, gpu_gl_renderer.h:
 * not the mod API; weak, absent = no photos), as its debug server's
 * screenshot_hires and wide_shot read it. */
extern void gpu_get_display_info(GpuDisplayInfo *out) __attribute__((weak));
extern int gr_scale(void) __attribute__((weak));
extern int gr_read_display_hires(uint32_t *out, int pitch, int dx, int dy, int dw, int dh) __attribute__((weak));
extern int gr_render_display(uint32_t *out, int pitch, int dx, int dy, int dw, int dh) __attribute__((weak));
extern int gr_wide_supported(void) __attribute__((weak));
extern int gr_render_wide_display(uint32_t *out, int pitch, int base_x, int disp_y, int disp_h) __attribute__((weak));
extern void gl_renderer_sync_cpu(void) __attribute__((weak));

extern int gr_backend(void) __attribute__((weak));     /* GrBackend: 0 software, 1 OpenGL, 2 Vulkan */
int tmst_compat_renderer(void) {
    int b = gr_backend ? gr_backend() : -1;
    return b >= 0 && b <= 2 ? b : TMST_RENDERER_UNKNOWN;
}

int tmst_compat_read_frame(uint32_t **out, int *w, int *h, int *scale, int *side) {
    *out = NULL; *w = *h = *side = 0; *scale = 1;
    if (!gpu_get_display_info || !gr_scale || !gr_render_display) return 0;
    GpuDisplayInfo di;
    gpu_get_display_info(&di);
    if (di.disabled || di.depth24 || !di.width || !di.height) return 0;
    int s = gr_scale(); if (s < 1) s = 1;
    int dw = (int)(di.width > 640 ? 640 : di.width), dh = (int)(di.height > 512 ? 512 : di.height);
    int extra = ws_nw_extra ? ws_nw_extra() : 0;
    if (extra > 0 && gr_wide_supported && gr_wide_supported() && gr_render_wide_display) {
        /* 16:9: the composited wide frame, sides and all */
        int ow = (dw + extra) * s, oh = dh * s;
        uint32_t *buf = malloc((size_t)ow * (size_t)oh * 4);
        if (buf) {
            if (gl_renderer_sync_cpu) gl_renderer_sync_cpu();
            if (gr_render_wide_display(buf, ow * 4, (int)di.display_x, (int)di.display_y, dh) > 0) {
                *out = buf; *w = ow; *h = oh; *scale = s; *side = extra / 2 * s;
                goto opaque;
            }
            free(buf);
        }
    }
    {
        int ow = dw * s, oh = dh * s;
        uint32_t *buf = malloc((size_t)ow * (size_t)oh * 4);
        if (!buf) return 0;
        if (!(gr_read_display_hires && gr_read_display_hires(buf, ow * 4, (int)di.display_x, (int)di.display_y, dw, dh))) {
            s = 1; ow = dw; oh = dh;                 /* no hi-res surface: the native frame */
            if (!gr_render_display(buf, ow * 4, (int)di.display_x, (int)di.display_y, dw, dh)) { free(buf); return 0; }
        }
        *out = buf; *w = ow; *h = oh; *scale = s;
    }
opaque:
    for (size_t i = 0, n = (size_t)*w * (size_t)*h; i < n; i++) (*out)[i] |= 0xFF000000u;
    return 1;
}
