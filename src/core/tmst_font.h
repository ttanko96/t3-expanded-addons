/* TMST core: text into ARGB pictures, with the add-on's own 5x7 font (drawn
 * for TMST, upper case, digits and ASCII punctuation; lower case is shown in
 * upper case). Each character is a 6x8 cell, times `scale`. */
#ifndef TMST_FONT_H
#define TMST_FONT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TMST_FONT_CELL_W = 6, TMST_FONT_CELL_H = 8 };

/* Width in pixels of `text` at `scale`. */
int tmst_text_width(const char *text, int scale);
/* `text` into the w x h ARGB picture at (x, y), clipped. */
void tmst_text_argb(uint32_t *dst, int w, int h, int x, int y, const char *text, int scale, uint32_t argb);
/* A filled rectangle, clipped. */
void tmst_fill_argb(uint32_t *dst, int w, int h, int x, int y, int rw, int rh, uint32_t argb);

#ifdef __cplusplus
}
#endif
#endif
