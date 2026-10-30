/* TMST core: a row of our own in the fight's pause menu (the tmst_menu of
 * the pause), and the pause itself held for a mod's own screen.
 *
 * The pause menu of a fight (Arcade, VS, Team Battle, Time Attack,
 * Survival, Tekken Force; START in the fight) is func_800780BC: CANCEL,
 * COMMAND, RESET from a table of labels at 0x80098A44 (CANCEL, COMMAND,
 * RESET, RULES), three rows, four in Tekken Ball, where the fourth is RULES:
 * the routine reads the mode (0x800AFA88) once, at 0x80078164, for the
 * count, the box's height and the rows' place. Our row goes above RESET with
 * no code changed: while the routine runs, the table says CANCEL, COMMAND,
 * ours, RESET, and for the one stretch of it that reads the mode (entered
 * at 0x8007815C, the return of the call before the read) the mode says
 * Tekken Ball; both are given back each time the routine hands over (it is
 * entered again at every return of a call it makes), so nothing else in the
 * game, nor its sounds and text routines (checked), ever sees them. The
 * choice the routine makes (0x80098A41, the cursor + 1) is set right after:
 * our row (3, cursor 2) is taken back and given to the mod; RESET, one row
 * lower, gives 4, which becomes RESET's 3 before anything reads it (4 would
 * be Tekken Ball's RULES).
 *
 * Practice (mode 5) has its own panels, drawn by its overlay from the disc,
 * and Tekken Ball (mode 7) has RULES on that fourth row: no row there; a mod
 * offers its screen there another way (tmst_pause_kind). Tekken Force (mode
 * 8) has the same menu but no row either: its stages are corridors built for
 * its side-on camera, nothing a free look can show (the player's choice).
 *
 * Held (tmst_pause_hold), the routine is not run: the menu is not drawn and
 * takes no buttons; the fight stays paused, as the game left it. */
#ifndef TMST_PAUSE_H
#define TMST_PAUSE_H
#ifdef __cplusplus
extern "C" {
#endif

/* Called once, the frame the row is confirmed. */
typedef void (*TmstPauseChosen)(void);

/* The row labelled `label` (upper case, the menu's font; up to 8 letters
 * fit its box). One row. Returns 0 when it cannot be added. */
int tmst_pause_add_row(const char *label, TmstPauseChosen chosen);
/* The menu comes back with our row highlighted. */
void tmst_pause_select_next_time(void);
/* Nonzero: the pause menu is not run (not drawn, no buttons). */
void tmst_pause_hold(int on);

enum {
    TMST_PAUSE_NONE,        /* not a fight, or the fight is not paused */
    TMST_PAUSE_MENU,        /* the pause menu with our row */
    TMST_PAUSE_PRACTICE,    /* Practice's own panels */
    TMST_PAUSE_OTHER        /* paused, no place for us (Tekken Ball, Tekken Force) */
};
int tmst_pause_kind(void);
/* The player whose pause it is (0 or 1), -1 when not paused. */
int tmst_pause_player(void);

#ifdef __cplusplus
}
#endif
#endif
