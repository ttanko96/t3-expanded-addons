/* tmst.tekken1: Tekken 1 Content, what the add-on takes from the player's own
 * Tekken PlayStation disc (the add-on's setup: setup/tmst_setup.py), in one
 * package. Its feature "tekken1" is the package's switch; each part is a
 * feature of its own that requires it, shown in the Mods tab's next group,
 * TMST / Tekken 1 Content.
 *
 * Parts:
 *   Stages Tekken 1's 11 stages: the pack handed to the Expanded's stage
 *          engine, the fight and its choice on the STAGE line (TEKKEN 1)
 *          are src/tekken2/tmst_tekken2_stages.c's (Tekken 2's machinery,
 *          shared); the sky, the layers and the effects
 *          src/tekken1/tmst_tekken1_stages.c's. */
#include "tmst_core.h"
#include "tmst_compat.h"
#include "mod_plugins.h"

static int on;                           /* the feature, at activation */

int tmst_tekken1_on(void) { return on; }

static void activate(void) {
    on = tmst_guard("tmst.tekken1", TMST_NEED_ACTIVATION);
}

PSX_MOD_CONSTRUCTOR(tmst_register_tekken1) {
    (void)psx_mod_register_activation_plugin("tmst.tekken1", activate);
}
