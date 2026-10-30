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
| chained symbols: the Expanded's `__wrap_<sym>` renamed in its compiled object before the link | `addon.cmake` `TMST_CHAINS`, `dev/tmst_chain.py` | build (missing object = error naming the symbol); wrapped or not is read from psx-runtime's link options at configure time; not wrapped (another release, TTT1 Characters off): we wrap it and our file's `TMST_BRIDGE_<sym>` part calls `__real_<sym>` |
| runtime functions we wrap: `gpu_ws_prepass_linked_list` (start of each GPU list DMA), `psx_sdl_audio_open` (audio output) | `src/core/tmst_overlay.c`, `tmst_audio.c` | build (link error if renamed) |
| upper VRAM: `psx_mod_gpu_high_vram_*`, packet memory as trusted source; area x 0..511, y 768..1023 | `src/core/tmst_overlay.c` | `TMST_HAS_HIGH_VRAM_UPLOAD` |
| `tekken3_stage_high_vram_used/generation()` (weak) | `src/compat/tmst_compat.c` | absent = nothing taken |
| guest: 0x8006AE14 = play music (a0 = id) | `src/core/tmst_music.c` | retail, stable |
| main menu: the working list's address in the menu's LUI/ORI (0x800DB5D0/D8, patched by the Expanded), its layout (count +4, rows of 8 from +16), the Expanded's menu code in its `__wrap_func_80028CD4` (seen only with its own rows: ours are in the list in phase 1 only) | `src/core/tmst_menu.c` | retail + the Expanded's patch, checked by the menu entry's first word |
| `tekken3_outfits_input` (every pad read, main.cpp), chained | `src/core/tmst_input.c` | build |
| `spu_render`, `spu_set_host_mix_volume` (the Expanded's TTT1 voices wrap both), chained; `__real_spu_set_host_mix_volume` to mute the game's music | `src/core/tmst_spu.c` | build |
| `gpu_high_vram_pixels()` (weak): borrowing upper VRAM on menus | `src/compat/tmst_compat.c` | absent = no borrowing |
| `tekken3_guest_key()`, `tekken3_ttt1_grid_character()` (weak), `mods/ttt1/<Key>-T3-label.txt` | `src/jukebox/tmst_jukebox.c` | absent = stock fighters only |
| OpenGL samples the upper half only if it exists before the context: `tmst_overlay_prepare()` at every drawing mod's activation | `src/core/tmst_overlay.c` | – |
| the selector's title font in VRAM: page (832,0) 4 bpp, palette (336,505), 14 x 24 cells from u 172 (retail) | Jukebox banner and category line | retail, stable |
| runtime disc reader `iso_open`/`iso_read_*` (psxrecomp), `disc.cfg` beside the game, `disc/SLUS_004.02` (setup's copy, for the title lists) | Jukebox | link / file present |
| `memcard_debug_info()`: `jukebox.txt` beside the memory card | Jukebox | link |
| the recompiler's function entries: `cpu->pc` 0 = a function's first entry, otherwise the address it goes on at after a call it made; a return sets `cpu->pc` to `ra` | `src/core/tmst_pause.c` (`func_800780BC` chained: our row; held = not run) | in game (Photo Mode: row, choice, hold) |
| pause menu: labels at 0x80098A44, its one mode read at 0x80078164 (entered at 0x8007815C), choice 0x80098A41, cursor 0x80098A40, delay 0x800A8672; paused 0x80095494, mode 0x800AFA88 | `src/core/tmst_pause.c` | retail, stable |
| the ordering table: cleared in reverse, 1024 ranks, the list starting at rank 1023 | `src/core/tmst_overlay.c` (`tmst_overlay_rank`) | – |
| the core's font sheet in the upper VRAM half (192 x 9, placed where `tekken3_stage_high_vram_used` leaves room; the Vulkan renderer does not sample the half) | `src/core/tmst_overlay.c` (`tmst_prim_text`) | `TMST_HAS_HIGH_VRAM_UPLOAD` |
| `psx_mod_widescreen_x_margin()` (weak) | `src/compat/tmst_compat.c` | absent = no margin |
| `tekken3_hit_effect_high_vram_used()`, `tekken3_ttt1_wolf_high_vram_used()` (weak): TTT Hitspark's and the wolf's cells in the upper half, kept clear of our font and pictures | `src/compat/tmst_compat.c` (`tmst_compat_high_vram_taken`) | absent = only the stage's |
| `tekken3_arcade_stages_t3_stage()` (weak) | `src/compat/tmst_compat.c` | absent = the PS1 floor's size |
| the runtime's display readout (weak): `gpu_get_display_info`, `gr_scale`, `gr_read_display_hires`, `gr_render_display`, `gr_wide_supported`, `gr_render_wide_display`, `ws_nw_extra` (also the visible side for the frames: `psx_mod_widescreen_x_margin` includes the culling's guard), `gl_renderer_sync_cpu`, `gr_backend` (as the debug server's `screenshot_hires` / `wide_shot`) | `src/compat/tmst_compat.c` (`tmst_compat_read_frame`) | absent = no photos, no exact live preview |
| the paused frame's whole-screen way (0x800954A4 set; the view at 0x800A85C0: pitch >> 6, yaw >> 6, eye at +20/+24/+28; GTE H = control register 26), the HUD routine 0x8003D6B0 called from the continuation 0x8002B650 before 0x8004A6BC, a fighter's drawing 0x8003A818 skipped behind the eye | `src/core/tmst_camera.c` (chains: `func_80048240` (the Expanded wraps it), `func_8004A6BC`, `func_8003A818`) | in game (Photo Mode: Arcade, Practice) |
| camera slot 0 at 0x800A02A8 (eye, pitch, yaw, H), the fighters at 0x800ADCA8 (x, y, z, 16 bytes each), the PS1 floor's grid (tile 0x8009E780, tiles across 0x8009E824 / 0x8009E828) | `src/core/tmst_camera.c` | retail, stable |

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
