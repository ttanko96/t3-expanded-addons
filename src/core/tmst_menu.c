/* TMST core: our main-menu rows, always last (tmst_menu.h). */
#include "tmst_menu.h"
#include "tmst_core.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <string.h>

enum { SCREEN = 0x800ae204u, PHASE = 0x800ae224u, SELECTION = 0x80097f40u, MENU = 4,
       MENU_ENTRY = 0x800db5b4u, LIST_HI = 0x800db5d0u, LIST_LO = 0x800db5d8u,
       MAX_ROWS = 2, ROW_BYTES = 8, ROWS_AT = 16, MAX_COUNT = 16 };

typedef struct { uint32_t label; unsigned like_mode; TmstMenuChosen chosen; int index; } Row;
static Row rows[MAX_ROWS];
static int row_count;
static int select_row = -1;           /* highlight this row when it comes back */

int tmst_menu_add_row(const char *label, unsigned like_mode, TmstMenuChosen chosen) {
    if (row_count >= MAX_ROWS || !label) return -1;
    size_t n = strlen(label) + 1;
    uint32_t text = psx_mod_alloc_guest_memory((uint32_t)((n + 3) & ~3u), 4);
    if (!text) return -1;
    for (size_t i = 0; i < n; i++) psx_mod_write_byte(text + (uint32_t)i, (uint8_t)label[i]);
    rows[row_count] = (Row){ text, like_mode, chosen, -1 };
    return row_count++;
}

void tmst_menu_select_next_time(int row) { select_row = row; }

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

/* Our rows out of the list (they are always its last rows). */
static void take_out(uint32_t list) {
    unsigned count = count_of(list);
    if (!count || count > MAX_COUNT) return;
    while (count > 0 && ours(psx_mod_read_word(row_at(list, count - 1))) >= 0) count--;
    psx_mod_write_word(list + 4, count);
}

/* Our rows back after the last row, where the room is: never over a label
 * the list's own memory holds after the rows. */
static void put_back(uint32_t list) {
    unsigned count = count_of(list);
    if (!count || count > MAX_COUNT) return;
    uint32_t limit = list + 256;                      /* never past what the menu itself had */
    for (unsigned i = 0; i < count; i++) {
        uint32_t label = psx_mod_read_word(row_at(list, i));
        if (label > row_at(list, count) && label < limit) limit = label;
    }
    for (int r = 0; r < row_count; r++) {
        uint32_t look = 0;
        for (unsigned i = 0; i < count && !look; i++) {
            uint32_t w = psx_mod_read_word(row_at(list, i) + 4);
            if ((w & 0xffu) == rows[r].like_mode && ours(psx_mod_read_word(row_at(list, i))) < 0) look = w;
        }
        if (!look || row_at(list, count) + ROW_BYTES > limit || count >= MAX_COUNT) { rows[r].index = -1; continue; }
        psx_mod_write_word(row_at(list, count), rows[r].label);
        psx_mod_write_word(row_at(list, count) + 4, look);
        rows[r].index = (int)count++;
    }
    psx_mod_write_word(list + 4, count);
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
 * runs (it always sees its own list, its limit of eleven rows included) and
 * back after it for the native menu, which draws them, at the end, in every
 * phase it draws (1 waiting, 2 the choice, 3 the attract). A row of ours
 * carries the mode word of its native row, so when it is confirmed the game
 * dispatches that mode itself, the highlight staying on our row. */
static int told;                      /* the confirmation reported */
void __wrap_func_80028CD4(CPUState *cpu) {
    uint32_t list = row_count && psx_mod_game_started() && psx_mod_read_half(SCREEN) == MENU ? list_address() : 0;
    int was_selected = -1;
    unsigned phase = list ? psx_mod_read_half(PHASE) : 0;
    if (list) {
        unsigned sel = psx_mod_read_byte(SELECTION);
        for (int r = 0; r < row_count; r++) if (rows[r].index >= 0 && (unsigned)rows[r].index == sel) was_selected = r;
        take_out(list);
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
    put_back(list);
    if (was_selected >= 0 && rows[was_selected].index >= 0)
        psx_mod_write_byte(SELECTION, (uint8_t)rows[was_selected].index);
    else if (phase == 1 && select_row >= 0 && select_row < row_count && rows[select_row].index >= 0) {
        psx_mod_write_byte(SELECTION, (uint8_t)rows[select_row].index);
        select_row = -1;
    }
}

/* Off the menu, the list must hold the native rows only, the game keeping
 * the highlighted index across modes: our rows out (a highlighted one gives
 * the highlight to the native row of its mode), and the Expanded's QUIT GAME
 * row (mode 0xFF) out too if it is still there. The Expanded takes it out at
 * its VBlank once the menu's last frame is drawn, but not while the list is
 * longer than its eleven rows, as it is with ours when that VBlank comes
 * first; taken out here the same way, its VBlank then finds nothing to do. */
enum { HOST_QUIT_MODE = 0xff };
void tmst_menu_vblank(void) {
    if (!row_count || !psx_mod_game_started() || psx_mod_read_half(SCREEN) == MENU) return;
    uint32_t list = list_address();
    if (!list) return;
    unsigned count = count_of(list), n = count, sel = psx_mod_read_byte(SELECTION);
    if (!count || count > MAX_COUNT) return;
    int like = -1;
    for (int r; n > 0 && (r = ours(psx_mod_read_word(row_at(list, n - 1)))) >= 0; n--)
        if (sel == n - 1) like = (int)rows[r].like_mode;
    for (int r = 0; r < row_count; r++) rows[r].index = -1;
    if (n == count) return;                        /* ours were not in: nothing of ours to undo */
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
