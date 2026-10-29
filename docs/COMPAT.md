# Compatibility of the TMST add-on with Tekken 3 Expanded

## Tested with

| Expanded | Add-on | Build | In game | Date | Notes |
|---|---|---|---|---|---|
| v1.4.1 (`ff6b0c1`) | 0.1.0 (P2) | OK (build-release + worktree) | headless: noop off = no TMST line, on = 3 lines; launcher: pending | 2026-10-29 | skeleton + `tmst.dev.noop` |
| main (`ff6b0c1`, = v1.4.1 today) | 0.1.0 (P2) | OK (worktree) | – | 2026-10-29 | |

`check_upstream.cmd` builds the add-on against the refs given and checks the surfaces below.

## What the add-on relies on

**Stable** (do not change between releases):
- retail guest addresses of SLUS-00402;
- the package manifest format;
- the plugin registration calls (`psx_mod_register_*`, `PSX_MOD_CONSTRUCTOR`).

**Not stable.** Only `src/compat/`, `dev/` and `tools/check_expanded.py` may know these:

| Surface | Used by | Checked by |
|---|---|---|
| target `psx-runtime`, `game.toml` at the root, `mods/packages/<id>/<ver>/` staging | `dev/tmst_inject.cmake` | `check_expanded.py` |
| `tekken3_wrap_symbol()` (macOS emulation of `--wrap`) | `dev/tmst_inject.cmake` | `check_expanded.py` |
| `mod_plugins.h` function names | `src/compat/tmst_features.cmake` → `TMST_HAS_*` | build |
| `tekken3_game_options_add()` | `src/compat/` (Jukebox, T2 option) | build |
| setup internals: `tools()`, `kept_mods`, `build_game()` arguments | `dev/build.py` | `check_expanded.py` |
| `git describe` of the Expanded folder | version guard (`tmst_guard`) | – |
| symbols the Expanded wraps itself (we must not wrap them again) | `addon.cmake` `TMST_WRAPS` | build (duplicate `__wrap_` = link error) |

## Known limits of the dev shim

- **macOS:** the Expanded applies its emulated wraps (`tekken3_apply_wraps()`) before our deferred setup, so
  add-on wraps are not applied there yet. Fine while `TMST_WRAPS` is empty; to be solved with the hook (API_GAPS G11).
- **Cached include:** `CMAKE_PROJECT_INCLUDE` stays in `build-release/CMakeCache.txt`, so the Expanded's own
  Update & play keeps building the add-on in. `dev\build.cmd --without` clears it.
- **Other refs:** `dev\build.cmd <ref>` links the current `generated/` (made by the installed release's codegen)
  into the ref's worktree. A ref whose code generator changed needs its own setup run.
