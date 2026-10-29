/* TMST core: an ARGB picture over the game's picture, in both renderers and
 * without touching them. The picture goes into the upper VRAM half
 * (psx_mod_gpu_high_vram_upload) and is drawn by textured sprites linked at
 * the end of the frame's display list, so it lands on top of everything.
 *
 * One picture at a time, up to TMST_OVERLAY_MAX_W x TMST_OVERLAY_MAX_H, in
 * the game's screen coordinates (Tekken 3 draws 368 x 480 in fights, 320 x 240
 * on most menus: the caller knows its screen). Alpha below 128 is see-through,
 * anything else opaque. */
#ifndef TMST_OVERLAY_H
#define TMST_OVERLAY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TMST_OVERLAY_MAX_W = 512, TMST_OVERLAY_MAX_H = 256 };

/* Shows `argb` (w x h, copied) at screen (x, y) from the next frame on, until
 * hidden. The same pixels again cost nothing; changed ones are uploaded once.
 * Returns 0 when the picture cannot be shown (too big, no upper VRAM). */
int tmst_overlay_show(const uint32_t *argb, int w, int h, int x, int y);
/* Moves the shown picture. */
void tmst_overlay_move(int x, int y);
void tmst_overlay_hide(void);
int tmst_overlay_visible(void);
/* Once per VBlank, from the VBlank plugin of the mod that shows the picture:
 * the picture is linked once per frame. */
void tmst_overlay_vblank(void);

#ifdef __cplusplus
}
#endif
#endif
