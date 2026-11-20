/* tmst.tournament: the opponents' costume from the player's button
 * (tmst_tournament_costume.h). */
#include "tmst_tournament_costume.h"
#include <string.h>

enum { LP, LK, RP, RK, START };

static unsigned variant_costume(int v) { return v == START ? 2 : v == LK || v == RK ? 1 : 0; }
static unsigned key_costume(int key) { return key == TMST_KEY_CROSS || key == TMST_KEY_CIRCLE ? 1 : 0; }
static int has_v(TmstHasVariant has, void *ctx, int v) { return v >= 0 && has && has((unsigned)v, ctx); }

TmstCostume tmst_costume_native(int key, const char *who, TmstHasVariant has, void *ctx, int third_unlocked) {
    TmstCostume c = {-1, 0};
    if (key < 0 || key >= TMST_KEYS) key = TMST_KEY_SQUARE;
    if (!who) who = "";
    if (!strcmp(who, "tiger")) {
        c.variant = has_v(has, ctx, START) ? START : -1;
        c.costume = 2;
        return c;
    }
    if (!strcmp(who, "panda")) {
        int v = key == TMST_KEY_SQUARE || key == TMST_KEY_CROSS ? LK : RK;
        if (!has_v(has, ctx, v)) v = v == LK ? RK : LK;
        c.variant = has_v(has, ctx, v) ? v : -1;
        c.costume = 1;
        return c;
    }
    if (!strcmp(who, "kuma")) key = key == TMST_KEY_CROSS ? TMST_KEY_SQUARE : key == TMST_KEY_CIRCLE ? TMST_KEY_TRIANGLE : key;
    if (!strcmp(who, "eddy") && key == TMST_KEY_R1) key = TMST_KEY_TRIANGLE;
    int any = 0;
    for (int v = LP; v <= START; v++) any |= has_v(has, ctx, v);
    if (any) {
        int v = key;                          /* the keys are the variants' order */
        if (v == START && !has_v(has, ctx, START)) v = RP;
        if (has_v(has, ctx, v)) { c.variant = v; c.costume = variant_costume(v); return c; }
        if (key == TMST_KEY_R1) key = TMST_KEY_TRIANGLE;
        c.costume = key_costume(key);         /* no such model: the game's costume, the Expanded's model */
        return c;
    }
    c.costume = key == TMST_KEY_R1 ? (third_unlocked ? 2 : 0) : key_costume(key);
    return c;
}

TmstCostume tmst_costume_guest(int key, unsigned have) {
    TmstCostume c = {-1, key_costume(key)};
    if (c.costume == 1 && !(have & 2)) c.costume = 0;
    return c;
}
