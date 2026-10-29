/* tmst.dev.noop: proves the add-on is in the game. It changes nothing; it
 * only writes to the log when its feature is on: once at activation, once
 * after ten seconds of VBlanks. Removed before the first release. */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "mod_plugins.h"

static int on;
static unsigned frames;

static void activate(void) {
    on = tmst_guard("tmst.dev.noop", TMST_NEED_ACTIVATION | TMST_NEED_VBLANK);
    if (on) tmst_log("tmst.dev.noop is active");
}

static void tick(void) {
    if (on && ++frames == 600) tmst_log("tmst.dev.noop: 600 VBlanks seen");
}

PSX_MOD_CONSTRUCTOR(tmst_register_noop) {
    (void)psx_mod_register_activation_plugin("tmst.dev.noop", activate);
    (void)psx_mod_register_vblank_plugin("tmst.dev.noop", tick);
}
