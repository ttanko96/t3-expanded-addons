/* GAME OPTION's mod rows, with room for more of them: the Expanded's
 * src/tekken3_game_options.c (its copy, the same rules; PolyForm
 * Noncommercial, Matthew Stan) taken over whole by --wrap of its six
 * functions, its MAX_ROWS 8 made ROWS_MAX here. The Expanded's own rows
 * (CPU MOVESET, CPU COSTUME, STAGE SELECT, STAGES, PS2 EFFECTS) and ours
 * (TEKKEN 2 STAGES, TEKKEN 1 STAGES, WIREFRAME, BIG HEAD) are nine: its
 * eight refused the last one added, Big Head Mode's (the user's report,
 * 2026-11-19: "add an override that allows more rows"). The Expanded's copy
 * then holds no row and does nothing. Its file is never changed.
 *
 * The page is data in the options overlay: a stack of menu levels at
 * 0x800EAD24, 0x34 bytes each (+0 cursor, +0x14 first row's y, +0x18 row
 * spacing, +0x20 rows, +0x28 item count), the level shown in the byte
 * 0x800EC020. GAME OPTION is level 1: nine 20-byte rows from 0x800B908C,
 * then EXIT. A row is +0 the value byte's address, +4 its name, +8 its label
 * list, +12 1 (a row of values), +13 the number of values, +15 the default
 * (drawn in green), +16 2, +18 the modes lit in the box on the right.
 * Eleven items fill the screen; with more, the game is given a window of
 * eleven out of the whole list (the game's nine rows copied again every
 * frame, the mods' rows, EXIT) that follows the cursor (the game's sound
 * 0x546C, 0x800753BC, return 0x800DC890, the new cursor in s0). Arrows over
 * and under the list (drawn in the Expanded's display list,
 * tekken3_native_moves.c, through tekken3_game_options_arrows).
 *
 * The STAGES row's TEKKEN 1 and TEKKEN 2 (after their values, issue #11):
 * src/tekken2/tmst_tekken2_stages.c's hooks, when linked (weak). */
#include "tmst_core.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef TMST_HAS_GAME_OPTIONS
enum { SCREEN = 0x800ae204, OPTIONS = 5, LEVEL = 0x800ec020, ENTRY = 0x800ead58,
       DESCRIPTOR = ENTRY + 0x20, ROWS = 0x800b908c, GAME_ROWS = 9, ROW_SIZE = 20,
       GAME_ITEMS = GAME_ROWS + 1, VISIBLE = 11, ROWS_MAX = 16, MAX_LABELS = 16,
       MOVE_SOUND = 0x546c, MOVED = 0x800dc890, PAD = 0x800adf00 };

static uint32_t extra[ROWS_MAX], value_byte[ROWS_MAX];
static unsigned extras;
static uint32_t all, window;             /* the whole list; the eleven the game sees */
static unsigned top;                     /* the whole list's item at the window's top */

static uint32_t text(const char *s) {
    uint32_t n = (uint32_t)strlen(s) + 1, at = psx_mod_alloc_guest_memory(n, 4);
    for (uint32_t i = 0; at && i < n; i++) psx_mod_write_byte(at + i, (uint8_t)s[i]);
    return at;
}

/* A row added (the Expanded's tekken3_game_options_add). */
int tmst_game_options_add(const char *name, const char *const *labels, unsigned count,
                          unsigned value, unsigned default_value, unsigned modes) {
    if (extras >= ROWS_MAX || !count || count > MAX_LABELS) {
        tmst_log("game options: no room for the %s row (%u rows)", name, extras);
        return -1;
    }
    if (!all) {
        all = psx_mod_alloc_guest_memory((GAME_ITEMS + ROWS_MAX) * ROW_SIZE, 4);
        window = psx_mod_alloc_guest_memory(VISIBLE * ROW_SIZE, 4);
        if (!all || !window) { all = window = 0; return -1; }
    }
    uint32_t row = psx_mod_alloc_guest_memory(ROW_SIZE, 4), byte = psx_mod_alloc_guest_memory(4, 4);
    uint32_t list = psx_mod_alloc_guest_memory(count * 4, 4), title = text(name);
    if (!row || !byte || !list || !title) return -1;
    for (unsigned i = 0; i < count; i++) {
        uint32_t label = text(labels[i]);
        if (!label) return -1;
        psx_mod_write_word(list + i * 4, label);
    }
    psx_mod_write_word(byte, value < count ? value : 0);
    psx_mod_write_word(row, byte);
    psx_mod_write_word(row + 4, title);
    psx_mod_write_word(row + 8, list);
    psx_mod_write_word(row + 12, 1u | count << 8 | (default_value < count ? default_value : 0) << 24);
    psx_mod_write_word(row + 16, (modes & 0xffffu) << 16 | 2u);
    extra[extras] = row; value_byte[extras] = byte;
    fprintf(stderr, "Game options: %s row added to GAME OPTION (%u values, TMST's room for %d)\n", name, count,
            ROWS_MAX);
    return (int)extras++;
}

int tmst_game_options_value(int row) {
    if (row < 0 || (unsigned)row >= extras) return -1;
    return psx_mod_read_byte(value_byte[row]);
}

/* src/tekken2/tmst_tekken2_stages.c: the STAGES row with TEKKEN 1, 2 (-2: not
 * that row, add it as it is), and the value it reads (its own value turned
 * into theirs). */
int tmst_tekken2_options_add(const char *name, const char *const *labels, unsigned count,
                             unsigned value, unsigned default_value, unsigned modes) __attribute__((weak));
int tmst_tekken2_options_value(int row, int v) __attribute__((weak));

int __wrap_tekken3_game_options_add(const char *name, const char *const *labels, unsigned count,
                                    unsigned value, unsigned default_value, unsigned modes) {
    if (tmst_tekken2_options_add) {
        int row = tmst_tekken2_options_add(name, labels, count, value, default_value, modes);
        if (row != -2) return row;
    }
    return tmst_game_options_add(name, labels, count, value, default_value, modes);
}
int __wrap_tekken3_game_options_value(int row) {
    int v = tmst_game_options_value(row);
    return tmst_tekken2_options_value ? tmst_tekken2_options_value(row, v) : v;
}

static unsigned items(void) { return GAME_ITEMS + extras; }
static unsigned shown(void) { return items() < VISIBLE ? items() : VISIBLE; }

static void copy_row(uint32_t to, uint32_t from) {
    for (unsigned i = 0; i < ROW_SIZE; i += 4) {
        uint32_t w = psx_mod_read_word(from + i);
        if (psx_mod_read_word(to + i) != w) psx_mod_write_word(to + i, w);
    }
}
static void fill_window(void) {
    for (unsigned i = 0; i < shown(); i++) copy_row(window + i * ROW_SIZE, all + (top + i) * ROW_SIZE);
}

/* The game's page: its overlay's own table (first row the difficulty, last
 * the speaker), or our window over it. */
static int page_ours(void) {
    uint32_t rows = psx_mod_read_word(DESCRIPTOR);
    if (rows != ROWS && rows != window) return 0;
    if (rows == ROWS && psx_mod_read_word(DESCRIPTOR + 8) != GAME_ITEMS) return 0;
    return psx_mod_read_word(ROWS) == 0x80097f06 &&
           psx_mod_read_word(ROWS + (GAME_ROWS - 1) * ROW_SIZE) == 0x80097f04;
}

void __wrap_tekken3_game_options_tick(void) {
    if (!extras || !psx_mod_game_started()) return;
    if (psx_mod_read_word(SCREEN) != OPTIONS) { top = 0; return; }
    if (!page_ours()) return;
    for (unsigned i = 0; i < GAME_ROWS; i++) copy_row(all + i * ROW_SIZE, ROWS + i * ROW_SIZE);
    for (unsigned i = 0; i < extras; i++) copy_row(all + (GAME_ROWS + i) * ROW_SIZE, extra[i]);
    copy_row(all + (items() - 1) * ROW_SIZE, ROWS + GAME_ROWS * ROW_SIZE);
    if (psx_mod_read_word(DESCRIPTOR) == ROWS) top = 0;          /* the game set the page up again */
    if (top + shown() > items()) top = items() - shown();
    fill_window();
    if (psx_mod_read_word(DESCRIPTOR) != window) {
        psx_mod_write_word(DESCRIPTOR, window);
        psx_mod_write_word(DESCRIPTOR + 8, shown());
    }
}

/* The window that keeps item `at` on screen with one more item past it. */
static unsigned top_for(unsigned at, unsigned from) {
    unsigned k = shown(), n = items(), t = from;
    if (at < t + 1) t = at ? at - 1 : 0;
    if (at + 2 > t + k) t = at + 2 - k;
    if (t + k > n) t = n - k;
    return t;
}

void __wrap_tekken3_game_options_cursor(CPUState *cpu) {
    if (!extras || items() <= VISIBLE || cpu->gpr[5] != MOVE_SOUND || cpu->gpr[31] != MOVED ||
        cpu->gpr[19] != ENTRY || psx_mod_read_word(SCREEN) != OPTIONS ||
        psx_mod_read_word(DESCRIPTOR) != window)
        return;
    unsigned pad = psx_mod_read_half(PAD) | psx_mod_read_half(PAD + 2);
    int step = (int)(pad >> 14 & 1) - (int)(pad >> 12 & 1);
    if (!step) return;
    unsigned n = items(), from = top + psx_mod_read_word(ENTRY);
    unsigned at = (unsigned)((int)from + step + (int)n) % n;
    unsigned t = top_for(at, top);
    if (t != top) { top = t; fill_window(); }
    cpu->gpr[16] = at - top;
}

typedef struct { uint32_t at, end; unsigned words; } Prims;
static void put(Prims *p, uint32_t w) { if (p->at < p->end) { psx_mod_write_word(p->at, w); p->at += 4; p->words++; } }
static uint32_t xy(int x, int y) { return (uint32_t)(y & 0xffff) << 16 | (uint32_t)(x & 0xffff); }
static void triangle(Prims *p, uint32_t colour, int x0, int y0, int x1, int y1, int x2, int y2) {
    put(p, 0x20000000u | colour); put(p, xy(x0, y0)); put(p, xy(x1, y1)); put(p, xy(x2, y2));
}
enum { ARROW_X = 184, OUTLINE = 0x101010, ARROW = 0x38c8f8 };
static void arrow(Prims *p, int y, int dir) {
    triangle(p, OUTLINE, ARROW_X, y, ARROW_X - 9, y - 8 * dir, ARROW_X + 9, y - 8 * dir);
    triangle(p, ARROW, ARROW_X, y - 2 * dir, ARROW_X - 6, y - 7 * dir, ARROW_X + 6, y - 7 * dir);
}

int __wrap_tekken3_game_options_scrolls(void) {
    return extras && items() > VISIBLE && psx_mod_read_word(SCREEN) == OPTIONS &&
           psx_mod_read_byte(LEVEL) == 1 && psx_mod_read_word(DESCRIPTOR) == window;
}

uint32_t __wrap_tekken3_game_options_arrows(uint32_t base, uint32_t bytes, uint32_t link, unsigned frame) {
    if (!__wrap_tekken3_game_options_scrolls()) return link;
    int above = top > 0, below = top + shown() < items();
    int first = (int)psx_mod_read_word(ENTRY + 0x14), step = (int)psx_mod_read_word(ENTRY + 0x18);
    int sway = frame % 60 < 30;
    Prims p = { base + 4, base + bytes, 0 };
    if (above) arrow(&p, first - 14 - sway, -1);
    if (below) arrow(&p, first + ((int)shown() - 1) * step + 25 + sway, 1);
    psx_mod_write_word(base, (uint32_t)p.words << 24 | (link & 0xffffff));
    return base & 0xffffff;
}
#endif
