/* TMST core: what every TMST mod shares. Not a mod of its own (no package,
 * no plugin): a library linked with the mods. */
#ifndef TMST_CORE_H
#define TMST_CORE_H
#ifdef __cplusplus
extern "C" {
#endif

/* "TMST: " + the message on stderr (the game's log). */
void tmst_log(const char *fmt, ...);

/* The Expanded this build was made from: `git describe` of its folder
 * ("v1.4.1", "v1.4.1-3-gabcdef0"), "unknown" without git. */
const char *tmst_upstream_rev(void);
/* Nonzero when that release is in addon.cmake's TMST_TESTED_WITH. */
int tmst_upstream_tested(void);

/* The version guard, called by a mod at its activation: logs the add-on and
 * the Expanded once, then whether `mod` can run. `needs` is a mask of
 * TMST_NEED_* (src/compat/tmst_compat.h); a need the Expanded cannot meet
 * switches the mod off (logged, never a crash). Returns 1 to go on. */
int tmst_guard(const char *mod, unsigned needs);

#ifdef __cplusplus
}
#endif
#endif
