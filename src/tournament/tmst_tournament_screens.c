/* tmst.tournament: the screens (tmst_tournament_screens.h). */
#include "tmst_tournament_screens.h"
#include "tmst_tournament_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GOLD = 0xe8b830, WHITE = 0xe8e4dc, RED = 0xd03030, DIM = 0x8a8880 };
enum { LINE = 0xff4a4a50u, LINE_WON = 0xffe8b830u, LINE_OTHER = 0xffa89860u, LINE_OUT = 0xffd03030u,
       STRIP = 0xb0000000u };
static const char *const round_names[TMST_BRACKET_ROUNDS] = {"FIRST ROUND", "QUARTER FINAL", "SEMI FINAL", "FINAL",
                                                             "GRAND FINAL"};

/* ---- the pyramid ----
 * Level 0: the 16 slots; level L (1..4): the winners of the matches of round
 * L - 1, in bracket order (match j of round 0, 8 + j, 12 + j, 14). The boss
 * above level 4. */
static const int level_count[6] = {16, 8, 4, 2, 1, 1};
static const int level_y[6] = {382, 324, 264, 200, 134, 66}, level_w[6] = {36, 38, 40, 44, 46, 48},
                 level_h[6] = {38, 40, 42, 46, 48, 50};
static int box_cx(int level, int j) {
    if (level == 0) return ART_SAFE_X + 2 + j * 40 + 18;
    if (level == 5) return ART_W / 2;
    return (box_cx(level - 1, 2 * j) + box_cx(level - 1, 2 * j + 1)) / 2;
}
static void box(int level, int j, int *x, int *y, int *w, int *h) {
    *w = level_w[level]; *h = level_h[level]; *x = box_cx(level, j) - *w / 2; *y = level_y[level];
}
static const int level_first_match[5] = {-1, 0, 8, 12, 14};
/* The match whose winner sits at (level, j); -1 for the slots. */
static int node_match(int level, int j) { return level == 0 || level > 4 ? -1 : level_first_match[level] + j; }
/* The match a node's fighter plays next. */
static int node_feeds(int level, int j) {
    return level == 0 ? j / 2 : level == 1 ? 8 + j / 2 : level == 2 ? 12 + j / 2 : level == 3 ? 14 : 15;
}
static int match_level(int m) { return m < 8 ? 1 : m < 12 ? 2 : m < 14 ? 3 : m < 15 ? 4 : 5; }
static int match_index(int m) { return m < 8 ? m : m < 12 ? m - 8 : m < 14 ? m - 12 : 0; }

static int node_fighter(const ScreenProgress *s, int level, int j) {
    if (level == 0) return s->bracket->slot[j];
    if (level == 5) return s->bracket->boss;
    int m = node_match(level, j);
    return (s->shown >> m & 1) && !((s->moving >> m & 1) && s->moving_t < 256) ? s->bracket->winner[m] : TMST_NOBODY;
}

static void face(const ScreenProgress *s, int fighter, int x, int y, int w, int h, int grey) {
    art_fill(x - 2, y - 2, w + 4, h + 4, 0xff101014u);
    if (fighter == TMST_NOBODY) { art_fill(x, y, w, h, 0xff1c1c22u); return; }
    const ArtImage *im = art_icon(s->roster->key(fighter));
    if (im) art_blit(im, x, y, w, h, grey ? ART_GREY : ART_PLAIN, 255);
    else {
        art_fill(x, y, w, h, grey ? 0xff202024u : 0xff3a3020u);
        char two[3] = {s->roster->name(fighter)[0], s->roster->name(fighter)[1], 0};
        art_text(two, x + w / 2, y + h / 2 - 7, 1, 60, grey ? DIM : WHITE);
    }
}
static void hline(int x0, int x1, int y, int t, uint32_t c) { if (x1 < x0) { int k = x0; x0 = x1; x1 = k; } art_fill(x0, y - t / 2, x1 - x0 + 1, t, c); }
static void vline(int x, int y0, int y1, int t, uint32_t c) { if (y1 < y0) { int k = y0; y0 = y1; y1 = k; } art_fill(x - t / 2, y0, t, y1 - y0 + 1, c); }

/* A match's lines: from each child up to the joint, the joint, up to the
 * parent; the winner's way coloured once it is shown. */
static void match_lines(const ScreenProgress *s, int m) {
    int L = match_level(m), j = match_index(m), px, py, pw, ph, ax, ay, aw, ah, bx, by, bw, bh;
    if (L == 5) {                                            /* the Grand Final: the finalist and the boss */
        box(4, 0, &ax, &ay, &aw, &ah); box(5, 0, &px, &py, &pw, &ph);
        uint32_t c = (s->shown >> m & 1) ? LINE_WON : LINE;
        vline(ART_W / 2, py + ph, ay, 3, c);
        return;
    }
    box(L, j, &px, &py, &pw, &ph); box(L - 1, 2 * j, &ax, &ay, &aw, &ah); box(L - 1, 2 * j + 1, &bx, &by, &bw, &bh);
    int joint = py + ph + (ay - (py + ph)) / 2, acx = ax + aw / 2, bcx = bx + bw / 2, pcx = px + pw / 2;
    vline(acx, ay, joint, 2, LINE); vline(bcx, by, joint, 2, LINE); hline(acx, bcx, joint, 2, LINE); vline(pcx, joint, py + ph, 2, LINE);
    if (!(s->shown >> m & 1)) return;
    int w = s->bracket->winner[m], left = node_fighter(s, L - 1, 2 * j) == w;
    int lost = w != s->bracket->player && (node_fighter(s, L - 1, 2 * j) == s->bracket->player ||
                                           node_fighter(s, L - 1, 2 * j + 1) == s->bracket->player);
    uint32_t c = w == s->bracket->player ? LINE_WON : LINE_OTHER;
    int t = (s->moving >> m & 1) ? s->moving_t : 256, wx = left ? acx : bcx, wy = left ? ay : by;
    if (s->over && lost) {                                   /* the player's line, broken in red */
        int px0 = left ? bcx : acx;
        vline(px0, left ? by : ay, joint + (left ? by - joint : ay - joint) / 3, 4, LINE_OUT);
    }
    /* drawn as far as the winner has come */
    int seg1 = wy - joint, seg2 = abs(wx - pcx), seg3 = joint - (py + ph), len = seg1 + seg2 + seg3, done = len * t / 256;
    vline(wx, wy, wy - (done < seg1 ? done : seg1), 3, c);
    if (done > seg1) hline(wx, wx + (pcx > wx ? 1 : -1) * (done - seg1 < seg2 ? done - seg1 : seg2), joint, 3, c);
    if (done > seg1 + seg2) vline(pcx, joint, joint - (done - seg1 - seg2), 3, c);
}

static void pyramid(const ScreenProgress *s) {
    for (int m = 0; m < TMST_BRACKET_MATCHES; m++) match_lines(s, m);
    for (int L = 0; L <= 5; L++)
        for (int j = 0; j < level_count[L]; j++) {
            int x, y, w, h, f = node_fighter(s, L, j);
            box(L, j, &x, &y, &w, &h);
            if (L == 0 && j >= s->drawn) {                       /* still being drawn: a face turning */
                int k = (s->shuffle / 3 + j * 7) % TMST_BRACKET_SLOTS;
                face(s, s->bracket->slot[k], x, y, w, h, 1);
                continue;
            }
            int fed = node_feeds(L, j), out = f != TMST_NOBODY && (s->shown >> fed & 1) &&
                                            s->bracket->winner[fed] != f && !((s->moving >> fed & 1) && s->moving_t < 256);
            face(s, f, x, y, w, h, out);
            if (f == s->bracket->player) art_frame(x - 3, y - 3, w + 6, h + 6, 2, out ? 0xffd03030u : 0xffe8b830u);
            if (L == 5) art_frame(x - 3, y - 3, w + 6, h + 6, 2, 0xffd03030u);
        }
    for (int m = 0; m < TMST_BRACKET_MATCHES; m++) {         /* the winners on their way up */
        if (!(s->moving >> m & 1) || s->moving_t >= 256 || !(s->shown >> m & 1)) continue;
        int L = match_level(m), j = match_index(m), ax, ay, aw, ah, px, py, pw, ph;
        if (L == 5) { box(4, 0, &ax, &ay, &aw, &ah); box(5, 0, &px, &py, &pw, &ph); }
        else {
            int left = node_fighter(s, L - 1, 2 * j) == s->bracket->winner[m];
            box(L - 1, 2 * j + !left, &ax, &ay, &aw, &ah); box(L, j, &px, &py, &pw, &ph);
        }
        int t = s->moving_t;
        face(s, s->bracket->winner[m], ax + (px - ax) * t / 256, ay + (py - ay) * t / 256, aw + (pw - aw) * t / 256,
             ah + (ph - ah) * t / 256, 0);
    }
    if (s->next >= 0 && s->blink) {                          /* the next match: both fighters framed */
        int m = s->next, L = match_level(m), j = match_index(m), x, y, w, h;
        for (int k = 0; k < 2; k++) {
            if (L == 5) { if (k) box(5, 0, &x, &y, &w, &h); else box(4, 0, &x, &y, &w, &h); }
            else box(L - 1, 2 * j + k, &x, &y, &w, &h);
            art_frame(x - 5, y - 5, w + 10, h + 10, 2, 0xffffffffu);
        }
    }
    art_text("FINAL BOSS", ART_W / 2 + level_w[5] / 2 + 10, level_y[5] + 18, 0, 50, RED);
}

void screen_match_point(int match, int *x, int *y) {
    int L = match_level(match), j = match_index(match), ax, ay, aw, ah, bx, by, bw, bh;
    if (L == 5) { box(4, 0, &ax, &ay, &aw, &ah); box(5, 0, &bx, &by, &bw, &bh); }
    else { box(L - 1, 2 * j, &ax, &ay, &aw, &ah); box(L - 1, 2 * j + 1, &bx, &by, &bw, &bh); }
    *x = (ax + aw / 2 + bx + bw / 2) / 2; *y = (ay + ah / 2 + by + bh / 2) / 2;
}

void screen_progress(const ScreenProgress *s) {
    art_background("progress");
    art_text(s->tourney, ART_W / 2, 18, 1, 80, 0);
    int r = s->next >= 0 ? tmst_bracket_round(s->next) : -1;
    if (r >= 0) art_text(round_names[r], ART_W / 2, 42, 1, 55, WHITE);
    pyramid(s);
    art_fill(0, 424, ART_W, 56, STRIP);
    if (s->next >= 0) {
        int a, b; tmst_bracket_fighters(s->bracket, s->next, &a, &b);
        if (s->next == TMST_GRAND_FINAL) b = s->bracket->boss;
        char line[96];
        snprintf(line, sizeof line, "%s  VS  %s", a >= 0 ? s->roster->name(a) : "", b >= 0 ? s->roster->name(b) : "");
        art_text(line, ART_W / 2, 428, 1, 70, WHITE);
    }
    if (s->prompt) art_text(s->prompt, ART_SAFE_X + ART_SAFE_W - 4, 456, 2, 50, DIM);
}

/* A face as big as the screen asks: the VS portrait, or the icon stretched. */
static void big_face(const ScreenProgress *s, int fighter, int x, int y, int w, int h, int mode) {
    if (fighter == TMST_NOBODY) return;
    const char *key = s->roster->key(fighter);
    const ArtImage *im = art_portrait(key);
    if (!im) im = art_icon(key);
    if (im) art_blit(im, x, y, w, h, mode, 255);
    else art_fill(x, y, w, h, 0xff3a3020u);
}
void screen_match(const ScreenProgress *s, int match) {
    art_background("match");
    int r = tmst_bracket_round(match), a, b;
    tmst_bracket_fighters(s->bracket, match, &a, &b);
    if (match == TMST_GRAND_FINAL) { a = s->bracket->player; b = s->bracket->boss; }
    if (b == s->bracket->player) { int k = a; a = b; b = k; }
    art_text(s->tourney, ART_W / 2, 20, 1, 70, 0);
    art_text(round_names[r], ART_W / 2, 44, 1, 100, WHITE);
    big_face(s, a, 150, 96, 220, 252, ART_PLAIN);
    big_face(s, b, ART_W - 150 - 220, 96, 220, 252, ART_MIRROR);
    art_frame(147, 93, 226, 258, 3, 0xffe8b830u);
    art_frame(ART_W - 150 - 223, 93, 226, 258, 3, 0xffd03030u);
    art_text("VS", ART_W / 2, 196, 1, 200, RED);
    art_text(a >= 0 ? s->roster->name(a) : "", 260, 360, 1, 80, WHITE);
    art_text(b >= 0 ? s->roster->name(b) : "", ART_W - 260, 360, 1, 80, WHITE);
    if (s->prompt) art_text(s->prompt, ART_W / 2, 440, 1, 60, DIM);
}

void screen_champion(const ScreenProgress *s, const char *unlocked) {
    art_background("champion");
    art_text("CHAMPION", ART_W / 2, 20, 1, 200, 0);
    art_text(s->tourney, ART_W / 2, 74, 1, 70, WHITE);
    big_face(s, s->bracket->player, 150, 106, 220, 252, ART_PLAIN);
    art_frame(146, 102, 228, 260, 4, 0xffe8b830u);
    art_text(s->roster->name(s->bracket->player), 260, 368, 1, 80, GOLD);
    int bx = 520, by = 150, bw = 176, bh = 202;
    big_face(s, s->bracket->boss, bx, by, bw, bh, ART_GREY | ART_MIRROR);
    for (int i = -3; i <= 3; i++) {                           /* crossed out */
        for (int k = 0; k < bh; k++) {
            int x1 = bx + k * bw / bh, x2 = bx + bw - k * bw / bh;
            art_fill(x1 + i, by + k, 1, 1, 0xffd03030u); art_fill(x2 + i, by + k, 1, 1, 0xffd03030u);
        }
    }
    art_text("DEFEATED", bx + bw / 2, by + bh + 8, 1, 60, RED);
    art_text(s->roster->name(s->bracket->boss), bx + bw / 2, by - 24, 1, 60, DIM);
    art_fill(0, 404, ART_W, 76, STRIP);
    if (unlocked) {
        char line[96]; snprintf(line, sizeof line, "%s  UNLOCKED!", unlocked);
        art_text(line, ART_W / 2, 412, 1, 70, GOLD);
    }
    if (s->prompt) art_text(s->prompt, ART_W / 2, 448, 1, 60, DIM);
}

void screen_over(const ScreenProgress *s) {
    art_background("over");
    art_text("TOURNAMENT OVER", ART_W / 2, 18, 1, 80, RED);
    art_text(s->tourney, ART_W / 2, 42, 1, 55, WHITE);
    ScreenProgress p = *s;
    p.over = 1; p.next = -1;
    pyramid(&p);
    int m = tmst_bracket_lost_at(s->bracket, s->bracket->player), champ = tmst_bracket_champion(s->bracket);
    art_fill(0, 424, ART_W, 56, STRIP);
    char line[128];
    if (m >= 0) {
        snprintf(line, sizeof line, "ELIMINATED BY %s  %s", s->roster->name(s->bracket->winner[m]),
                 round_names[tmst_bracket_round(m)]);
        art_text(line, ART_W / 2, 428, 1, 60, WHITE);
    }
    if (champ >= 0) {
        snprintf(line, sizeof line, "CHAMPION  %s", s->roster->name(champ));
        art_text(line, ART_SAFE_X + 8, 456, 0, 55, GOLD);
    }
    if (s->prompt) art_text(s->prompt, ART_SAFE_X + ART_SAFE_W - 4, 456, 2, 50, DIM);
}

/* ---- the guides: the boxes the screens fill, over the background ---- */
static void guide_box(int x, int y, int w, int h, uint32_t c) { art_frame(x, y, w, h, 1, c); }
void screen_guide(int which) {
    static const char *const names[4] = {"progress", "match", "champion", "over"};
    art_background(names[which]);
    art_frame(ART_SAFE_X, 0, ART_SAFE_W, ART_H, 1, 0xff30ff30u);       /* the 4:3 screen */
    uint32_t face_c = 0xffff40ffu, text_c = 0xff40c0ffu;
    if (which == 0 || which == 3) {
        for (int L = 0; L <= 5; L++)
            for (int j = 0; j < level_count[L]; j++) { int x, y, w, h; box(L, j, &x, &y, &w, &h); guide_box(x, y, w, h, face_c); }
        guide_box(ART_W / 2 - 200, 16, 400, 22, text_c);
        guide_box(ART_W / 2 - 120, 40, 240, 16, text_c);
        guide_box(0, 424, ART_W, 56, text_c);
    } else if (which == 1) {
        guide_box(150, 96, 220, 252, face_c); guide_box(ART_W - 370, 96, 220, 252, face_c);
        guide_box(ART_W / 2 - 200, 18, 400, 18, text_c); guide_box(ART_W / 2 - 180, 42, 360, 26, text_c);
        guide_box(ART_W / 2 - 30, 190, 60, 52, text_c);
        guide_box(150, 356, 220, 22, text_c); guide_box(ART_W - 370, 356, 220, 22, text_c);
        guide_box(140, 396, 210, 12, text_c);
    } else {
        guide_box(150, 106, 220, 252, face_c); guide_box(520, 150, 176, 202, face_c);
        guide_box(ART_W / 2 - 220, 18, 440, 50, text_c); guide_box(ART_W / 2 - 180, 72, 360, 18, text_c);
        guide_box(0, 404, ART_W, 76, text_c);
    }
}
