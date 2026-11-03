# TMST add-on for Tekken 3 Expanded: what the game's build takes from this
# folder. Declarative only (lists, no commands), so that the Expanded's own
# add-on hook can read the same file once it exists; until then
# dev/tmst_inject.cmake does. Paths are relative to this folder.

set(TMST_ID "tmst")
set(TMST_VERSION "0.1.0")
# Expanded releases this add-on was tested with (docs/COMPAT.md). Another
# release still builds and runs: the add-on detects features, not versions,
# and only notes the release as untested in the log.
set(TMST_TESTED_WITH "v1.4.1")

# C sources linked into the game (psx-runtime).
set(TMST_SOURCES
    src/core/tmst_core.c
    src/core/tmst_font.c
    src/core/tmst_overlay.c
    src/core/tmst_audio.c
    src/core/tmst_music.c
    src/core/tmst_menu.c
    src/core/tmst_input.c
    src/core/tmst_spu.c
    src/core/tmst_pause.c
    src/core/tmst_camera.c
    src/core/tmst_png.c
    src/core/tmst_files.c
    src/compat/tmst_compat.c
    src/jukebox/tmst_jukebox.c
    src/photo/tmst_photo.c
    src/photo/tmst_photo_fx.c
    src/tournament/tmst_tournament_pools.c
    src/tournament/tmst_bracket.c
    src/tournament/tmst_tournament_progress.c
    src/tournament/tmst_tournament_art.c
    src/tournament/tmst_tournament_screens.c
    src/tournament/tmst_tournament.c
    src/tekken2/tmst_tekken2.c
)
set(TMST_INCLUDE_DIRS
    src/core
    src/compat
    src/tournament
)

# Symbols wrapped with -Wl,--wrap, each "symbol|file defining __wrap_symbol".
# Only symbols the Expanded does not wrap itself (docs/COMPAT.md).
set(TMST_WRAPS
    "gpu_ws_prepass_linked_list|src/core/tmst_overlay.c"
    "psx_sdl_audio_open|src/core/tmst_audio.c"
)

# Symbols the Expanded may wrap itself (guest routines and runtime
# functions), each "symbol|file defining __wrap_symbol": ours runs first and
# calls __tmst_chain_symbol (theirs). Wrapped in the build: the build renames
# their wrapper (dev/tmst_chain.py). Not wrapped there (another release, or
# TTT1 Characters off): we wrap it, and the file defines __tmst_chain_ under
# TMST_BRIDGE_symbol as a call of __real_symbol.
set(TMST_CHAINS
    "func_8006AE14|src/core/tmst_music.c"
    "func_80028CD4|src/core/tmst_menu.c"
    "tekken3_outfits_input|src/core/tmst_input.c"
    "spu_render|src/core/tmst_spu.c"
    "spu_set_host_mix_volume|src/core/tmst_spu.c"
    "func_800780BC|src/core/tmst_pause.c"
    "func_80048240|src/core/tmst_camera.c"
    "func_8004A6BC|src/core/tmst_camera.c"
    "func_8003A818|src/core/tmst_camera.c"
    "func_8004F0E4|src/tournament/tmst_tournament.c"
    "func_8004439C|src/tournament/tmst_tournament.c"
)

# Packages staged into the game's mods/packages (mods/<id>/<version>/manifest.toml).
set(TMST_PACKAGES
    tmst.jukebox
    tmst.photo-mode
    tmst.tournament
    tmst.tekken2
)

# Python steps run after the Expanded's setup (setup/tmst_setup.py).
set(TMST_SETUP setup/tmst_setup.py)
