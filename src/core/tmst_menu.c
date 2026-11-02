/* TMST core: our main-menu rows (tmst_menu.h). */
#include "tmst_menu.h"
#include "tmst_core.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <string.h>

enum { SCREEN = 0x800ae204u, PHASE = 0x800ae224u, SELECTION = 0x80097f40u, MENU = 4,
       MENU_ENTRY = 0x800db5b4u, LIST_HI = 0x800db5d0u, LIST_LO = 0x800db5d8u,
       MAX_ROWS = 4, ROW_BYTES = 8, ROWS_AT = 16, MAX_COUNT = 16, AT_END = 0x100 };

typedef struct { uint32_t label; unsigned like_mode, after_mode; TmstMenuChosen chosen; int index; } Row;
static Row rows[MAX_ROWS];
static int row_count;
static int select_row = -1;           /* highlight this row when it comes back */

static int add_row(const char *label, unsigned like_mode, unsigned after_mode, TmstMenuChosen chosen) {
    if (row_count >= MAX_ROWS || !label) return -1;
    size_t n = strlen(label) + 1;
    uint32_t text = psx_mod_alloc_guest_memory((uint32_t)((n + 3) & ~3u), 4);
    if (!text) return -1;
    for (size_t i = 0; i < n; i++) psx_mod_write_byte(text + (uint32_t)i, (uint8_t)label[i]);
    rows[row_count] = (Row){ text, like_mode, after_mode, chosen, -1 };
    return row_count++;
}
int tmst_menu_add_row(const char *label, unsigned like_mode, TmstMenuChosen chosen) {
    return add_row(label, like_mode, AT_END, chosen);
}
int tmst_menu_add_row_after(const char *label, unsigned like_mode, unsigned after_mode, TmstMenuChosen chosen) {
    return add_row(label, like_mode, after_mode & 0xffu, chosen);
}

void tmst_menu_select_next_time(int row) { select_row = row; }
int tmst_menu_highlighted(int row) {
    return row >= 0 && row < row_count && rows[row].index >= 0 && psx_mod_read_half(SCREEN) == MENU &&
           psx_mod_read_half(PHASE) == 1 && psx_mod_read_byte(SELECTION) == (unsigned)rows[row].index;
}

/* The working list the native menu reads: from the menu's own LUI and
 * ORI/ADDIU (as the game has them, or as the Expanded patched them). 0 when
 * the menu code is not the one we know. */
static uint32_t list_address(void) {
    if (psx_mod_read_word(MENU_ENTRY) != 0x27bdffa0u) return 0;
    uint32_t hi = psx_mod_read_word(LIST_HI), lo = psx_mod_read_word(LIST_LO);
    if ((hi & 0xffff0000u) != 0x3c020000u) return 0;
    uint32_t base = (hi & 0xffffu) << 16;
    if ((lo & 0xffff0000u) == 0x34560000u) return base | (lo & 0xffffu);                  /* ori s6, v0 */
    if ((lo & 0xffff0000u) == 0x24560000u) return base + (uint32_t)(int32_t)(int16_t)(lo & 0xffffu); /* addiu */
    return 0;
}

static uint32_t row_at(uint32_t list, unsigned i) { return list + ROWS_AT + i * ROW_BYTES; }
static unsigned count_of(uint32_t list) { return psx_mod_read_word(list + 4); }
static int ours(uint32_t label) {
    for (int r = 0; r < row_count; r++) if (rows[r].label == label) return r;
    return -1;
}

/* Our rows out of the list, wherever they are. Returns the highlight `sel`
 * counted in the list left (on a row of ours: the row after it). */
static unsigned take_out(uint32_t list, unsigned sel) {
    unsigned count = count_of(list), n = 0, out = sel;
    if (!count || count > MAX_COUNT) return sel;
    for (unsigned i = 0; i < count; i++) {
        uint32_t label = psx_mod_read_word(row_at(list, i)), word = psx_mod_read_word(row_at(list, i) + 4);
        if (ours(label) >= 0) { if (i < sel) out--; continue; }
        if (n != i) { psx_mod_write_word(row_at(list, n), label); psx_mod_write_word(row_at(list, n) + 4, word); }
        n++;
    }
    psx_mod_write_word(list + 4, n);
    return n && out >= n ? n - 1 : out;
}

/* Our rows back, each after its native row (or after the last row), where
 * the room is: never over a label the list's own memory holds after the
 * rows. Returns the highlight `sel` (counted without ours) counted with them. */
static unsigned put_back(uint32_t list, unsigned sel) {
    unsigned count = count_of(list);
    if (!count || count > MAX_COUNT) return sel;
    uint32_t limit = list + 256;                      /* never past what the menu itself had */
    uint32_t label[MAX_COUNT], word[MAX_COUNT], look[MAX_ROWS] = {0};
    for (unsigned i = 0; i < count; i++) {
        label[i] = psx_mod_read_word(row_at(list, i)); word[i] = psx_mod_read_word(row_at(list, i) + 4);
        if (label[i] > row_at(list, count) && label[i] < limit) limit = label[i];
    }
    for (int r = 0; r < row_count; r++) {
        rows[r].index = -1;
        for (unsigned i = 0; i < count && !look[r]; i++) if ((word[i] & 0xffu) == rows[r].like_mode) look[r] = word[i];
    }
    uint32_t out_label[MAX_COUNT], out_word[MAX_COUNT];
    unsigned n = 0, out_sel = sel, room = (limit - row_at(list, 0)) / ROW_BYTES;
    int placed[MAX_ROWS] = {0};
    room = room > MAX_COUNT ? MAX_COUNT : room;
    for (unsigned i = 0; i <= count; i++) {
        if (i < count) {
            if (i == sel) out_sel = n;
            out_label[n] = label[i]; out_word[n++] = word[i];
        }
        for (int r = 0; r < row_count; r++) {
            if (placed[r] || !look[r]) continue;
            if (i < count ? rows[r].after_mode != (word[i] & 0xffu) : 0) continue;
            placed[r] = 1;
            if (n + (count - (i < count ? i + 1 : count)) >= room) continue;   /* the native rows still to come keep theirs */
            rows[r].index = (int)n;
            out_label[n] = rows[r].label; out_word[n++] = look[r];
        }
    }
    for (unsigned i = 0; i < n; i++) {
        psx_mod_write_word(row_at(list, i), out_label[i]); psx_mod_write_word(row_at(list, i) + 4, out_word[i]);
    }
    psx_mod_write_word(list + 4, n);
    return out_sel;
}

extern void __tmst_chain_func_80028CD4(CPUState *cpu);
#ifdef TMST_BRIDGE_func_80028CD4
extern void __real_func_80028CD4(CPUState *cpu);
void __tmst_chain_func_80028CD4(CPUState *cpu) { __real_func_80028CD4(cpu); }
#endif

/* The native row whose mode a row of ours dispatches (not one of ours). */
static int like_index(uint32_t list, unsigned like_mode) {
    unsigned count = count_of(list);
    for (unsigned i = 0; i < count && i < MAX_COUNT; i++)
        if ((psx_mod_read_word(row_at(list, i) + 4) & 0xffu) == like_mode && ours(psx_mod_read_word(row_at(list, i))) < 0)
            return (int)i;
    return -1;
}

/* The screen-dispatch arm that hands off to the menu overlay (the Expanded's
 * hook point too). Our rows are out of the list while the Expanded's code
 * runs (it always sees its own list, its limit of eleven rows included, and
 * the highlight counted in it) and back after it for the native menu, which
 * draws them in every phase it draws (1 waiting, 2 the choice, 3 the
 * attract). A row of ours carries the mode word of its native row, so when
 * it is confirmed the game dispatches that mode itself, the highlight
 * staying on our row. */
static int told;                      /* the confirmation reported */
void __wrap_func_80028CD4(CPUState *cpu) {
    uint32_t list = row_count && psx_mod_game_started() && psx_mod_read_half(SCREEN) == MENU ? list_address() : 0;
    int was_selected = -1;
    unsigned phase = list ? psx_mod_read_half(PHASE) : 0;
    if (list) {
        unsigned sel = psx_mod_read_byte(SELECTION);
        if (sel < count_of(list) && sel < MAX_COUNT) was_selected = ours(psx_mod_read_word(row_at(list, sel)));
        psx_mod_write_byte(SELECTION, (uint8_t)take_out(list, sel));
        for (int r = 0; r < row_count; r++) rows[r].index = -1;
        if (was_selected >= 0 && phase == 2 && !told) {
            told = 1;
            tmst_log("menu: row %d chosen, the game starts its mode", was_selected);
            if (rows[was_selected].chosen) rows[was_selected].chosen();
        }
        if (phase != 2) told = 0;
    }
    __tmst_chain_func_80028CD4(cpu);
    /* Only when handing off to the menu (not when Quit consumed the call). */
    if (!list || phase < 1 || phase > 3 || cpu->pc != MENU_ENTRY || cpu->gpr[31] != 0x80028cdcu) return;
    unsigned sel = put_back(list, psx_mod_read_byte(SELECTION));
    if (was_selected >= 0 && rows[was_selected].index >= 0) sel = (unsigned)rows[was_selected].index;
    else if (phase == 1 && select_row >= 0 && select_row < row_count && rows[select_row].index >= 0) {
        sel = (unsigned)rows[select_row].index;
        select_row = -1;
    }
    psx_mod_write_byte(SELECTION, (uint8_t)sel);
}

/* Off the menu, the list must hold the native rows only, the game keeping
 * the highlighted index across modes: our rows out (a highlighted one gives
 * the highlight to the native row of its mode), and the Expanded's QUIT GAME
 * row (mode 0xFF) out too if it is still there. The Expanded takes it out at
 * its VBlank once the menu's last frame is drawn, but not while the list is
 * longer than its eleven rows, as it is with ours when that VBlank comes
 * first; taken out here the same way, its VBlank then finds nothing to do. */
int tmst_menu_distance(int row) {
    if (row < 0 || row >= row_count || rows[row].index < 0 || psx_mod_read_half(SCREEN) != MENU) return -1;
    uint32_t list = list_address();
    unsigned count = list ? count_of(list) : 0, sel = psx_mod_read_byte(SELECTION);
    if (!count || count > MAX_COUNT) return -1;
    int d = rows[row].index - (int)sel;
    if (d < 0) d = -d;
    return d * 2 > (int)count ? (int)count - d : d;
}

enum { HOST_QUIT_MODE = 0xff };
void tmst_menu_vblank(void) {
    if (!row_count || !psx_mod_game_started() || psx_mod_read_half(SCREEN) == MENU) return;
    uint32_t list = list_address();
    if (!list) return;
    unsigned count = count_of(list), sel = psx_mod_read_byte(SELECTION);
    if (!count || count > MAX_COUNT) return;
    int like = -1, any = 0;
    for (unsigned i = 0; i < count; i++) {
        int r = ours(psx_mod_read_word(row_at(list, i)));
        if (r < 0) continue;
        any = 1;
        if (sel == i) like = (int)rows[r].like_mode;
    }
    for (int r = 0; r < row_count; r++) rows[r].index = -1;
    if (!any) return;                              /* ours were not in: nothing of ours to undo */
    sel = take_out(list, sel);
    unsigned n = count_of(list);
    for (unsigned i = 0; i < n; i++) {
        if ((psx_mod_read_word(row_at(list, i) + 4) & 0xffu) != HOST_QUIT_MODE) continue;
        for (unsigned j = i; j + 1 < n; j++) {
            psx_mod_write_word(row_at(list, j), psx_mod_read_word(row_at(list, j + 1)));
            psx_mod_write_word(row_at(list, j) + 4, psx_mod_read_word(row_at(list, j + 1) + 4));
        }
        n--;
        if (like < 0 && sel >= i) sel = sel == i ? 0 : sel - 1;
        break;
    }
    psx_mod_write_word(list + 4, n);
    if (like >= 0) { int to = like_index(list, (unsigned)like); sel = to >= 0 ? (unsigned)to : 0; }
    psx_mod_write_byte(SELECTION, (uint8_t)sel);
}
