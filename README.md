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

### Tournament Mode
The King of Iron Fist Tournament, as a mode of its own.
- **Open:** **TOURNAMENT MODE** in the main menu (in gold, under TEKKEN FORCE MODE).
- **Tournaments:** Tekken 1, Tekken 2, Tekken 3, Tekken Tag and Free. At first only Tekken 1 is open; winning a
  tournament opens the next (Free after Tekken Tag), for good. Up/down choose · ✕ enter · ○ back to the main menu.
- **Your fighter:** on the Team Battle grid. Only the highlighted fighters take part in the tournament; the face
  buttons pick the costume (a fighter with a TTT1 moveset asks for it on the MOVESET card) · L2/R2 switch the
  character panel (with TTT1 characters) · SELECT back to the tournaments.
- **The bracket:** 16 fighters, single elimination: first round, quarter final, semi final, final, then the Grand
  Final against the boss. A match is won with two rounds, each round starting with full life; the CPU gets stronger
  from round to round of the tournament. The other matches of a round are decided as you play yours.
- **The bosses:** Tekken 1 Heihachi; Tekken 2 Kazuya, then Devil; Tekken 3 Ogre, then True Ogre (one round each, you
  take the life you have left on to the second); Tekken Tag Unknown (one round: she heals and changes her style);
  Free a boss drawn at the start (three rounds). The bosses cannot be picked.
- **Screens:** the bracket after each match (✕ next match · ○ leave the tournament), the next match before each
  fight, CHAMPION (and the tournament it unlocks) or TOURNAMENT OVER at the end.
- The tournaments won are saved beside the memory card (`saves\tournament.txt`; deleting it locks them again).
- The screens' backgrounds are PNG files you may redraw: `addons\tmst\data\tournament\` (`progress`, `match`,
  `champion`, `over`, 854×480); `data\tournament\guides\` shows where the mod draws on each.

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
3. **Double-click `addons\tmst\Setup TMST.cmd`.** It opens the add-on's setup window (the Expanded's look, with a
   progress bar and a setup log), which uses the Expanded's own Python and compilers. Click **Install & play**; it:
   - takes your **Tekken 2** disc (optional; nothing is taken from it yet): the window fills it in when it finds one
     beside the discs the Expanded's setup was given; **Browse** for another, or clear the field to go on without it. USA
     (SLUS-00213) and Europe (SCES-00255), as a `.cue` with its `.bin` files, a `.bin` or a `.chd`. A disc dropped on
     `Setup TMST.cmd` is filled in too;
   - makes what the mods need on your machine: the Jukebox's TTT console and arcade music (from the TTT PS2 disc's and
     tektagt.zip's music the Expanded's setup imported) and its title lists;
   - rebuilds the game with every mod of the add-on (Jukebox, Photo Mode, Tournament Mode), as the Expanded's setup builds it: your mod
     settings and game data stay.

   The window says so if the Expanded has not been set up yet or the game is running. When it reads
   `The TMST add-on is installed`, **Play** opens the game's launcher. Once the Expanded runs add-on setups itself, its
   **Update & play** does all of this.

## Settings

Start `Tekken 3 Expanded.exe`, open the **Mods** tab and switch the mods on in the **TMST** group (all are off by
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
