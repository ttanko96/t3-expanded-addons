# Compatibility of the TMST add-on with Tekken 3 Expanded

## Tested with

| Expanded | Add-on | Build | In game | Date | Notes |
|---|---|---|---|---|---|
| v1.4.1 (`ff6b0c1`) | 0.1.0 (P2) | OK (build-release + worktree) | headless: noop off = no TMST line, on = 3 lines; launcher: pending | 2026-10-29 | skeleton + `tmst.dev.noop` |
| main (`ff6b0c1`, = v1.4.1 today) | 0.1.0 (P2) | OK (worktree) | – | 2026-10-29 | |
| v1.4.1 + main | 0.1.0 (P3) | OK (build-release, debug, both worktrees; chain renamed in each) | debug headless: noop on = label drawn over a fight, music 1 and 8 seen through the chain; off = no TMST line, upper VRAM never enabled | 2026-10-29 | core |

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
| chained symbols: the Expanded's `__wrap_<sym>` renamed in its compiled object before the link | `addon.cmake` `TMST_CHAINS`, `dev/tmst_chain.py` | build (missing object = error naming the symbol); a release that stops wrapping one: we wrap it, `tmst_chain_bridge.c` |
| runtime functions we wrap: `gpu_ws_prepass_linked_list` (start of each GPU list DMA), `psx_sdl_audio_open` (audio output) | `src/core/tmst_overlay.c`, `tmst_audio.c` | build (link error if renamed) |
| upper VRAM: `psx_mod_gpu_high_vram_*`, packet memory as trusted source; area x 0..511, y 768..1023 | `src/core/tmst_overlay.c` | `TMST_HAS_HIGH_VRAM_UPLOAD` |
| `tekken3_stage_high_vram_used/generation()` (weak) | `src/compat/tmst_compat.c` | absent = nothing taken |
| guest: 0x8006AE14 = play music (a0 = id) | `src/core/tmst_music.c` | retail, stable |

## Known limits of the dev shim

- **macOS:** the Expanded applies its emulated wraps (`tekken3_apply_wraps()`) before our deferred setup, so
  add-on wraps are not applied there yet. Fine while `TMST_WRAPS` is empty; to be solved with the hook (API_GAPS G11).
- **Cached include:** `CMAKE_PROJECT_INCLUDE` stays in `build-release/CMakeCache.txt`, so the Expanded's own
  Update & play keeps building the add-on in. `dev\build.cmd --without` clears it.
- **Chains and `--without`:** renamed objects are listed in `<build>/tmst_chain_state.json`; `--without` (or a
  chain dropped from `addon.cmake`) deletes them so that Ninja compiles them again as the Expanded wrote them.
- **Removing the add-on folder:** run `dev\build.cmd --without` first. Otherwise the cached
  `CMAKE_PROJECT_INCLUDE` points at a missing file and the Expanded's Update & play cannot configure.
- **Other refs:** `dev\build.cmd <ref>` links the current `generated/` (made by the installed release's codegen)
  into the ref's worktree. A ref whose code generator changed needs its own setup run.
