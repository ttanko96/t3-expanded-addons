/* tmst.dev.noop: proves the add-on and its core work in the game, changing
 * nothing of the game itself. With its feature on:
 *   - the log: the version guard, activation, 600 VBlanks, every music the
 *     game starts (the chained 0x8006AE14, passed on unchanged);
 *   - a small "TMST ADD-ON" label in the top left corner (the overlay);
 *   - a short quiet beep when the sound starts (the audio mixer).
 * Off, none of it runs. Removed before the first release. */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "tmst_font.h"
#include "tmst_overlay.h"
#include "tmst_audio.h"
#include "tmst_music.h"
#include "mod_plugins.h"
#include <stdatomic.h>
#include <stdint.h>

enum { LABEL_W = 84, LABEL_H = 12 };
static int on;
static unsigned frames;
static uint32_t label[LABEL_W * LABEL_H];
static atomic_int beep_left = 0;       /* samples of the beep still to mix */
static unsigned beep_phase;

static int music(int id) {
    tmst_log("tmst.dev.noop: the game starts music %d", id);
    return id;
}

static void beep(int16_t *stereo, int count, int rate) {
    int left = atomic_load(&beep_left);
    if (left <= 0 || rate <= 0) return;
    int n = count < left ? count : left;
    unsigned step = (unsigned)(880ull * 65536 / (unsigned)rate);
    for (int i = 0; i < n; i++) {
        int s = (beep_phase & 0x8000) ? 1200 : -1200;     /* square, quiet */
        beep_phase += step;
        for (int c = 0; c < 2; c++) {
            int v = stereo[i * 2 + c] + s;
            stereo[i * 2 + c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    }
    atomic_store(&beep_left, left - n);
}

static void activate(void) {
    on = tmst_guard("tmst.dev.noop", TMST_NEED_ACTIVATION | TMST_NEED_VBLANK | TMST_NEED_HIGH_VRAM);
    if (!on) return;
    tmst_log("tmst.dev.noop is active");
    tmst_music_set_hook(music);
    tmst_fill_argb(label, LABEL_W, LABEL_H, 0, 0, LABEL_W, LABEL_H, 0xFF202040u);
    tmst_text_argb(label, LABEL_W, LABEL_H, 3, 2, "TMST ADD-ON", 1, 0xFFFFD040u);
    atomic_store(&beep_left, 13230);    /* 0.3 s at 44.1 kHz */
    if (!tmst_audio_add_mixer(beep)) tmst_log("tmst.dev.noop: no mixer slot");
}

static void tick(void) {
    if (!on) return;
    tmst_overlay_vblank();
    if (!tmst_overlay_visible() && !tmst_overlay_show(label, LABEL_W, LABEL_H, 8, 8))
        { on = 0; tmst_log("tmst.dev.noop: the overlay cannot be shown"); return; }
    if (++frames == 600) tmst_log("tmst.dev.noop: 600 VBlanks seen, audio at %d Hz", tmst_audio_rate());
}

PSX_MOD_CONSTRUCTOR(tmst_register_noop) {
    (void)psx_mod_register_activation_plugin("tmst.dev.noop", activate);
    (void)psx_mod_register_vblank_plugin("tmst.dev.noop", tick);
}
