/* TMST compat (tmst_compat.h): the features found at build time. */
#include "tmst_compat.h"
#include <stdio.h>
#include <string.h>

unsigned tmst_compat_available(void) {
    unsigned m = 0;
#ifdef TMST_HAS_ACTIVATION_PLUGIN
    m |= TMST_NEED_ACTIVATION;
#endif
#ifdef TMST_HAS_VBLANK_PLUGIN
    m |= TMST_NEED_VBLANK;
#endif
#ifdef TMST_HAS_GUEST_MEMORY
    m |= TMST_NEED_GUEST_MEMORY;
#endif
#ifdef TMST_HAS_HIGH_VRAM_UPLOAD
    m |= TMST_NEED_HIGH_VRAM;
#endif
#ifdef TMST_HAS_GAME_OPTIONS
    m |= TMST_NEED_GAME_OPTIONS;
#endif
    return m;
}

const char *tmst_compat_describe(unsigned mask) {
    static const char *const names[] = { "activation plugins", "VBlank plugins", "guest memory",
                                         "upper VRAM uploads", "GAME OPTION rows" };
    static char out[160];
    out[0] = 0;
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (!(mask >> i & 1)) continue;
        if (out[0]) strncat(out, ", ", sizeof out - strlen(out) - 1);
        strncat(out, names[i], sizeof out - strlen(out) - 1);
    }
    return out[0] ? out : "nothing";
}
