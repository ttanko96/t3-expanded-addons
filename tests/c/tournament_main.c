/* tests/test_tournament.py: the Tournament Mode's data side, driven from the command line.
 *   pools                               each tournament: "id|boss|key key ..." ("*" = whole roster, "-" = drawn boss)
 *   bracket <seed> <player> <boss> <n> <results>
 *                                       roster 0..n-1; results: W / L per player match, then the rest finished.
 *                                       Prints "slots ...", "step <mask> <winners...>" per step, "out|champion ..."
 *   progress <path> load|win:<t>        prints "won <mask> open <t...> [opens <t>]" */
#include "tmst_bracket.h"
#include "tmst_tournament_pools.h"
#include "tmst_tournament_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void winners(const TmstBracket *b) {
    for (int m = 0; m < TMST_BRACKET_MATCHES; m++) printf(" %d", b->winner[m]);
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "pools")) {
        for (int t = 0; t < TMST_TOURNEYS; t++) {
            const TmstTourney *d = tmst_tourney(t);
            printf("%s|%s%s%s|", d->id, d->boss ? d->boss : "-", d->boss2 ? "+" : "", d->boss2 ? d->boss2 : "");
            if (!d->keys) printf("*");
            for (const char *const *k = d->out; k && *k; k++) printf(" -%s", *k);
            for (const char *const *k = d->keys; k && *k; k++) printf("%s%s", k == d->keys ? "" : " ", *k);
            printf("\n");
        }
        return 0;
    }
    if (argc == 7 && !strcmp(argv[1], "bracket")) {
        int n = atoi(argv[5]), pool[256];
        for (int i = 0; i < n && i < 256; i++) pool[i] = i;
        TmstBracket b;
        if (!tmst_bracket_new(&b, atoi(argv[3]), atoi(argv[4]), pool, n, (uint32_t)strtoul(argv[2], NULL, 10))) {
            printf("refused\n");
            return 0;
        }
        printf("slots");
        for (int s = 0; s < TMST_BRACKET_SLOTS; s++) printf(" %d", b.slot[s]);
        printf("\n");
        for (const char *r = argv[6]; *r && tmst_bracket_player_match(&b) >= 0; r++) {
            int m = tmst_bracket_player_match(&b);
            printf("match %d vs %d\n", m, tmst_bracket_opponent(&b));
            printf("step %u", tmst_bracket_player_result(&b, *r == 'W'));
            winners(&b);
        }
        printf("finish %u", tmst_bracket_finish(&b));
        winners(&b);
        printf("%s %d lost_at %d\n", tmst_bracket_champion(&b) == b.player ? "champion" : "out",
               tmst_bracket_champion(&b), tmst_bracket_lost_at(&b, b.player));
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "progress")) {
        TmstProgress p;
        tmst_progress_load(&p, argv[2]);
        int opens = -2;
        if (!strncmp(argv[3], "win:", 4)) {
            opens = tmst_progress_win(&p, atoi(argv[3] + 4));
            if (!tmst_progress_save(&p, argv[2])) return 1;
        }
        printf("won %u open", p.won);
        for (int t = 0; t < TMST_TOURNEYS; t++) if (tmst_progress_open(&p, t)) printf(" %d", t);
        if (opens != -2) printf(" opens %d", opens);
        printf("\n");
        return 0;
    }
    return 2;
}
