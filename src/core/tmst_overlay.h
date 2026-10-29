/* TMST core: our own drawing over the game's picture, in both renderers and
 * without touching them.
 *
 * Each frame the core links a chain of packets of its own at the end of the
 * frame's display list (drawn last, so on top):
 *   - the mod's native primitives, if it set a builder (tmst_overlay_set_prims):
 *     rectangles, polygons, the game's own font from its VRAM;
 *   - the picture: an ARGB picture of the mod's, uploaded into the upper VRAM
 *     half and drawn 1:1 (sprites) or scaled onto a screen rectangle (textured
 *     quads: a picture with more texels than the rectangle has pixels is sharp
 *     where the renderer draws at a higher resolution, the OpenGL one).
 * One picture at a time, up to TMST_OVERLAY_MAX_W x TMST_OVERLAY_MAX_H, in
 * the game's screen coordinates (368 x 480 in fights and on the selectors).
 * Alpha below 128 is see-through, 128..254 half see-through (the GPU's
 * B/2 + F/2), 255 opaque. */
#ifndef TMST_OVERLAY_H
#define TMST_OVERLAY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TMST_OVERLAY_MAX_W = 1024, TMST_OVERLAY_MAX_H = 512 };

/* From the activation of every mod that draws (start-up): the upper VRAM half
 * must exist before the OpenGL renderer starts, or it never samples it. */
int tmst_overlay_prepare(void);
enum {
    /* The area may hold the stage engine's pixels (a stage pack stays loaded
     * after its fight): take it anyway. Only for screens without a stage
     * (menus, selectors). The area's pixels are always kept and given back. */
    TMST_OVERLAY_BORROW = 1
};

/* Shows `argb` (w x h texels, copied) from the next frame on, until hidden,
 * on the screen rectangle (x, y, dw, dh); dw x dh = w x h draws it 1:1.
 * The same pixels again cost nothing; changed ones are uploaded once.
 * Returns 0 when the picture cannot be shown (too big, no upper VRAM). */
int tmst_overlay_show_scaled(const uint32_t *argb, int w, int h, int x, int y, int dw, int dh, unsigned flags);
int tmst_overlay_show_ex(const uint32_t *argb, int w, int h, int x, int y, unsigned flags);
int tmst_overlay_show(const uint32_t *argb, int w, int h, int x, int y);
/* Moves the shown picture. */
void tmst_overlay_move(int x, int y);
/* Hidden from the next frame; the area's pixels are given back now. */
void tmst_overlay_hide(void);
int tmst_overlay_visible(void);
/* Once per VBlank, from the VBlank plugin of the mod that shows the picture:
 * the packets are linked once per frame. */
void tmst_overlay_vblank(void);

/* Native primitives, built each frame into the core's packets (several
 * nodes as needed). A builder writes GP0 words; room(n) before a primitive of
 * n words keeps it in one node. */
typedef struct TmstPrims TmstPrims;
void tmst_prim_room(TmstPrims *k, unsigned words);
void tmst_prim_word(TmstPrims *k, uint32_t word);
static inline uint32_t tmst_xy(int x, int y) { return ((uint32_t)(y & 0xffff) << 16) | (uint32_t)(x & 0xffff); }
/* A builder, run while the picture is shown (before the picture is drawn). */
typedef void (*TmstPrimsBuilder)(TmstPrims *k);
void tmst_overlay_set_prims(TmstPrimsBuilder builder);
/* A visitor of each node of the game's lists (not ours), called while the
 * picture is shown, before the GPU draws the node: a mod may move the game's
 * primitives out of sight. */
typedef void (*TmstNodeVisitor)(uint32_t node);
void tmst_overlay_set_visitor(TmstNodeVisitor visitor);

#ifdef __cplusplus
}
#endif
#endif
