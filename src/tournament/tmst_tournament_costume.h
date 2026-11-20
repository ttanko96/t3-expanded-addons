/* tmst.tournament: the opponents wear the player's costume button (the
 * user's request, 2026-11-20: as Tekken 3's Arcade gave every opponent the
 * player's punch or kick costume, with TTT1's buttons too).
 *
 * A native drawn by its TTT1 models has one a button (the Expanded's
 * variants, src/tekken3_native_moves.c key_variant: 0 square LP, 1 cross
 * LK, 2 triangle RP, 3 circle RK, 4 Start, R1's), the game's costume the
 * one the model stands for (LP, RP 0; LK, RK 1; Start 2). R1 without a
 * Start model is triangle. Fighters of their own in a tournament: Kuma
 * keeps to his own costume (his cross and circle models are Panda: square,
 * triangle instead), Panda wears his (cross for square and cross, circle
 * for the rest), Eddy's R1 is triangle (his Start is Tiger), Tiger is his
 * Start. A native without TTT1 models: the game's costume of the button
 * (R1 its third, unlocked, else triangle's). A guest: its first costume
 * for square, triangle and R1, its second for cross and circle (when it
 * has one). Built and tested alone. */
#ifndef TMST_TOURNAMENT_COSTUME_H
#define TMST_TOURNAMENT_COSTUME_H
#ifdef __cplusplus
extern "C" {
#endif

enum { TMST_KEY_SQUARE, TMST_KEY_CROSS, TMST_KEY_TRIANGLE, TMST_KEY_CIRCLE, TMST_KEY_R1, TMST_KEYS };
/* variant: the TTT1 model (0..4), -1 none (the Expanded's own choice);
 * costume: the game's (0 punch, 1 kick, 2 third). */
typedef struct { int variant; unsigned costume; } TmstCostume;
/* Whether the native has its TTT1 model `variant` (its file imported). */
typedef int (*TmstHasVariant)(unsigned variant, void *ctx);

/* The native `who` (a tournament key: "kuma", "panda", "eddy", "tiger",
 * ...) for the player's key; third_unlocked: its third costume is. */
TmstCostume tmst_costume_native(int key, const char *who, TmstHasVariant has, void *ctx, int third_unlocked);
/* A guest for the player's key; have: its costumes' files (bit 0 the
 * first, bit 1 the second, bit 2 the third). */
TmstCostume tmst_costume_guest(int key, unsigned have);

#ifdef __cplusplus
}
#endif
#endif
