/* tmst.tekken2.bighead: Tekken 2 Content's Big Head Mode, Tekken 2's hidden
 * mode rebuilt.
 *
 * Tekken 2 (SLUS-00213 v1.1, read from its program): with a switch on
 * (0x800D2D58) and Select held, each model part's vertices are multiplied in
 * place by a table's x, y, z (0x800A0F10, 16 = 1) at most twice a load: the
 * head x1.5 (then x2.25), the hands and the limbs' thickness less.
 *
 * Here not Select at the selector but GAME OPTION's TEKKEN 2 BIG HEAD MODE
 * row, OFF / BIG / BIGGER (Tekken 2's two sizes; the user's choice,
 * 2026-11-15), kept beside the memory card in t2-bighead.txt ("bighead
 * big"). All fighters, in Arcade, VS, Team Battle (the Tournament is one),
 * Time Attack, Survival, Practice, Tekken Ball and Tekken Force (the user's
 * wish, 2026-11-15: there the player and up to two enemies, records 0..2,
 * measured).
 *
 * The voices (Tekken 2: its sound player, 0x800745E0, raises a character's
 * own sounds, category 4 on, by 3 semitones while that player's model has
 * grown: the note + 3, both sizes). Tekken 3: sound_play (0x800753BC) takes
 * a key whose bank bits 0xC000 mean the player's own bank (0x800A8C38 +
 * player: its cries, the KO echo too) and ends in 0x800885D8 (the SPU
 * voice, bank, program, tone, ...): when the caller is sound_play (return
 * 0x80075524) with such a key, that voice's pitch register is written 3
 * semitones up (its note argument does not move the pitch: measured). A
 * TTT1 guest's cries (and a native's on its DR voices) are the
 * Expanded's own PCM (src/tekken3_ttt1_voices.c), read from
 * <prefix>-voices.juv by fopen: our fopen (src/tekken2/tmst_tekken2_stages.c)
 * hands it a copy of that bank with every sample 3 semitones up (linear
 * interpolation, as the Expanded's B140 does), written beside the memory
 * card (t2-bighead-voices.juv). The bank follows the guest's generation,
 * bumped at each pick at the selector: the row's change counts from the
 * next pick on.
 *
 * Tekken 3: the fighter's drawing (0x8003AFCC, from 0x8003A818) takes each
 * bone in turn: its world matrix built in a scratch matrix (0x8003B5C0),
 * copied to the record (+0x8F4 + 68 bone: 3x3 s16, 4096 = 1, the place at
 * +0x14; what hits and cameras read), then 0x8003B71C(view, world, bone's
 * +0x20) makes the matrix the bone's mesh is drawn with; a second call
 * (a2 = the scratch itself) makes its light matrix. Here the world matrix's
 * columns (the bone's local axes) are scaled for the first call only and put
 * back at its return: the record, the light and the next bones never see it.
 * Tekken 3's bones come in Tekken 2's parts' order (measured: 2 the head,
 * 6 / 10 the hands, 14 / 17 the feet), so Tekken 2's table is taken bone for
 * bone; bone 18 (the hair, at the head's place) grows with the head.
 *
 * Bench: TMST_T2_BIGHEAD=1 / 2 takes the row as BIG / BIGGER (the fights'
 * rule kept); TMST_T2_BH_BONES="2:1.5;6:1.25,1.625,1.625" (bone:s or
 * bone:sx,sy,sz) the bones and their scales in place of the table;
 * TMST_T2_BH_DUMP=1 logs every bone's place of player 1 every 240
 * drawings, =2 each new screen / phase / mode a fighter is drawn in (the
 * census that set the rule: all on screen 8). */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "cpu_state.h"
#include "memcard.h"
#include "mod_plugins.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TMST_HAS_GAME_OPTIONS
#include "tekken3_game_options.h"
#endif

enum { RECORD = 0x800a9228, RECORD_STRIDE = 0x188c, RECORDS = 3, BONES = 0x8f4, BONE_BYTES = 68, BONE_COUNT = 22,
       PLACE = 0x14, VIEW = 0x20, SCREEN = 0x800ae204, MODE = 0x800afa88, OPTIONS = 5, FIGHT = 8 };
/* The modes (0x800AFA88; GAME OPTION's lamps the same bits): the Tournament
 * plays as a Team Battle. */
enum { ARCADE = 0, VS = 1, TEAM_BATTLE = 2, TIME_ATTACK = 3, SURVIVAL = 4, PRACTICE = 5, BALL = 7, FORCE = 8,
       ROW_MODES = 1u << ARCADE | 1u << VS | 1u << TEAM_BATTLE | 1u << TIME_ATTACK | 1u << SURVIVAL |
                   1u << PRACTICE | 1u << BALL | 1u << FORCE };
/* The voices: 3 semitones up (Tekken 2's note + 3). */
enum { SOUND_PLAY_RETURN = 0x80075524, OWN_BANK = 0xc000 };
enum { OFF, BIG, BIGGER, VALUES };
static const char *const labels[VALUES] = { "OFF", "BIG", "BIGGER" };
static const char *const words[VALUES] = { "off", "big", "bigger" };
#define ROW_NAME "TEKKEN 2 BIG HEAD MODE"

/* Tekken 2's table (0x800A0F10: x, y, z of each part, 16 = 1; 0 none), and
 * bone 18 as the head. */
static const uint8_t t2_table[BONE_COUNT][3] = {
    {0, 0, 0}, {16, 20, 16}, {24, 24, 24}, {0, 0, 0}, {16, 24, 24}, {16, 24, 24}, {20, 26, 26}, {0, 0, 0},
    {16, 24, 24}, {16, 24, 24}, {20, 26, 26}, {16, 20, 20}, {16, 20, 20}, {16, 20, 20}, {18, 16, 18},
    {16, 20, 20}, {16, 20, 20}, {18, 16, 18}, {24, 24, 24}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };

static double scales[VALUES][BONE_COUNT][3];   /* per size: 0 = the bone as it is */
static int part;                               /* the feature, at activation */
static int wanted = OFF, loaded;               /* GAME OPTION's row; t2-bighead.txt read */
static int bench = -1, dump;
static unsigned drawings;

int tmst_tekken2_on(void);                     /* src/tekken2/tmst_tekken2.c: the package's switch */

static void table(void) {
    for (int b = 0; b < BONE_COUNT; b++)
        for (int k = 0; k < 3; k++) {
            double s = t2_table[b][k] / 16.0;
            scales[BIG][b][k] = s;                 /* once, then twice: Tekken 2's two sizes */
            scales[BIGGER][b][k] = s * s;
        }
}

static int bench_on(void) {
    if (bench >= 0) return bench;
    const char *e = getenv("TMST_T2_BIGHEAD");
    bench = e && (*e == '1' || *e == '2') ? *e - '0' : 0;
    const char *v = getenv("TMST_T2_BH_BONES");
    if (v && *v) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", v);
        memset(scales, 0, sizeof scales);
        for (char *t = strtok(buf, ";"); t; t = strtok(NULL, ";")) {
            int b; double a, c, d;
            int n = sscanf(t, "%d:%lf,%lf,%lf", &b, &a, &c, &d);
            if (n < 2 || b < 0 || b >= BONE_COUNT) continue;
            for (int s = BIG; s < VALUES; s++) {
                scales[s][b][0] = a; scales[s][b][1] = n == 4 ? c : a; scales[s][b][2] = n == 4 ? d : a;
            }
        }
    }
    dump = (v = getenv("TMST_T2_BH_DUMP")) && (*v == '1' || *v == '2') ? *v - '0' : 0;
    if (bench) tmst_log("tekken2: Big Head Mode bench on (%s%s)", labels[bench], getenv("TMST_T2_BH_BONES") ? ", bones given" : "");
    return bench;
}

/* ---- the row, kept beside the memory card ---- */

static int setting_path(char *out, size_t size) {
    const char *card = NULL; out[0] = 0;
    if (memcard_debug_info(0, &card, NULL, NULL, NULL) || !card || !*card) return 0;
    const char *slash = strrchr(card, '/'), *bs = strrchr(card, '\\');
    if (bs && (!slash || bs > slash)) slash = bs;
    int n = slash ? (int)(slash - card) : 1;
    snprintf(out, size, "%.*s/t2-bighead.txt", n, slash ? card : ".");
    return 1;
}
static void save(void) {
    char path[4096];
    FILE *f = setting_path(path, sizeof path) ? fopen(path, "wb") : NULL;
    if (!f) return;
    fprintf(f, "bighead %s\n", words[wanted]);
    fclose(f);
}
/* Once, as soon as the memory card's place is known. */
static void load(void) {
    char path[4096];
    if (loaded || !setting_path(path, sizeof path)) return;
    loaded = 1;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char key[16], value[16], line[64];
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%15s %15s", key, value) == 2 && !strcmp(key, "bighead"))
            for (int v = 0; v < VALUES; v++) if (!strcmp(value, words[v])) wanted = v;
    fclose(f);
    if (wanted != OFF) tmst_log("tekken2: " ROW_NAME " is %s (t2-bighead.txt)", labels[wanted]);
}

/* GAME OPTION's row, added at the first visit of the Options screen, under
 * the Wireframe Mode's (the user's wish, 2026-11-15): the rows come in the
 * order they are added, so ours waits for that one. */
int tmst_t2wf_row_done(void) __attribute__((weak));
static void menu(void) {
#ifdef TMST_HAS_GAME_OPTIONS
    static int row = -2, shown;
    if (row == -2 && tmst_t2wf_row_done && !tmst_t2wf_row_done()) return;
    if (row == -2) {
        row = tekken3_game_options_add(ROW_NAME, labels, VALUES, (unsigned)wanted, OFF, ROW_MODES);
        shown = wanted;
    }
    if (row < 0) return;
    int v = tekken3_game_options_value(row);
    if (v >= 0 && v < VALUES && v != shown) {
        shown = wanted = v;
        save();
        tmst_log("tekken2: " ROW_NAME " set to %s", labels[v]);
    }
#endif
}

/* ---- which fights, which size ---- */

/* The size this frame: the part on, the row (or the bench) not OFF, one of
 * the row's modes, the fight's screen; else OFF. */
static int size_now(void) {
    if (!part || !tmst_tekken2_on()) return OFF;
    int s = bench_on() ? bench : wanted;
    if (s == OFF || psx_mod_read_word(SCREEN) != FIGHT) return OFF;
    uint32_t m = psx_mod_read_word(MODE);
    return m <= 31 && (ROW_MODES >> m & 1u) ? s : OFF;
}

/* The head's scale now (1: not grown), for the Wireframe Mode's camera
 * behind the head (src/tekken2/tmst_tekken2_wireframe.c): it keeps its
 * distance from a grown head. */
double tmst_t2bh_head_scale(void) {
    int size = size_now();
    return size == OFF || scales[size][2][0] <= 0 ? 1.0 : scales[size][2][0];
}

static int32_t s32(uint32_t a) { return (int32_t)psx_mod_read_word(a); }
static int16_t s16(uint32_t a) { return (int16_t)psx_mod_read_half(a); }
static int clamp16(double v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int)(v < 0 ? v - 0.5 : v + 0.5); }

static void scale_columns(uint32_t m, const double s[3]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            uint32_t a = m + 2u * (uint32_t)(r * 3 + c);
            psx_mod_write_half(a, (uint16_t)clamp16(s16(a) * s[c]));
        }
}

static void dump_bones(uint32_t record) {
    char line[1024]; int n = 0;
    n += snprintf(line + n, sizeof line - (size_t)n, "tekken2: bones P1");
    for (int b = 0; b < BONE_COUNT && n < (int)sizeof line - 40; b++) {
        uint32_t at = record + BONES + (uint32_t)b * BONE_BYTES + PLACE;
        n += snprintf(line + n, sizeof line - (size_t)n, " %d:%d,%d,%d", b, s32(at), s32(at + 4), s32(at + 8));
    }
    tmst_log("%s", line);
}

/* bench (TMST_T2_BH_DUMP=2): each new screen / phase / mode a fighter is
 * drawn in, once, with the size given there. */
static void census(int size) {
    static uint32_t seen[64]; static int count;
    uint32_t key = (psx_mod_read_word(SCREEN) & 0xffu) << 24 | (psx_mod_read_half(0x800ae224) & 0xffu) << 16 |
                   (psx_mod_read_word(MODE) & 0xffu) << 8 | (uint32_t)size;
    for (int i = 0; i < count; i++) if (seen[i] == key) return;
    if (count < 64) seen[count++] = key;
    tmst_log("tekken2: big head census: screen %u phase %u mode %u -> %s", key >> 24, key >> 16 & 0xffu,
             key >> 8 & 0xffu, labels[size]);
}

/* The bone a call's a2 is the view matrix of (a fighter record's +0x8F4 +
 * 68 b + 0x20; records 0..2: Tekken Force's enemies are 1 and 2), or -1;
 * *record set. */
static int bone_of(uint32_t a2, uint32_t *record) {
    for (uint32_t p = 0; p < RECORDS; p++) {
        uint32_t r = RECORD + p * RECORD_STRIDE, first = r + BONES + VIEW;
        if (a2 >= first && a2 < first + BONE_COUNT * BONE_BYTES && (a2 - first) % BONE_BYTES == 0) {
            *record = r;
            return (int)((a2 - first) / BONE_BYTES);
        }
    }
    return -1;
}

extern void __tmst_chain_func_8003B71C(CPUState *cpu);
#ifdef TMST_BRIDGE_func_8003B71C
extern void __real_func_8003B71C(CPUState *cpu);
void __tmst_chain_func_8003B71C(CPUState *cpu) { __real_func_8003B71C(cpu); }
#endif
void __wrap_func_8003B71C(CPUState *cpu) {
    static uint32_t ret, sp, world;
    static uint8_t kept[18];                   /* the 3x3 */
    static int changed;
    if (cpu->pc == 0 || cpu->pc == 0x8003b71cu) {
        changed = 0;
        uint32_t record = 0;
        int bone, size = size_now();
        if (dump == 2 && bone_of(cpu->gpr[6], &record) == 0) census(size);        if (size != OFF && (bone = bone_of(cpu->gpr[6], &record)) >= 0) {
            if (dump == 1 && record == RECORD && bone == 0 && ++drawings % 240 == 1) dump_bones(record);
            const double *s = scales[size][bone];
            if (s[0] > 0) {
                world = cpu->gpr[5];
                for (uint32_t i = 0; i < sizeof kept; i++) kept[i] = psx_mod_read_byte(world + i);
                scale_columns(world, s);
                changed = 1; ret = cpu->gpr[31]; sp = cpu->gpr[29];
            }
        }
    }
    __tmst_chain_func_8003B71C(cpu);
    if (!changed || cpu->pc != ret || cpu->gpr[29] != sp) return;
    changed = 0;
    for (uint32_t i = 0; i < sizeof kept; i++) psx_mod_write_byte(world + i, kept[i]);
}

/* ---- the voices ---- */

/* A native's own sounds (see the top). 0x800885D8's a0 is the SPU voice it
 * keys on (< 24); its note argument does not move the pitch (measured: the
 * key-on's pitch the same with the note + 3), so the voice is marked and
 * its pitch register (0x1F801C04 + 16 v) written 3 semitones up as the
 * driver writes it (spu_write below). The mark is taken at that voice's next
 * key-on (0x1F801D88 / 0x1F801D8A) not from such a call: music and the
 * other sounds keep their pitch. */
static uint32_t pending, raised_voices;                /* SPU voices, a bit each */
static const double SEMITONES_UP = 1.189207115002721;  /* 2^(3/12) */

/* Who called sound_play (its return address, kept at its sp + 0x34): only
 * the cries. The player's own bank holds the announcer's call of the
 * fighter's name too (0x800757A8: key 0xC640, sound_play back to 0x80075820;
 * heard at Practice's start: the user's report, 2026-11-15), left as it is.
 * The cries: the voice dispatcher 0x80040C90 (back to 0x80040D40) and the KO
 * cry's two echoes of the task 0x800744E0 (0x80074594, 0x80074604: back to
 * 0x800745DC, 0x8007464C). */
static int cry_caller(uint32_t back) {
    return back == 0x80040d40u || back == 0x800745dcu || back == 0x8007464cu;
}

extern void __tmst_chain_func_800885D8(CPUState *cpu);
#ifdef TMST_BRIDGE_func_800885D8
extern void __real_func_800885D8(CPUState *cpu);
void __tmst_chain_func_800885D8(CPUState *cpu) { __real_func_800885D8(cpu); }
#endif
void __wrap_func_800885D8(CPUState *cpu) {
    uint32_t v = cpu->gpr[4] & 0xffffu;
    if ((cpu->pc == 0 || cpu->pc == 0x800885d8u) && v < 24) {
        int own = cpu->gpr[31] == SOUND_PLAY_RETURN && (cpu->gpr[19] & OWN_BANK) == OWN_BANK && size_now() != OFF &&
                  cry_caller(psx_mod_read_word(cpu->gpr[29] + 0x34));
        if (own) pending |= 1u << v; else pending &= ~(1u << v);
        static unsigned shown;                                /* bench: the first few */
        if (own && dump == 2 && shown < 60) { shown++; tmst_log("tekken2: big head voice: key %04X on SPU voice %u, sound_play from %08X", cpu->gpr[19] & 0xffffu, v, psx_mod_read_word(cpu->gpr[29] + 0x34)); }
    }
    __tmst_chain_func_800885D8(cpu);
}

void __real_spu_write(uint32_t addr, uint32_t value);
static uint16_t raw_pitch[24];                         /* the driver's own value, per voice */

/* A raised voice taken by another sound (the announcer, on the voice a cry
 * had: the user's report, 2026-11-15): its pitch back to the driver's own
 * value, before that sound's key-on, whatever order the registers come in. */
static void lower(uint32_t keys) {
    uint32_t drop = raised_voices & keys & ~pending;
    for (uint32_t v = 0; drop; v++, drop >>= 1)
        if (drop & 1u) __real_spu_write(0x1f801c04u + 16u * v, raw_pitch[v]);
    raised_voices &= ~(keys & ~pending);
}

void __wrap_spu_write(uint32_t addr, uint32_t value) {
    if (addr >= 0x1f801c00u && addr < 0x1f801c00u + 24 * 16) {
        uint32_t v = (addr - 0x1f801c00u) >> 4, reg = addr & 15u;
        if (reg == 4u) {                                         /* the pitch */
            raw_pitch[v] = (uint16_t)value;
            if ((pending | raised_voices) >> v & 1u) {
                double p = (value & 0xffffu) * SEMITONES_UP;
                value = (value & 0xffff0000u) | (uint32_t)(p > 0x3fff ? 0x3fff : p + 0.5);
            }
        } else if (reg == 6u) {                                  /* a sample's start: another sound, unless ours */
            lower(1u << v);
        }
    }
    if (addr == 0x1f801d88u || addr == 0x1f801d8au) {            /* key-on: voices 0..15, 16..23 */
        uint32_t keys = (value & 0xffffu) << (addr == 0x1f801d8au ? 16 : 0);
        lower(keys);
        raised_voices |= keys & pending;
        pending &= ~keys;
    }
    __real_spu_write(addr, value);
}

/* The guests' banks: the row (or the bench) not OFF, in one of its modes
 * (the bank is read at the first cry, in the fight). */
static int voices_on(void) {
    if (!part || !tmst_tekken2_on() || (bench_on() ? bench : wanted) == OFF) return 0;
    uint32_t m = psx_mod_read_word(MODE);
    return m <= 31 && (ROW_MODES >> m & 1u);
}

static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

FILE *__tmst_chain_fopen(const char *path, const char *mode);   /* src/tekken2/tmst_tekken2_stages.c */

/* The bank (JUV1: 'JUV1', 1, size, count, 44100, pool, odds, -; then count
 * entries of id, offset, frames, category; then the samples' 16-bit PCM in
 * order) with each sample 3 semitones up; NULL when it is not one. */
static uint8_t *raised(const uint8_t *in, long size, long *out_size) {
    if (size < 32 || le32(in) != 0x3156554au || le32(in + 4) != 1 || le32(in + 8) != (uint32_t)size) return NULL;
    uint32_t count = le32(in + 12);
    if (count < 1 || count > 64 || 32u + count * 16u > (uint32_t)size) return NULL;
    const double step = 1.189207115002721;                    /* 2^(3/12) */
    uint32_t total = 32 + count * 16;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t off = le32(in + 32 + i * 16 + 4), frames = le32(in + 32 + i * 16 + 8);
        if (!frames || off > (uint32_t)size || frames > ((uint32_t)size - off) / 2) return NULL;
        total += 2 * ((uint32_t)((frames - 1) / step) + 1);
    }
    uint8_t *out = malloc(total);
    if (!out) return NULL;
    memcpy(out, in, 32 + count * 16);
    put32(out + 8, total);
    uint32_t at = 32 + count * 16;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *e = out + 32 + i * 16;
        const uint8_t *pcm = in + le32(e + 4);
        uint32_t frames = le32(e + 8), n = (uint32_t)((frames - 1) / step) + 1;
        for (uint32_t k = 0; k < n; k++) {
            double x = k * step; uint32_t i0 = (uint32_t)x; double f = x - i0;
            int a = (int16_t)(pcm[2 * i0] | pcm[2 * i0 + 1] << 8);
            int b = i0 + 1 < frames ? (int16_t)(pcm[2 * i0 + 2] | pcm[2 * i0 + 3] << 8) : 0;
            int v = (int)(a + (b - a) * f);
            out[at + 2 * k] = (uint8_t)v; out[at + 2 * k + 1] = (uint8_t)(v >> 8);
        }
        put32(e + 4, at); put32(e + 8, n);
        at += 2 * n;
    }
    *out_size = (long)total;
    return out;
}

/* src/tekken2/tmst_tekken2_stages.c's fopen: a voice bank read while the
 * mode is on gets the raised copy; NULL: the file as it is. */
FILE *tmst_t2bh_fopen(const char *path, const char *mode) {
    static const char tail[] = "-voices.juv";
    size_t n = path ? strlen(path) : 0;
    if (!mode || mode[0] != 'r' || n < sizeof tail - 1 || strcmp(path + n - (sizeof tail - 1), tail) || !voices_on())
        return NULL;
    FILE *f = __tmst_chain_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f), out_size = 0;
    fseek(f, 0, SEEK_SET);
    uint8_t *in = size > 0 && size < 16L * 1024 * 1024 ? malloc((size_t)size) : NULL;
    int ok = in && fread(in, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    uint8_t *out = ok ? raised(in, size, &out_size) : NULL;
    free(in);
    char to[4096];
    const char *card = NULL;
    if (out && !memcard_debug_info(0, &card, NULL, NULL, NULL) && card && *card) {
        const char *slash = strrchr(card, '/'), *bs = strrchr(card, '\\');
        if (bs && (!slash || bs > slash)) slash = bs;
        snprintf(to, sizeof to, "%.*s/t2-bighead-voices.juv", slash ? (int)(slash - card) : 1, slash ? card : ".");
        FILE *w = __tmst_chain_fopen(to, "wb");
        ok = w && fwrite(out, 1, (size_t)out_size, w) == (size_t)out_size;
        if (w) fclose(w);
    } else ok = 0;
    free(out);
    if (!ok) { tmst_log("tekken2: Big Head Mode: %s left as it is (not a voice bank we know)", path); return NULL; }
    tmst_log("tekken2: Big Head Mode: %s 3 semitones up", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    return __tmst_chain_fopen(to, mode);
}

static void tick(void) {
    if (!part || !psx_mod_game_started()) return;
    load();
    if (psx_mod_read_word(SCREEN) == OPTIONS && tmst_tekken2_on()) menu();
}

static void activate(void) {
    table();
    part = tmst_guard("tmst.tekken2.bighead", TMST_NEED_ACTIVATION | TMST_NEED_VBLANK | TMST_NEED_GAME_OPTIONS);
    if (part) tmst_log("tekken2: Big Head Mode ready (GAME OPTION's " ROW_NAME " row)");
    (void)bench_on();
}

PSX_MOD_CONSTRUCTOR(tmst_register_tekken2_bighead) {
    (void)psx_mod_register_activation_plugin("tmst.tekken2.bighead", activate);
    (void)psx_mod_register_vblank_plugin("tmst.tekken2.bighead", tick);
}
