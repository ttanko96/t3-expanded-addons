/* TMST core: the music call, chained (tmst_music.h). addon.cmake lists
 * func_8006AE14 in TMST_CHAINS: the build renames the Expanded's
 * __wrap_func_8006AE14 to __tmst_chain_func_8006AE14 (dev/tmst_chain.py),
 * or, on a release that does not wrap it, makes that name call the game's
 * own routine. Either way ours runs first and theirs, then the game's, after. */
#include "tmst_music.h"
#include "cpu_state.h"
#include <stdint.h>

static TmstMusicHook hook;

void tmst_music_set_hook(TmstMusicHook h) { hook = h; }

extern void __tmst_chain_func_8006AE14(CPUState *cpu);
void __wrap_func_8006AE14(CPUState *cpu) {
    /* The routine is entered by its call (pc 0) or by the recompiled code at
     * its own address; re-entries at inner pcs go straight on. */
    if (hook && (cpu->pc == 0 || cpu->pc == 0x8006ae14u)) {
        int id = (int)(int16_t)cpu->gpr[4], to = hook(id);
        if (to < 0) { cpu->pc = cpu->gpr[31]; return; }
        cpu->gpr[4] = (uint32_t)to;
    }
    __tmst_chain_func_8006AE14(cpu);
}
