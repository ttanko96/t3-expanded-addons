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
    src/compat/tmst_compat.c
    src/dev/tmst_noop.c
)
set(TMST_INCLUDE_DIRS
    src/core
    src/compat
)

# Symbols wrapped with -Wl,--wrap, each "symbol|file defining __wrap_symbol".
# Only symbols the Expanded does not wrap itself (docs/COMPAT.md).
set(TMST_WRAPS
)

# Packages staged into the game's mods/packages (mods/<id>/<version>/manifest.toml).
set(TMST_PACKAGES
    tmst.dev.noop
)

# Python steps run after the Expanded's setup (setup/tmst_setup.py).
set(TMST_SETUP setup/tmst_setup.py)
