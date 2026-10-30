# TMST add-on for Tekken 3 Expanded

New features for [Tekken 3 Expanded](https://github.com/omarma/tekken3-expanded), packaged as mods that show up in the game's own **Mods**
tab, in the **TMST** group. Each mod can be switched on and off there; nothing in the Expanded's own files is changed.

No game data is included: everything a mod needs from the game is made on your machine by the add-on's setup.

## Features (currently)

### Jukebox
Choose the music of every fighter.
- **Open:** the last row of the main menu, **JUKEBOX**.
- **Grid:** ✕ a fighter's music list · L2/R2 switch the character panel (with TTT1 characters) · □ reset every
  fighter to their own music · ○ exit (asks to save changes).
- **List:** L2/R2 category · △ preview / stop · ✕ give the track to the fighter · □ reset this fighter · ○ back.
- In a fight the fighters' chosen music plays (P2's first), whatever stage is picked.
- The choices are saved beside the memory card (`saves\jukebox.txt`). The track titles are plain text lists in
  `addons\tmst\workspace\jukebox\` (`file = title` per line; `--disable` in a title leaves the track out).

### Photo Mode
A free camera in any paused fight, and photos of it.
- **Open:** pause a fight and choose **PHOTO** (Arcade, VS, Team Battle, Time Attack, Survival). In Practice, press
  **SELECT** while paused. Not in Tekken Force or Tekken Ball.
- **Camera:** D-pad move · L1/R1 down/up · □ + D-pad turn and look · L2/R2 zoom · SELECT reset the camera. The camera
  stops at the edge of the stage.
- **Panel (START):** ZOOM, FOCUS and BLUR (depth of field: the fighters sharp, or a sharp band), FILTER (14: black &
  white, sepia, noir, vintage, high contrast, cold, warm, teal & orange, duotone, poster, night vision, scanlines,
  negative), FRAME (8: letterbox, polaroid, film strip, vignette, Tekken, CRT TV, comic), RESET.
- ✕ take a photo · △ hide the HUD · ○ back to the pause menu, the fight exactly as it was.
- Photos are saved as PNG at the renderer's resolution (16:9 with its sides) in `addons\tmst\screenshots\`
  (`photo_YYYYMMDD_HHMMSS.png`).
- Filters that need every pixel's colour (black & white, sepia, negative ...) and, in 16:9, the blur on the screen's
  sides are shown exactly once the camera stands still; while it moves, a close look.
- Best with the OpenGL renderer. With Vulkan the panel's text and the exact preview are not shown.

## Installation (currently)

The Expanded's own add-on support is still being made. Until then the add-on is built into the game by its own
developer build script; once that support exists, the Expanded's **Update & play** will be enough.

You need Windows 10/11, a Tekken 3 Expanded that has been set up once (the game starts; `build-release\` and
`.setup\venv\` exist in its folder), access to this repository and about 300 MB of free space. Below,
`C:\Games\tekken3-expanded` is the Expanded's folder: change it if yours is elsewhere.

1. **Close the game.** A running game cannot be rebuilt.
2. **Put the add-on in place**, exactly at `C:\Games\tekken3-expanded\addons\tmst\`:
   ```bash
   git clone https://github.com/ttanko96/t3-expanded-addons.git "C:\Games\tekken3-expanded\addons\tmst"
   ```
   (Or download the ZIP, rename the folder to `tmst` and put it in `addons\`; `addons\tmst\addon.cmake` must exist.)
3. **Set it up** (makes what the mods need on your machine, such as the Jukebox's title lists):
   ```bash
   "C:\Games\tekken3-expanded\.setup\venv\Scripts\python.exe" "C:\Games\tekken3-expanded\addons\tmst\setup\tmst_setup.py"
   ```
   It ends with `TMST setup: done`.
4. **Rebuild the game with the add-on** (as the Expanded's setup builds it; your mod settings and game data stay):
   ```bash
   "C:\Games\tekken3-expanded\addons\tmst\dev\build.cmd"
   ```
   It ends with `TMST build: build-release with the add-on`.

## Settings

Start `Tekken 3 Expanded.exe`, open the **Mods** tab and switch the mods on in the **TMST** group (both are off by
default). Restart the game after switching a mod. "TMST add-on test" is a developer test: leave it off.

## Updating and removing

- **Expanded update** (Update & play): the add-on stays in the build. If the TMST group is missing afterwards, repeat
  step 4.
- **Add-on update:** `git -C "C:\Games\tekken3-expanded\addons\tmst" pull`, then steps 3 and 4.
- **Removing:** close the game, rebuild without the add-on, then delete the folder:
  ```bash
  "C:\Games\tekken3-expanded\addons\tmst\dev\build.cmd" --without
  ```
  Deleting the folder first breaks the Expanded's Update & play: put it back and rebuild without it.

A Hungarian guide with troubleshooting is in `docs\INSTALL.hu.md`. Report problems with the add-on to its author, not
to the Expanded's.

## License

PolyForm Noncommercial 1.0.0.
