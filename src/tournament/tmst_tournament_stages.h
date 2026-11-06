/* tmst.tournament: each tournament's stage for the fighter the player meets
 * (the CPU's: its home, as in Arcade; the user's lists, 2026-11-06).
 *
 * T1 and T2: a Tekken 2 stage (0..22, src/tekken2/tmst_tekken2_stages.c's
 * order; T1 the Tekken 2 stages until a Tekken 1 Content comes). T3: a
 * Tekken 3 arena (0..14). TTT: a Tekken Tag stage (0..14, the Expanded's TTT
 * Stages numbering). Free: none, the system's settings decide. Whether the
 * stage can be drawn (Tekken 2 Content, the TTT packs) is the game side's
 * business: this file knows only names, so it builds and is tested alone. */
#ifndef TMST_TOURNAMENT_STAGES_H
#define TMST_TOURNAMENT_STAGES_H
#ifdef __cplusplus
extern "C" {
#endif

enum { TMST_TSTAGE_NONE, TMST_TSTAGE_T2, TMST_TSTAGE_T3, TMST_TSTAGE_TTT };

/* The kind of tournament t's stage against `key` (TMST_TSTAGE_*), its number
 * in *stage (-1 with NONE). school: the fighter wears the school outfit
 * (Xiaoyu's, Jin's: the T3 and TTT school stage 7). */
int tmst_tourney_stage(int t, const char *key, int school, int *stage);

/* Tournament t's music against `key` on its stage (kind and number from
 * tmst_tourney_stage), the Console pieces (the user's choice, 2026-11-06):
 * T1 and T2 the Tekken 2 stage's piece (T1's own: a future Tekken 1
 * Content), T3 the fighter's own theme (the disc's XA), TTT the TTT stage's
 * piece. The Jukebox's file or XA name ("t2-paul.bgm", "xa-42"), its
 * category in *cat ("t2-console", "t3-console", "ttt-console"); NULL none. */
const char *tmst_tourney_music(int t, const char *key, int kind, int stage, const char **cat);

#ifdef __cplusplus
}
#endif
#endif
