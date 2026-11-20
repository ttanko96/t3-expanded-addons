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

enum { TMST_TSTAGE_NONE, TMST_TSTAGE_T2, TMST_TSTAGE_T3, TMST_TSTAGE_TTT, TMST_TSTAGE_T1 };

/* The kind of tournament t's stage against `key` (TMST_TSTAGE_*), its number
 * in *stage (-1 with NONE). school: the fighter wears the school outfit
 * (Xiaoyu's, Jin's: the T3 and TTT school stage 7). */
int tmst_tourney_stage(int t, const char *key, int school, int *stage);

/* Tournament t's music against `key` on its stage (kind and number from
 * tmst_tourney_stage), the Console pieces (the user's choice, 2026-11-06):
 * T1 and T2 the Tekken 2 stage's piece (T1's own: a future Tekken 1
 * Content), T3 the fighter's own theme (the disc's XA), TTT the TTT stage's
 * piece. The Jukebox's file or XA name ("t2-paul.bgm", "xa-42"), its
 * category in *cat ("t2-console", "t3-console", "ttt-console"); NULL none.
 * arcade (GAME OPTION's BGM SELECT, issue #9): T1, T2 and TTT the Arcade
 * piece ("t2-arcade", "ttt-arcade"); T3 still the Console's XA, which the
 * game side turns into its Arcade piece by the game's own music table. */
const char *tmst_tourney_music(int t, const char *key, int kind, int stage, int arcade, const char **cat);
/* A Tekken 2 stage's piece (0..22: its owner's theme), Console or Arcade;
 * the tournaments' and the STAGE line's (src/tekken2/tmst_tekken2_stages.c). */
const char *tmst_t2_stage_music(int stage, int arcade);
/* A Tekken 1 stage's piece (0..10, the stage files' order: Windermere ..
 * Chicago), Console or Arcade: the Jukebox's TEKKEN 1 lists' (t1-console /
 * t1-arcade; the Tekken 2 disc's Tekken 1 themes and Venezia, the Tekken 1
 * disc's); the STAGE line's and the STAGES row's (src/tekken2/tmst_tekken2_stages.c). */
const char *tmst_t1_stage_music(int stage, int arcade);

/* GAME OPTION's STAGES = TEKKEN 2 (issue #11, outside the tournaments): the
 * stage of the fighter who decides the fight's, its Tekken 2 stage
 * (TMST_TSTAGE_T2: Tekken 2's own, or a Tekken 3 newcomer's of the user's
 * list; NONE: not on the list, the system decides), its number in *stage.
 * school: Xiaoyu or Jin in the school outfit, Tekken 3's arena 7 (_T3). */
int tmst_option_t2_stage(const char *key, int school, int *stage);
/* STAGES = RANDOM's Tekken 2 lot: 1 and the fighter's Tekken 2 stage (the
 * TEKKEN 2 value's). None for Xiaoyu and Jin in the school outfit. */
int tmst_random_t2_stage(const char *key, int school, int *stage);
/* STAGES = TEKKEN 1 (the user's list, 2026-11-19): the fighter's Tekken 1
 * stage (TMST_TSTAGE_T1, 0..10; NONE: not on the list), the school outfit
 * Tekken 3's arena 7 (_T3). */
int tmst_option_t1_stage(const char *key, int school, int *stage);
/* STAGES = RANDOM's Tekken 1 lot: 1 and the fighter's Tekken 1 stage. With
 * both lots the pool is T3, TTT, T2 and T1, one chance in four each. None for
 * the school outfit. */
int tmst_random_t1_stage(const char *key, int school, int *stage);

#ifdef __cplusplus
}
#endif
#endif
