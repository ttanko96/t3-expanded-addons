# TMST add-on for Tekken 3 Expanded

New features for [Tekken 3 Expanded](https://github.com/omarma/tekken3-expanded), packaged as mods that show up in the game's own **Mods**
tab, in the **TMST** group. Each mod can be switched on and off there; nothing in the Expanded's own files is changed.

No game data is included: everything a mod needs from the game is made on your machine by the add-on's setup.

## Add-On Features (currently)

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

## Installation

You need Windows 10/11, a Tekken 3 Expanded that has been set up once with its own `Setup Tekken 3.cmd` (the game
starts), access to this repository and about 750 MB of free space. Below, `C:\Games\tekken3-expanded` is the
Expanded's folder: change it if yours is elsewhere.

1. **Close the game.** A running game cannot be rebuilt.
2. **Put the add-on in place**, exactly at `C:\Games\tekken3-expanded\addons\tmst\`:
   ```bash
   git clone https://github.com/ttanko96/t3-expanded-addons.git "C:\Games\tekken3-expanded\addons\tmst"
   ```
   (Or download the ZIP, rename the folder to `tmst` and put it in `addons\`; `addons\tmst\addon.cmake` must exist.)
3. **Double-click `addons\tmst\Setup TMST.cmd`.** It uses the Expanded's own Python and compilers, and:
   - looks for your **Tekken 2** disc (optional; nothing is taken from it yet) beside the discs the Expanded's setup was
     given, else asks for it in a window (Cancel goes on without it). USA (SLUS-00213) and Europe (SCES-00255), as a
     `.cue` with its `.bin` files, a `.bin` or a `.chd`. To give or change it later, drop the disc on `Setup TMST.cmd`;
   - makes what the mods need on your machine: the Jukebox's TTT console and arcade music (from the TTT PS2 disc's and
     tektagt.zip's music the Expanded's setup imported) and its title lists;
   - rebuilds the game with every mod of the add-on (Jukebox, Photo Mode), as the Expanded's setup builds it: your mod
     settings and game data stay.

   It stops with a clear message if the Expanded has not been set up yet, and ends with
   `The TMST add-on is installed`. Once the Expanded runs add-on setups itself, its **Update & play** does all of this.

## Settings

Start `Tekken 3 Expanded.exe`, open the **Mods** tab and switch the mods on in the **TMST** group (both are off by
default). Restart the game after switching a mod.

## Updating and removing

- **Expanded update** (Update & play): the add-on stays in the build. If the TMST group is missing afterwards, run
  `Setup TMST.cmd` again.
- **Add-on update:** `git -C "C:\Games\tekken3-expanded\addons\tmst" pull`, then `Setup TMST.cmd`.
- **Removing:** close the game, rebuild without the add-on, then delete the folder:
  ```bash
  "C:\Games\tekken3-expanded\addons\tmst\dev\build.cmd" --without
  ```
  If the folder is deleted first, the Expanded's next Update & play builds the game without the add-on by itself.

## License

PolyForm Noncommercial 1.0.0.
