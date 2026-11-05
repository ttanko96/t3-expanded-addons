/* tmst.tekken2.stages: Tekken 2 Content's Stages, Tekken 2's 23 stages drawn
 * by the Expanded's stage engine (src/tekken3_arcade_stages.c) on its Tekken 3
 * path, as Tekken 3 arcade's are (its floor: the near cells in halves, lit
 * round the fighters where a stage wants; the user's choice, 2026-11-03).
 *
 * The packs: tools/tekken2_stages.py makes them at setup from the player's
 * Tekken 2 disc (format 3, numbers 20 + Tekken 2's stage) into
 * workspace/t2-stages/classic; the build stages them beside the game as
 * mods/t2-stages/classic (addon.cmake's TMST_STAGED).
 *
 * The engine reads arena n's pack from <its folder>/stageNN.ast (n 0..12),
 * once: it keeps it while the arena stays n, and an arena whose pack it could
 * not read stays the PS1's until another arena is tried. A Tekken 2 fight is
 * always in arena 9 (Nina's: none of the engine's per-stage rules, its far
 * background's or its objects', applies to it), written until the fight
 * (with the guests' arena, tekken3_stage_select_arena, and the STAGES row's
 * decision, tekken3_stage_select_choice, saying the same). Our fopen wrapper
 * hands the engine, for stage09.ast in that fight, a copy of our pack made
 * arena 9's (the number in its header; loaded.ast beside the packs). Which
 * pack the engine holds is followed through the opens it makes; at a fight's
 * start, when the one it holds for this arena is not the one wanted (the last
 * fight was another Tekken 2 stage in arena 9, or Tekken 2's, and this one is
 * the game's own), or the arena was refused before, the arena reads 8 (or 7)
 * as the PS1's stage routine next starts (tmst_tekken2_stage_entry), until the
 * engine opens that one: it gets an empty pack (blank.ast: nothing drawn, one
 * frame), and the arena is given back, which the engine then loads again.
 *
 * Without tekken3.zip: the engine's Tekken 3 path comes up only when the
 * Arcade Stages feature (on by default) finds one of its packs at its
 * activation (packs_present: fopen of stage00..12). Until the game starts,
 * none there and ours made, the fopen wrapper answers that look with one of
 * ours (whether this part is on or not: that package activates first, ids in
 * order). The engine is then up, and its other arenas stay the PS1's (no
 * pack: refused).
 *
 * Which fights: VS and Practice, from the STAGE line (F18,
 * src/tekken3_stage_select.c), where TEKKEN 2 is a game after the last of
 * theirs (before DEFAULT), its stages RANDOM then the 23. Their line keeps
 * DEFAULT while TEKKEN 2 shows, and all is done through their own keys: our
 * input wrapper keeps some presses from them (until let go) and passes the
 * others, so that their machine moves as it would (LEFT from TEKKEN 2 to
 * their last game, their take) and only shows ours (the line wrapper). A take
 * on TEKKEN 2 arms the line; the fight that loads next in its mode (their
 * fight hook) is ours: the stage source says Tekken 3's (the TTT music and
 * stage stay out); the music stays the fighter's.
 *
 * The pack's own sections (the engine reads only its own): T2FX, the
 * fighters' light (SetBackColor, 0x800826E8, dimmed on the dark stages, as the
 * remake's FDIM); T2CH, Hong Kong's running lights (palettes uploaded in turn);
 * T2MR, Mirror Darkness's mirror (src/tekken2/tmst_tekken2_mirror.c); T2AN,
 * the Remake's animations whose frames are not in the VRAM (the engine's ANIM
 * keeps its frames there; tools/t2_remake_stages.py frames_out()): each
 * period, as the PS1's stage routine starts and not in the pause, its next
 * frame uploaded to its place, as the engine's ANIM does.
 *
 * GAME OPTION's TEKKEN 2 STAGES row: CLASSIC, REMAKE. REMAKE draws the
 * Classic's packs while mods/t2-stages/remake has none (the user's choice,
 * 2026-11-03). The choices are kept beside the memory card in t2-stages.txt
 * ("vs on 13", "practice off random", "version classic").
 *
 * Benches: TMST_T2_STAGE=N gives every fight Tekken 2's stage N (0..22),
 * TMST_T2_STAGES_DIR the folder holding classic/ and remake/. */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef TMST_HAS_STAGE_ENGINE
#include "tekken3_stage_source.h"
#include "memcard.h"
#include "psx_sdl.h"
#ifdef TMST_HAS_GAME_OPTIONS
#include "tekken3_game_options.h"
#endif

enum { STAGES = 23, NUMBER0 = 20 };
static const char *const names[STAGES] = {
    "GRAND CANYON", "SZECHWAN FIELDS", "MORNING FIELDS", "NEW YORK CITY", "CHURCH SANCTUARY",
    "HONG KONG ROOFTOP", "PAGODA TEMPLE", "SERENE FOREST", "AKIHABARA", "THE GREAT WALL",
    "CAMBODIAN WETLANDS", "ATHENS", "NOWHERE DESERT", "LAUNCH PAD", "HILLS", "KYOTO AT SUNSET",
    "TAJ MAHAL", "BEACH", "LONDON RUINS", "ARIZONA DESERT", "KOREAN TEMPLE", "ETERNAL DARKNESS",
    "MIRROR DARKNESS" };

enum { SCREEN = 0x800ae204, MENU = 4, OPTIONS = 5, FIGHT = 8, MODE = 0x800afa88, ARENA = 0x800adc84,
       ARENA_COPY = 0x800afaa2, NO_STAGE = 0x800adca4, LAST_ARENA = 12, T2_ARENA = 9,
       PAUSED = 0x80095494,     /* 1 while the pause menu is open (the engine's anim_tick reads it) */
       VS = 1, PRACTICE = 5, ROW_MODES = 1u << VS | 1u << PRACTICE };
/* Pad bits, active low. */
enum { PAD_SELECT = 0x0001, PAD_START = 0x0008, PAD_UP = 0x0010, PAD_RIGHT = 0x0020, PAD_DOWN = 0x0040,
       PAD_LEFT = 0x0080, PAD_CIRCLE = 0x2000, PAD_CROSS = 0x4000 };
enum { LINE_VS, LINE_PRACTICE, LINES };
static const char *const line_words[LINES] = { "vs", "practice" };
enum { CLASSIC, REMAKE, VERSIONS };
static const char *const version_words[VERSIONS] = { "classic", "remake" };
static const char *const version_labels[VERSIONS] = { "CLASSIC", "REMAKE" };
/* What the engine holds for an arena: the game's own pack, the empty one, or
 * Tekken 2's stage n (0..22). */
enum { HELD_NONE = -3, HELD_BLANK = -2, HELD_REAL = -1 };
enum { NO_DIM = 0x80, CHASE_MAX = 8, CHASE_FRAMES = 8, ANIMS_MAX = 8 };
static const double TAU = 6.283185307179586;

int __real_tekken3_stage_source(void);
int __real_tekken3_stage_ttt_stage(void);
void __real_tekken3_stage_select_fight(unsigned mode);
int __real_tekken3_stage_select_choice(unsigned mode, int *source, int *ttt_stage, int *arena);
int __real_tekken3_stage_select_arena(void);
uint16_t __real_tekken3_stage_select_input(int player, uint16_t buttons);
int __real_tekken3_stage_select_line(const char **label, const char **name, int *arrows);
FILE *__real_fopen(const char *path, const char *mode);

static int stages;                       /* the feature, at activation */
static int t2[LINES];                    /* TEKKEN 2 on the line (theirs on DEFAULT) */
static int chosen[LINES] = { -1, -1 };   /* its stage, -1 RANDOM */
static int on_stage[LINES];              /* its STAGE step */
static int armed[LINES];                 /* taken on TEKKEN 2: the next fight of that mode */
static int version = CLASSIC;
static int fight = -1, fight_mode = -1, moving;   /* the fight's stage, its mode; arena 9 written */
static uint64_t draw = 0x9e3779b97f4a7c15u;
/* the engine's pack: the arena it holds one for, what it is (of Tekken 2's,
 * which version: CLASSIC or REMAKE, switched in GAME OPTION between two
 * fights on the same stage); the arena it last tried; a switch under way
 * (the arena read meanwhile, the fight's) */
static int held_arena = -1, held = HELD_NONE, held_version = CLASSIC, tried = -1;
static int switch_want = -1, switch_to = -1, switch_back = -1, switch_frames, checked;
/* src/tekken2/tmst_tekken2_extras.c: the Remake's extras (ANI2..4, DUST,
 * FFLY, FTHR, RAIN, DIYA) */
void tmst_tekken2_extras_load(const uint8_t *pack, long size);
void tmst_tekken2_extras_clear(void);
void tmst_tekken2_extras_tick(void);
int tmst_tekken2_extras_on(void);
int tmst_tekken2_extras_light(uint32_t *rgb, int *left, int *frames);
/* the fight's pack's own: the fighters' light (FDIM's tint, flicker), the
 * running lights */
static unsigned dim = NO_DIM;
static uint8_t tint[3], flicker;
/* T2LT (the Remake's): the fighters' light of a Tekken 3 arena instead of
 * arena 9's. Tekken 3 lights its fighters from 28 bytes an arena
 * (0x80096C78 + 28 * arena; the Expanded's B76, src/tekken3_arcade_stages.c):
 * the back colour at +0 (SetBackColor, each frame), the light's colour and
 * angles at +8..+15 (0x80039960, each round's set-up), colours at +4, +16,
 * +20 (the stage's load). Our fights are arena 9's, whose light is dim
 * (back 0x640, the others' 0x898) and orange: on a stage in daylight its
 * fighters looked dark (the user's report, 2026-11-05). From the fight's
 * choice (before its load) to its end, arena 9's first 24 bytes are those
 * of the arena T2LT names, put back after, as the Expanded does for a TTT
 * stage's LGHT. */
enum { LIGHT_TABLE = 0x80096c78, LIGHT_ENTRY = 28, LIGHT_BYTES = 24 };
static int light_from = -1, light_stood;
static uint8_t light_saved[LIGHT_BYTES];
/* FARB's bottom colour (its 12 colour bytes after 6 halfwords: cell rgb, top
 * rgb, top on, bottom rgb, bottom on): the engine's Tekken 3 path draws it
 * under the panorama only to 75 % of the screen (the TTT path has its
 * below_band), and Tekken 3 never clears the screen. Where the floor's far
 * edge lies lower (the camera's angle, a wide screen), a band of nothing
 * showed: an older frame in the fight, black in the pause (the user's
 * screenshot, Arizona, Practice, 2026-11-05). Filled with it under all of
 * the stage (src/tekken2/tmst_tekken2_mirror.c ground_fill). */
static uint8_t ground[3], ground_on;
/* FIXW (the Remake's Church): its scenery fixed to the floor, the floor's
 * period (src/tekken2/tmst_tekken2_anchor.c); 0: it follows. */
static int32_t fixed_period;
/* COLL (the Church's): its scenery as walls; the shapes in a copy
 * (src/tekken2/tmst_tekken2_anchor.c collide). */
static int16_t *coll_data;
static int coll_n, coll_radius;
static int32_t mirror_at[6];             /* T2MR: radius, angle, width, top, bottom, panels (0: none) */
static uint8_t mirror_style[20];         /* the Remake's MIRR after its place: the frame, the glow (0: none) */
static int mirror_styled;
static struct {
    unsigned period, frames, count, frame, clock;
    uint16_t at[CHASE_MAX][2];
    uint16_t colours[CHASE_FRAMES][CHASE_MAX][16];
} chase;
/* T2AN: the place (halfwords), the period, the entries (each a frame), the
 * frames' pixels; in the section's copy (anim_data) */
static uint8_t *anim_data;
static unsigned anims;
static struct {
    uint16_t x, y, w, h, period, entries, frames;
    const uint16_t *order, *pixels;
    unsigned clock, at;
} anim[ANIMS_MAX];

/* ---- the files ---- */

static const char *base_dir(void) {
    static char dir[1024];
    if (!dir[0]) {
        const char *base = SDL_GetBasePath();
        snprintf(dir, sizeof dir, "%s", base ? base : "");
#if !defined(PSX_SDL3)
        SDL_free((void *)base);
#endif
    }
    return dir;
}
/* The engine's folder of Tekken 3's packs, as it makes it (pack_dir_of). */
static const char *t3_dir(void) {
    static char dir[1024];
    if (!dir[0]) {
        const char *e = getenv("TEKKEN3_ARCADE_STAGES_DIR");
        if (e && *e) snprintf(dir, sizeof dir, "%s", e);
        else snprintf(dir, sizeof dir, "%smods/arcade-stages", base_dir());
    }
    return dir;
}
/* Our folder (classic/, remake/, the copies handed over). */
static void our_path(const char *name, char *out, size_t size) {
    const char *e = getenv("TMST_T2_STAGES_DIR");
    if (e && *e) snprintf(out, size, "%s/%s", e, name);
    else snprintf(out, size, "%smods/t2-stages/%s", base_dir(), name);
}
static void pack_path(int version_dir, int stage, char *out, size_t size) {
    char name[64];
    snprintf(name, sizeof name, "%s/stage%02d.ast", version_words[version_dir], stage);
    our_path(name, out, size);
}
static int there(const char *path) {
    FILE *f = __real_fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}
/* Any pack of that version (cached). */
static int version_made(int v) {
    static signed char made[VERSIONS];
    if (!made[v]) {
        char path[1100];
        made[v] = -1;
        for (int s = 0; s < STAGES && made[v] < 0; s++) { pack_path(v, s, path, sizeof path); if (there(path)) made[v] = 1; }
    }
    return made[v] > 0;
}
/* The version drawn: REMAKE's packs only once they exist. */
static int drawn_version(void) { return version == REMAKE && version_made(REMAKE) ? REMAKE : CLASSIC; }
/* Tekken 3's own packs (tekken3.zip), any of the 13 (cached). */
static int t3_real(void) {
    static signed char real;
    if (!real) {
        char path[1100];
        real = -1;
        for (int s = 0; s <= LAST_ARENA && real < 0; s++) {
            snprintf(path, sizeof path, "%s/stage%02d.ast", t3_dir(), s);
            if (there(path)) real = 1;
        }
    }
    return real > 0;
}

/* A pack's file, read whole (NULL: none); its size in *size. */
static uint8_t *read_all(const char *path, long *size) {
    FILE *f = __real_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = *size > 16 ? malloc((size_t)*size) : NULL;
    if (b && fread(b, 1, (size_t)*size, f) != (size_t)*size) { free(b); b = NULL; }
    fclose(f);
    return b;
}
/* A format 3 pack's section (NULL: none). */
static const uint8_t *section(const uint8_t *b, long size, const char *id, uint32_t *len) {
    uint32_t n; memcpy(&n, b + 8, 4);
    for (uint32_t i = 0; i < n && 16 + 12 * (long)i + 12 <= size; i++) {
        if (memcmp(b + 16 + 12 * i, id, 4)) continue;
        uint32_t o, l; memcpy(&o, b + 20 + 12 * i, 4); memcpy(&l, b + 24 + 12 * i, 4);
        if ((long)o + (long)l > size) return NULL;
        *len = l;
        return b + o;
    }
    return NULL;
}

/* Our pack of Tekken 2's stage as arena n's (loaded.ast): the engine checks
 * that the header's number is the arena it asked for. */
static FILE *hand_over(int stage, int n, const char *mode) {
    char from[1100], to[1100];
    long size = 0;
    pack_path(drawn_version(), stage, from, sizeof from);
    uint8_t *b = read_all(from, &size);
    if (!b) return NULL;
    uint16_t number = (uint16_t)n;
    memcpy(b + 6, &number, 2);
    our_path("loaded.ast", to, sizeof to);
    FILE *f = __real_fopen(to, "wb");
    int ok = f && fwrite(b, 1, (size_t)size, f) == (size_t)size;
    if (f) fclose(f);
    free(b);
    return ok ? __real_fopen(to, mode) : NULL;
}

/* An empty pack for arena n (blank.ast): nothing to draw (no far background
 * cells or bands, no floor quads), its tables computed. */
static FILE *blank(int n, const char *mode) {
    enum { VRAM_B = 4, TILE_B = 8, FARB_B = 24, FLOR_B = 8 + 800 + 3200 + 100 + 4 + 100 + 2 * 4 * 4 * (1 + 2 + 4),
           TRIG_B = 2 * (4096 + 4096 + 192) + 1024 + 2 * 192, HEAD = 16 + 5 * 12 };
    static const char ids[5][5] = { "VRAM", "TILE", "FARB", "FLOR", "TRIG" };
    static const uint32_t sizes[5] = { VRAM_B, TILE_B, FARB_B, FLOR_B, TRIG_B };
    static uint8_t b[HEAD + VRAM_B + TILE_B + FARB_B + FLOR_B + TRIG_B];
    uint32_t total = sizeof b, count = 5, at = HEAD, tile = 800;
    uint16_t version3 = 3, number = (uint16_t)n;
    memset(b, 0, sizeof b);
    memcpy(b, "ASTG", 4); memcpy(b + 4, &version3, 2); memcpy(b + 6, &number, 2);
    memcpy(b + 8, &count, 4); memcpy(b + 12, &total, 4);
    for (int i = 0; i < 5; i++) {
        memcpy(b + 16 + 12 * i, ids[i], 4); memcpy(b + 20 + 12 * i, &at, 4); memcpy(b + 24 + 12 * i, &sizes[i], 4);
        if (i == 1) memcpy(b + at, &tile, 4);                     /* TILE: its size, no tiles */
        if (i == 2) {                                             /* FARB: 1 column, no rows, no bands */
            const int16_t h[6] = { 1, 0, 1, 0, 45, 0 };
            memcpy(b + at, h, sizeof h);
        }
        if (i == 4) {                                             /* TRIG */
            int16_t *t = (int16_t *)(b + at);
            for (int a = 0; a < 4096; a++) {
                t[a] = (int16_t)lround(4096 * sin(TAU * a / 4096));
                t[4096 + a] = (int16_t)lround(4096 * cos(TAU * a / 4096));
            }
            for (int k = 0; k < 192; k++) t[8192 + k] = (int16_t)(sqrt(64.0 + k) * 512);
            uint8_t *fog = b + at + 2 * (8192 + 192);
            for (int k = 0; k < 1024; k++) fog[k] = (uint8_t)lround(127 * pow(k / 1024.0, 1.5));
            memcpy(fog + 1024, t + 8192, 2 * 192);
        }
        at += sizes[i];
    }
    char path[1100];
    our_path("blank.ast", path, sizeof path);
    FILE *f = __real_fopen(path, "wb");
    int ok = f && fwrite(b, 1, sizeof b, f) == sizeof b;
    if (f) fclose(f);
    return ok ? __real_fopen(path, mode) : NULL;
}

/* The number of a Tekken 3 pack the engine opens (<its folder>/stageNN.ast),
 * -1 for any other file. */
static int t3_pack(const char *path) {
    size_t n = strlen(path);
    if (n < 12) return -1;
    const char *b = path + n - 11;                   /* stageNN.ast */
    if (b[-1] != '/' && b[-1] != '\\') return -1;
    if (memcmp(b, "stage", 5) || b[5] < '0' || b[5] > '9' || b[6] < '0' || b[6] > '9' || strcmp(b + 7, ".ast"))
        return -1;
    const char *dir = t3_dir();
    size_t d = strlen(dir);
    if ((size_t)(b - 1 - path) != d || memcmp(path, dir, d)) return -1;
    return (b[5] - '0') * 10 + (b[6] - '0');
}

static int our_fight(void);

FILE *__wrap_fopen(const char *path, const char *mode) {
    int n = path && mode && mode[0] == 'r' ? t3_pack(path) : -1;
    if (n < 0) return __real_fopen(path, mode);
    char ours[1100];
    if (!psx_mod_game_started()) {
        /* Arcade Stages' activation looking for its packs: one of ours, when it has none */
        FILE *f = __real_fopen(path, mode);
        if (f || n > LAST_ARENA || t3_real()) return f;
        for (int s = 0; s < STAGES && !f; s++) { pack_path(CLASSIC, s, ours, sizeof ours); f = __real_fopen(ours, mode); }
        if (f) tmst_log("tekken2: no Tekken 3 arcade stage packs, ours bring up the stage engine");
        return f;
    }
    FILE *f;
    int what;
    tried = n;
    if (n == switch_to) {                            /* the switch: the empty pack, the arena given back */
        f = blank(n, mode); what = HELD_BLANK;
        psx_mod_write_half(ARENA, (uint16_t)switch_back);
        switch_to = -1;
    } else if (n == T2_ARENA && our_fight()) {
        f = hand_over(fight, n, mode); what = fight;
        if (f) held_version = drawn_version();
    } else {
        f = __real_fopen(path, mode); what = HELD_REAL;
    }
    if (f) { held_arena = n; held = what; }
    return f;
}

/* Kept beside the memory card, as the Expanded's stage-select.txt. */
static void setting_path(char *out, size_t size) {
    const char *card = NULL; out[0] = 0;
    if (memcard_debug_info(0, &card, NULL, NULL, NULL) || !card || !*card) return;
    const char *slash = strrchr(card, '/'), *back = strrchr(card, '\\');
    if (back && (!slash || back > slash)) slash = back;
    int n = slash ? (int)(slash - card) : 1;
    snprintf(out, size, "%.*s/t2-stages.txt", n, slash ? card : ".");
}
static void save(void) {
    char path[4096]; setting_path(path, sizeof path);
    FILE *f = path[0] ? fopen(path, "wb") : NULL;
    if (!f) return;
    for (int i = 0; i < LINES; i++) {
        char stage[8] = "random";
        if (chosen[i] >= 0) snprintf(stage, sizeof stage, "%d", chosen[i]);
        fprintf(f, "%s %s %s\n", line_words[i], t2[i] ? "on" : "off", stage);
    }
    fprintf(f, "version %s\n", version_words[version]);
    fclose(f);
}
static void load(void) {
    static int done;
    if (done) return;
    done = 1;
    draw ^= (uint64_t)time(NULL) * 1000003u + (uint64_t)clock();
    char path[4096]; setting_path(path, sizeof path);
    FILE *f = path[0] ? fopen(path, "rb") : NULL;
    if (!f) return;
    char key[16], a[16], b[16], line[64];
    while (fgets(line, sizeof line, f)) {
        int n = sscanf(line, "%15s %15s %15s", key, a, b);
        if (n == 2 && !strcmp(key, "version")) version = !strcmp(a, "remake") ? REMAKE : CLASSIC;
        for (int i = 0; n == 3 && i < LINES; i++) {
            if (strcmp(key, line_words[i])) continue;
            t2[i] = !strcmp(a, "on");
            int s = atoi(b);
            chosen[i] = !strcmp(b, "random") || s < 0 || s >= STAGES ? -1 : s;
        }
    }
    fclose(f);
}

/* This part can draw: on, its packs there, the engine's Tekken 3 path up. */
static int ready(void) {
    if (!stages || !tmst_tekken2_on() || !tekken3_arcade_stages_source_ready(TEKKEN3_STAGE_T3)) return 0;
    if (!version_made(CLASSIC)) return 0;
    load();
    return 1;
}

static int pick(int count) {
    draw ^= draw << 13; draw ^= draw >> 7; draw ^= draw << 17;
    return (int)(draw % (uint64_t)count);
}
static const char *stage_name(int s) { return s < 0 ? "RANDOM" : names[s]; }

/* The fight's pack's own sections: the fighters' light, the running lights. */
static void fight_extras(void) {
    char path[1100];
    long size = 0;
    dim = NO_DIM;
    memset(tint, 0, sizeof tint);
    flicker = 0;
    light_from = -1;
    ground_on = 0;
    fixed_period = 0;
    free(coll_data);
    coll_data = NULL;
    coll_n = coll_radius = 0;
    memset(&chase, 0, sizeof chase);
    memset(mirror_at, 0, sizeof mirror_at);
    free(anim_data);
    anim_data = NULL;
    anims = 0;
    tmst_tekken2_extras_clear();
    pack_path(drawn_version(), fight, path, sizeof path);
    uint8_t *b = read_all(path, &size);
    if (!b) return;
    uint32_t len = 0;
    const uint8_t *o = section(b, size, "T2FX", &len);
    if (o && len >= 2) { uint16_t v; memcpy(&v, o, 2); dim = v ? v : NO_DIM; }
    /* FDIM (the Remake's): after its dim (T2FX's), r, g, b / 0x80 and a flicker's depth */
    o = section(b, size, "FDIM", &len);
    if (o && len >= 8) { memcpy(tint, o + 4, 3); flicker = o[7]; }
    o = section(b, size, "FARB", &len);
    if (o && len >= 24 && o[22]) { memcpy(ground, o + 19, 3); ground_on = 1; }
    o = section(b, size, "FIXW", &len);
    if (o && len >= 4) { memcpy(&fixed_period, o, 4); if (fixed_period < 0) fixed_period = 0; }
    o = section(b, size, "COLL", &len);
    if (o && len >= 4) {
        int16_t c[2]; memcpy(c, o, 4);
        if (c[0] > 0 && len >= 4u + 12u * (unsigned)c[0] && (coll_data = malloc(12u * (unsigned)c[0])) != NULL) {
            memcpy(coll_data, o + 4, 12u * (unsigned)c[0]);
            coll_n = c[0]; coll_radius = c[1];
        }
    }
    o = section(b, size, "T2LT", &len);
    if (o && len >= 2) {
        uint16_t a; memcpy(&a, o, 2);
        if (a <= 17 && a != T2_ARENA) light_from = a;
    }
    tmst_tekken2_extras_load(b, size);
    o = section(b, size, "T2MR", &len);
    if (o && len >= sizeof mirror_at) memcpy(mirror_at, o, sizeof mirror_at);
    /* MIRR (the old fork's, the Remake's): its style after the place, 20 bytes */
    memset(mirror_style, 0, sizeof mirror_style);
    mirror_styled = 0;
    o = section(b, size, "MIRR", &len);
    if (o && len >= 24 + 17) { memcpy(mirror_style, o + 24, len - 24 < 20 ? len - 24 : 20); mirror_styled = 1; }
    o = section(b, size, "T2CH", &len);
    if (o && len >= 8) {
        uint16_t h[4]; memcpy(h, o, 8);
        if (h[1] && h[1] <= CHASE_FRAMES && h[2] && h[2] <= CHASE_MAX && len >= 8 + 4u * h[2] + 32u * h[1] * h[2]) {
            chase.period = h[0] ? h[0] : 1; chase.frames = h[1]; chase.count = h[2];
            memcpy(chase.at, o + 8, 4u * h[2]);
            for (unsigned f = 0; f < h[1]; f++)
                for (unsigned p = 0; p < h[2]; p++)
                    memcpy(chase.colours[f][p], o + 8 + 4u * h[2] + 32u * (f * h[2] + p), 32);
        }
    }
    o = section(b, size, "T2AN", &len);
    if (o && len >= 4 && (anim_data = malloc(len)) != NULL) {
        memcpy(anim_data, o, len);
        uint32_t n, at = 4; memcpy(&n, anim_data, 4);
        for (uint32_t i = 0; i < n && anims < ANIMS_MAX && at + 20 <= len; i++) {
            uint16_t h[8]; memcpy(h, anim_data + at + 4, 16);
            uint32_t order = at + 20, pixels = order + ((2u * h[5] + 3u) & ~3u);
            uint32_t end = pixels + 2u * h[2] * h[3] * h[6];
            if (!h[2] || !h[3] || !h[5] || !h[6] || end > len) break;
            int good = 1;
            for (unsigned e = 0; e < h[5]; e++) {
                uint16_t f; memcpy(&f, anim_data + order + 2 * e, 2);
                if (f >= h[6]) good = 0;
            }
            if (good) {
                anim[anims].x = h[0]; anim[anims].y = h[1]; anim[anims].w = h[2]; anim[anims].h = h[3];
                anim[anims].period = h[4] ? h[4] : 1; anim[anims].entries = h[5]; anim[anims].frames = h[6];
                anim[anims].order = (const uint16_t *)(anim_data + order);
                anim[anims].pixels = (const uint16_t *)(anim_data + pixels);
                anim[anims].clock = anim[anims].at = 0;
                anims++;
            }
            at = end;
        }
    }
    free(b);
}

/* ---- the engine's questions ---- */

/* Our fight: its mode still, in arena 9. */
static int our_fight(void) {
    return fight >= 0 && psx_mod_read_word(MODE) == (uint32_t)fight_mode && psx_mod_read_half(ARENA) == T2_ARENA;
}
/* Our pack drawn now: our fight, the engine holding it. */
static int drawn(void) {
    return our_fight() && held_arena == T2_ARENA && held == fight && held_version == drawn_version() &&
           !psx_mod_read_byte(NO_STAGE) &&
           psx_mod_read_word(SCREEN) == FIGHT;
}
/* For src/tekken2/tmst_tekken2_mirror.c: the mirror's place while our pack
 * with one is drawn. */
int tmst_tekken2_mirror(int32_t at[6]) {
    if (mirror_at[0] <= 0 || !drawn()) return 0;
    memcpy(at, mirror_at, sizeof mirror_at);
    return 1;
}
/* ... and its style (the Remake's MIRR: frame, rails, glow), 0: none. */
int tmst_tekken2_mirror_style(uint8_t style[20]) {
    if (!mirror_styled) return 0;
    memcpy(style, mirror_style, sizeof mirror_style);
    return 1;
}
/* ... and whether its extras are to be drawn this frame. */
int tmst_tekken2_extras_wanted(void) { return tmst_tekken2_extras_on() && drawn(); }
/* ... whether our pack is drawn now (src/tekken2/tmst_tekken2_trace.c). */
int tmst_tekken2_drawn(void) { return drawn(); }
/* ... a Remake pack, drawn now (src/tekken2/tmst_tekken2_anchor.c). */
int tmst_tekken2_remake_drawn(void) { return drawn() && held_version == REMAKE; }
int32_t tmst_tekken2_fixed_period(void) { return fixed_period; }
/* ... and its walls: the shapes (6 halfwords each), their count, the fighters' radius (cm). */
const int16_t *tmst_tekken2_walls(int *n, int *radius) { *n = coll_n; *radius = coll_radius; return coll_data; }
/* ... and the colour under all of its stage (FARB's bottom colour), 0: none. */
int tmst_tekken2_ground(uint8_t rgb[3]) {
    if (!ground_on || !drawn()) return 0;
    memcpy(rgb, ground, 3);
    return 1;
}
int __wrap_tekken3_stage_source(void) { return our_fight() ? TEKKEN3_STAGE_T3 : __real_tekken3_stage_source(); }
int __wrap_tekken3_stage_ttt_stage(void) { return our_fight() ? -1 : __real_tekken3_stage_ttt_stage(); }

/* From the routine that picks the fight's arena (0x8004F3B0), each fight. */
void __wrap_tekken3_stage_select_fight(unsigned mode) {
    __real_tekken3_stage_select_fight(mode);
    fight = fight_mode = -1; moving = 0;
    if (!ready()) return;
    int line = mode == VS ? LINE_VS : mode == PRACTICE ? LINE_PRACTICE : -1, stage = -1;
    const char *e = getenv("TMST_T2_STAGE");
    if (e && *e) stage = atoi(e) % STAGES;
    else if (line >= 0 && armed[line]) stage = chosen[line] >= 0 ? chosen[line] : pick(STAGES);
    if (line >= 0) armed[line] = 0;
    if (stage < 0) return;
    fight = stage; fight_mode = (int)mode; moving = 1;
    fight_extras();
    tmst_log("tekken2: mode %u fight on Tekken 2's %s (stage %d, %s, arena %d)%s", mode, names[stage], stage,
             version_words[drawn_version()], T2_ARENA, line >= 0 && chosen[line] < 0 && !(e && *e) ? ", RANDOM" : "");
}
/* The same arena for the guests' correction (src/tekken3_ttt1_roster.c) and
 * the STAGES row's decision (Tekken 3's stage there: nothing moved). */
int __wrap_tekken3_stage_select_arena(void) {
    return fight >= 0 && moving ? T2_ARENA : __real_tekken3_stage_select_arena();
}
int __wrap_tekken3_stage_select_choice(unsigned mode, int *source, int *ttt_stage, int *arena) {
    if (fight < 0 || (unsigned)fight_mode != mode) return __real_tekken3_stage_select_choice(mode, source, ttt_stage, arena);
    *source = TEKKEN3_STAGE_T3; *ttt_stage = -1; *arena = T2_ARENA;
    return 1;
}

/* ---- the STAGE line ---- */

static uint16_t previous[2] = { 0xffff, 0xffff }, kept[2];

static void set_t2(int line, int on, unsigned player) {
    t2[line] = on; on_stage[line] = 0;
    tmst_log("tekken2: P%u sets the %s game to %s", player + 1, line ? "Practice" : "VS", on ? "TEKKEN 2" : "theirs");
    save();
}

uint16_t __wrap_tekken3_stage_select_input(int player, uint16_t buttons) {
    if (player < 0 || player > 1 || !psx_mod_game_started() || !ready())
        return __real_tekken3_stage_select_input(player, buttons);
    unsigned p = (unsigned)player;
    uint16_t edges = previous[p] & (uint16_t)~buttons, keep = 0;
    previous[p] = buttons;
    kept[p] &= (uint16_t)~buttons;                   /* kept keys let go */
    const char *label, *name; int arrows;
    int kind = __real_tekken3_stage_select_line(&label, &name, &arrows), line = kind - 1;
    if (!kind) on_stage[LINE_VS] = on_stage[LINE_PRACTICE] = 0;
    if (!kind || (line == LINE_PRACTICE && p)) return __real_tekken3_stage_select_input(player, buttons | kept[p]);
    int game_step = !strcmp(label, "GAME"), at_default = game_step && !strcmp(name, "DEFAULT");
    if (t2[line] && !at_default) { t2[line] = on_stage[line] = 0; save(); }   /* theirs moved on */
    armed[line] = 0;
    if (t2[line] && !on_stage[line]) {               /* GAME: TEKKEN 2 */
        if (edges & PAD_RIGHT) { set_t2(line, 0, p); keep = PAD_RIGHT; }        /* to their DEFAULT */
        else if (edges & PAD_LEFT) set_t2(line, 0, p);                          /* passed: their last game */
        else if ((edges & PAD_DOWN) || (line == LINE_PRACTICE && (edges & (PAD_CROSS | PAD_START)))) {
            on_stage[line] = 1; keep = edges & (PAD_DOWN | PAD_CROSS | PAD_START);
        } else if (edges & (PAD_CROSS | PAD_START)) armed[line] = 1;           /* VS: their take */
    } else if (t2[line]) {                           /* STAGE: RANDOM, then the 23 */
        if (edges & (PAD_LEFT | PAD_RIGHT)) {
            int s = chosen[line] + 1 + (edges & PAD_LEFT ? -1 : 1);
            chosen[line] = (s + STAGES + 1) % (STAGES + 1) - 1;
            keep = edges & (PAD_LEFT | PAD_RIGHT);
            tmst_log("tekken2: P%u sets the %s stage to %s", p + 1, line ? "Practice" : "VS", stage_name(chosen[line]));
            save();
        } else if (edges & (PAD_UP | PAD_CIRCLE | PAD_SELECT)) {
            on_stage[line] = 0; keep = edges & (PAD_UP | PAD_CIRCLE | PAD_SELECT);
        } else if (edges & (PAD_CROSS | PAD_START)) armed[line] = 1;           /* their take, on DEFAULT */
        else keep = edges & PAD_DOWN;
    } else if (at_default && (edges & PAD_LEFT)) {   /* their DEFAULT back to TEKKEN 2 */
        set_t2(line, 1, p); keep = PAD_LEFT;
    }
    kept[p] |= keep;
    uint16_t out = __real_tekken3_stage_select_input(player, buttons | kept[p]);
    /* RIGHT from their last game: their DEFAULT, TEKKEN 2 shows between */
    if (!t2[line] && game_step && !at_default && (edges & PAD_RIGHT) &&
        __real_tekken3_stage_select_line(&label, &name, &arrows) == kind && !strcmp(label, "GAME") &&
        !strcmp(name, "DEFAULT"))
        set_t2(line, 1, p);
    if (armed[line]) tmst_log("tekken2: P%u takes Tekken 2's %s for %s", p + 1, stage_name(chosen[line]),
                              line ? "Practice" : "VS");
    return out;
}

int __wrap_tekken3_stage_select_line(const char **label, const char **name, int *arrows) {
    int kind = __real_tekken3_stage_select_line(label, name, arrows), line = kind - 1;
    if (!kind || !ready() || !t2[line] || strcmp(*label, "GAME") || strcmp(*name, "DEFAULT")) return kind;
    if (!on_stage[line]) *name = "TEKKEN 2";
    else { *label = "TEKKEN 2"; *name = stage_name(chosen[line]); }
    return kind;
}

/* ---- every VBlank ---- */

/* GAME OPTION's row, added at the first visit of the Options screen. */
static void menu(void) {
#ifdef TMST_HAS_GAME_OPTIONS
    static int row = -2, shown;
    if (row == -2) {
        row = tekken3_game_options_add("TEKKEN 2 STAGES", version_labels, VERSIONS, (unsigned)version, CLASSIC,
                                       ROW_MODES);
        shown = version;
    }
    if (row < 0) return;
    int v = tekken3_game_options_value(row);
    if (v >= 0 && v < VERSIONS && v != shown) {
        shown = version = v;
        save();
        tmst_log("tekken2: TEKKEN 2 STAGES set to %s%s", version_labels[v],
                 v == REMAKE && !version_made(REMAKE) ? " (no Remake packs yet: Classic drawn)" : "");
    }
#endif
}

/* At a fight's start: the engine about to draw from a pack it holds for this
 * arena that is not this fight's, or not to try the arena again (refused
 * before) when Tekken 2's is wanted there: another arena read until it opens
 * that one (the empty pack; fopen gives the arena back). */
static void fight_start(void) {
    int arena = psx_mod_read_half(ARENA);
    if (arena < 0 || arena > LAST_ARENA || psx_mod_read_byte(NO_STAGE)) return;
    int want = our_fight() ? fight : HELD_REAL;
    int stale = held_arena == arena && (held != want || (want >= 0 && held_version != drawn_version()));
    int refused = want >= 0 && tried == arena && held_arena != arena;
    if (!stale && !refused) return;
    /* an arena the engine will load: not this one, not the one it holds, not
     * the one it tried last */
    static const int others[4] = { 8, 7, 10, 1 };
    int to = -1;
    for (int i = 0; i < 4 && to < 0; i++)
        if (others[i] != arena && others[i] != held_arena && others[i] != tried) to = others[i];
    if (to < 0) return;
    switch_back = arena;
    switch_want = to;
    tmst_log("tekken2: the engine holds %s%s for arena %d, %s%s wanted: arena %d until it loads that one",
             held == HELD_REAL ? "the game's pack" : held == HELD_BLANK ? "the empty pack" : held >= 0 ? names[held]
             : "nothing", held >= 0 ? (held_version == REMAKE ? " (remake)" : " (classic)") : "", arena,
             want >= 0 ? names[want] : "the game's", want >= 0 ? (drawn_version() == REMAKE ? " (remake)" : " (classic)")
             : "", switch_want);
}

/* From src/tekken2/tmst_tekken2_mirror.c, as the PS1's stage routine
 * (0x8006D014) starts, before the engine's wrapper asks for its pack: the
 * switch's arena, read by the engine this very frame. */
void tmst_tekken2_stage_entry(void) {
    /* the T2AN animations: once a frame of the stage, not in the pause */
    if (anims && drawn() && !psx_mod_read_word(PAUSED))
        for (unsigned i = 0; i < anims; i++) {
            if (++anim[i].clock < anim[i].period) continue;
            anim[i].clock = 0;
            unsigned f = anim[i].order[anim[i].at++ % anim[i].entries];
            (void)psx_mod_gpu_high_vram_upload(anim[i].x, anim[i].y, anim[i].w, anim[i].h,
                                               anim[i].pixels + (size_t)f * anim[i].w * anim[i].h);
        }
    /* the extras' palettes and rounds (not in the pause: theirs) */
    if (tmst_tekken2_extras_on() && drawn()) tmst_tekken2_extras_tick();
    if (switch_want < 0 || psx_mod_read_word(SCREEN) != FIGHT) return;
    switch_to = switch_want; switch_want = -1; switch_frames = 0;
    psx_mod_write_half(ARENA, (uint16_t)switch_to);
}

/* T2LT: arena 9's light entry the named arena's while our fight is on (its
 * mode still), its own bytes back after. */
static void light_tick(void) {
    int want = light_from >= 0 && fight >= 0 && psx_mod_read_word(MODE) == (uint32_t)fight_mode;
    uint32_t at = LIGHT_TABLE + LIGHT_ENTRY * T2_ARENA;
    if (want) {
        uint8_t e[LIGHT_BYTES];
        for (int i = 0; i < LIGHT_BYTES; i++) e[i] = psx_mod_read_byte(LIGHT_TABLE + LIGHT_ENTRY * (uint32_t)light_from + i);
        if (!light_stood) {
            for (int i = 0; i < LIGHT_BYTES; i++) light_saved[i] = psx_mod_read_byte(at + i);
            light_stood = 1;
            tmst_log("tekken2: arena %d's light on the fighters of %s", light_from, names[fight]);
        }
        for (int i = 0; i < LIGHT_BYTES; i++)
            if (psx_mod_read_byte(at + i) != e[i]) psx_mod_write_byte(at + i, e[i]);
    } else if (light_stood) {
        for (int i = 0; i < LIGHT_BYTES; i++) psx_mod_write_byte(at + i, light_saved[i]);
        light_stood = 0;
    }
}

static void tick(void) {
    if (!psx_mod_game_started() || !ready()) return;
    light_tick();
    unsigned screen = psx_mod_read_word(SCREEN);
    if (screen == OPTIONS) menu();
    if (screen == MENU) fight = -1;
    if (moving && screen == FIGHT) moving = 0;
    /* arena 9 until the fight */
    if (moving && fight >= 0 && psx_mod_read_half(ARENA) != T2_ARENA) {
        psx_mod_write_half(ARENA, T2_ARENA);
        psx_mod_write_byte(ARENA_COPY, T2_ARENA);
    }
    if (screen != FIGHT) { checked = 0; switch_want = -1; return; }
    if (switch_to >= 0 && ++switch_frames > 8) {     /* the engine never asked: the arena back */
        psx_mod_write_half(ARENA, (uint16_t)switch_back);
        switch_to = -1;
    }
    if (!checked) { checked = 1; fight_start(); }
    /* Hong Kong's running lights: the next frame's palettes */
    if (chase.count && drawn() && ++chase.clock >= chase.period) {
        chase.clock = 0;
        chase.frame = (chase.frame + 1) % chase.frames;
        for (unsigned p = 0; p < chase.count; p++)
            (void)psx_mod_gpu_high_vram_upload(chase.at[p][0], chase.at[p][1], 16, 1, chase.colours[chase.frame][p]);
    }
}

static void activate(void) {
    stages = tmst_guard("tmst.tekken2.stages", TMST_NEED_ACTIVATION | TMST_NEED_VBLANK | TMST_NEED_HIGH_VRAM);
    if (!stages) return;
    if (!version_made(CLASSIC)) tmst_log("tekken2: no Tekken 2 stage packs (Setup TMST.cmd with the disc), stages off");
    else if (!tekken3_arcade_stages_source_ready(TEKKEN3_STAGE_T3))
        tmst_log("tekken2: the stage engine is off (Arcade Stages off, or no upper VRAM), stages off");
    else tmst_log("tekken2: Tekken 2 stages on (TEKKEN 2 on the STAGE line), %s Tekken 3's packs", t3_real() ? "with" : "without");
}

PSX_MOD_CONSTRUCTOR(tmst_register_tekken2_stages) {
    (void)psx_mod_register_activation_plugin("tmst.tekken2.stages", activate);
    (void)psx_mod_register_vblank_plugin("tmst.tekken2.stages", tick);
}

#else   /* an Expanded without the stage engine: the part says so */

int tmst_tekken2_mirror(int32_t at[6]) { (void)at; return 0; }
int tmst_tekken2_mirror_style(uint8_t style[20]) { (void)style; return 0; }
int tmst_tekken2_extras_wanted(void) { return 0; }
int tmst_tekken2_ground(uint8_t rgb[3]) { (void)rgb; return 0; }
int tmst_tekken2_drawn(void) { return 0; }
int tmst_tekken2_remake_drawn(void) { return 0; }
int32_t tmst_tekken2_fixed_period(void) { return 0; }
const int16_t *tmst_tekken2_walls(int *n, int *radius) { *n = *radius = 0; return NULL; }
void tmst_tekken2_stage_entry(void) {}

static void activate(void) {
    if (tmst_guard("tmst.tekken2.stages", TMST_NEED_ACTIVATION))
        tmst_log("tekken2: this Expanded has no stage engine to draw them, Tekken 2 stages off");
}

PSX_MOD_CONSTRUCTOR(tmst_register_tekken2_stages) {
    (void)psx_mod_register_activation_plugin("tmst.tekken2.stages", activate);
}
#endif

/* 0x800826E8 SetBackColor(r, g, b): the fighters' ambient light (its callers
 * 0x80039CC0, 0x8003A164, 0x8003A2C8), dimmed on a dark Tekken 2 stage
 * (T2FX), as the remake's FDIM. Chained: the Expanded may wrap it one day. */
extern void __tmst_chain_func_800826E8(CPUState *cpu);
#ifdef TMST_BRIDGE_func_800826E8
extern void __real_func_800826E8(CPUState *cpu);
void __tmst_chain_func_800826E8(CPUState *cpu) { __real_func_800826E8(cpu); }
#endif
void __wrap_func_800826E8(CPUState *cpu) {
#ifdef TMST_HAS_STAGE_ENGINE
    if (dim != NO_DIM && (cpu->pc == 0 || cpu->pc == 0x800826e8u) && cpu->gpr[31] >= 0x80039c00u &&
        cpu->gpr[31] < 0x8003a400u && drawn())
        for (int k = 0; k < 3; k++) cpu->gpr[4 + k] = (uint32_t)((int)cpu->gpr[4 + k] * (int)dim / NO_DIM);
    /* FDIM's candlelight: tinted, its level wavering (three slow waves of
     * the vsync count), as the old fork's */
    if (tint[0] && (cpu->pc == 0 || cpu->pc == 0x800826e8u) && cpu->gpr[31] >= 0x80039c00u &&
        cpu->gpr[31] < 0x8003a400u && drawn()) {
        float t = (float)psx_mod_read_word(0x800a08f0u);
        float w = 0.5f * sinf(t * 0.11f) + 0.3f * sinf(t * 0.27f + 1.3f) + 0.2f * sinf(t * 0.61f + 2.1f);
        float lv = 1.0f + w * (float)flicker / 128.0f;
        for (int k = 0; k < 3; k++) {
            int v = (int)((float)(int)cpu->gpr[4 + k] * (float)tint[k] / 128.0f * lv);
            cpu->gpr[4 + k] = (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }
    /* a lightning flash adds its light, fading over its frames (the Remake's LGHT) */
    uint32_t rgb; int left, frames;
    if ((cpu->pc == 0 || cpu->pc == 0x800826e8u) && cpu->gpr[31] >= 0x80039c00u && cpu->gpr[31] < 0x8003a400u &&
        tmst_tekken2_extras_light(&rgb, &left, &frames) && drawn())
        for (int k = 0; k < 3; k++) {
            int v = (int)cpu->gpr[4 + k] + (int)((rgb >> (8 * k)) & 0xFF) * left / frames;
            cpu->gpr[4 + k] = (uint32_t)(v > 255 ? 255 : v);
        }
#endif
    __tmst_chain_func_800826E8(cpu);
}
