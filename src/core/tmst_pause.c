/* TMST core: our row in the fight's pause menu (tmst_pause.h). */
#include "tmst_pause.h"
#include "tmst_core.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <string.h>

enum { SCREEN = 0x800ae204u, FIGHT = 8, MODE = 0x800afa88u, NO_MENU = 0x800afa9bu, PRACTICE = 5, BALL = 7,
       PAUSED = 0x80095494u, LABELS = 0x80098a44u, CURSOR = 0x80098a40u, CHOICE = 0x80098a41u,
       DELAY = 0x800a8672u, MODE_READ = 0x8007815cu, OUR_ROW = 2, OUR_CHOICE = 3, RESET_CHOICE = 3,
       RULES_CHOICE = 4 };

static uint32_t label;
static TmstPauseChosen chosen;
static int held, select_ours;

int tmst_pause_add_row(const char *text, TmstPauseChosen cb) {
    if (label || !text) return 0;
    size_t n = strlen(text) + 1;
    uint32_t at = psx_mod_alloc_guest_memory((uint32_t)((n + 3) & ~3u), 4);
    if (!at) return 0;
    for (size_t i = 0; i < n; i++) psx_mod_write_byte(at + (uint32_t)i, (uint8_t)text[i]);
    label = at; chosen = cb;
    return 1;
}
void tmst_pause_select_next_time(void) { select_ours = 1; }
void tmst_pause_hold(int on) { held = on; }

int tmst_pause_kind(void) {
    if (!psx_mod_game_started() || psx_mod_read_half(SCREEN) != FIGHT || !psx_mod_read_word(PAUSED))
        return TMST_PAUSE_NONE;
    uint32_t mode = psx_mod_read_word(MODE);
    if (mode == PRACTICE) return TMST_PAUSE_PRACTICE;
    if (mode <= 4 && !psx_mod_read_byte(NO_MENU)) return TMST_PAUSE_MENU;
    return TMST_PAUSE_OTHER;
}
int tmst_pause_player(void) {
    uint32_t p = tmst_pause_kind() != TMST_PAUSE_NONE ? psx_mod_read_word(PAUSED) : 0;
    return p == 1 || p == 2 ? (int)p - 1 : -1;
}

extern void __tmst_chain_func_800780BC(CPUState *cpu);
#ifdef TMST_BRIDGE_func_800780BC
extern void __real_func_800780BC(CPUState *cpu);
void __tmst_chain_func_800780BC(CPUState *cpu) { __real_func_800780BC(cpu); }
#endif

/* Each entry of the routine: its first (pc 0) or a return of a call it made
 * (pc = where it goes on). */
void __wrap_func_800780BC(CPUState *cpu) {
    if (held && cpu->pc == 0) {                     /* not run this frame: back to its caller */
        cpu->pc = cpu->gpr[31];
        return;
    }
    if (!label || tmst_pause_kind() != TMST_PAUSE_MENU) { __tmst_chain_func_800780BC(cpu); return; }
    uint32_t rows[4];
    for (int i = 0; i < 4; i++) rows[i] = psx_mod_read_word(LABELS + 4u * i);
    psx_mod_write_word(LABELS + 8, label);
    psx_mod_write_word(LABELS + 12, rows[2]);       /* RESET one lower */
    if (select_ours && cpu->pc == 0) { psx_mod_write_byte(CURSOR, OUR_ROW); select_ours = 0; }
    int count_read = cpu->pc == MODE_READ;
    uint32_t mode = count_read ? psx_mod_read_word(MODE) : 0;
    if (count_read) psx_mod_write_word(MODE, BALL);  /* four rows */
    unsigned before = psx_mod_read_byte(CHOICE);
    __tmst_chain_func_800780BC(cpu);
    if (count_read) psx_mod_write_word(MODE, mode);
    for (int i = 0; i < 4; i++) psx_mod_write_word(LABELS + 4u * i, rows[i]);
    unsigned choice = psx_mod_read_byte(CHOICE);
    if (choice == RULES_CHOICE) {
        psx_mod_write_byte(CHOICE, RESET_CHOICE);
    } else if (choice == OUR_CHOICE && before != OUR_CHOICE && psx_mod_read_byte(CURSOR) == OUR_ROW) {
        psx_mod_write_byte(CHOICE, 0);              /* the game does nothing: the menu stays */
        psx_mod_write_byte(DELAY, 0);
        tmst_log("pause: our row chosen");
        if (chosen) chosen();
    }
}
