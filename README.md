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
- **Categories** (console and arcade each): Tekken 1 and Tekken 2 (with **Tekken 2 Content**'s Music on, from your Tekken 2
  disc: it plays ten Tekken 1 stage themes again), Tekken 3 (the game's disc), Tekken Tag Tournament (the TTT music
  the Expanded imported).
- In a fight the opponent's (P2's) chosen music plays, whatever stage is picked; without a choice, the game's own.
  It starts, fades and stops with the game's music (the match's end, the pause, the BGM SELECT's silence).
- The choices are saved beside the memory card (`saves\jukebox.txt`). The track titles are plain text lists in
  `addons\tmst\workspace\jukebox\` (`file = title` per line; `--disable` in a title leaves the track out).

### Photo Mode
A free camera in any paused fight, and photos of it.
- **Open:** pause a fight and choose **PHOTO** (Arcade, VS, Team Battle, Time Attack, Survival, Tekken Ball). In
  Practice, press **SELECT** while paused. Not in Tekken Force.
- **Camera:** D-pad move · L1/R1 down/up · □ + D-pad turn and look · L2/R2 zoom · SELECT reset the camera. The camera
  stops at the edge of the stage.
- **Panel (START):** ZOOM, FOCUS and BLUR (depth of field: the fighters sharp, or a sharp band), FILTER (14: black &
  white, sepia, noir, vintage, high contrast, cold, warm, teal & orange, duotone, poster, night vision, scanlines,
  negative), FRAME (8: letterbox, polaroid, film strip, vignette, T3E Red, T3E Black, CRT TV, comic; the T3E frames
  carry the T3E logo and Tekken 3's own letters), RESET.
- ✕ take a photo · △ hide the HUD (Tekken Ball's power bars and numbers too) · ○ back to the pause menu, the fight
  exactly as it was.
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
- **Boss intro:** before Devil, a short cinematic: Kazuya standing in the dark, the camera coming to him, then his
  Tekken Tag change in lightning and a white flash, and Devil stands there; his music starts with it, the move's
  own sounds too. START skips it (from the change on). Made from the fighters' own TTT1 moves in your install
  (TTT1 Characters needed; without them the fight starts as before).
- **Screens:** the bracket after each match (✕ next match · ○ leave the tournament), the next match before each
  fight, CHAMPION (and the tournament it unlocks) or TOURNAMENT OVER at the end.
- **Options:** OPTION → GAME OPTION shows TOURNAMENT in the box of modes at the top right, in gold where the
  option affects the tournament (as Team Battle's, but FIGHT COUNT: the tournament's rounds are its own; and TEKKEN 2 STAGES,
  which its Tekken 1 and Tekken 2 tournaments use).
- The tournaments won are saved beside the memory card (`saves\tournament.txt`; deleting it locks them again).
- The screens' backgrounds are PNG files you may redraw: `addons\tmst\data\tournament\` (`progress`, `match`,
  `champion`, `over`, 854×480); `data\tournament\guides\` shows where the mod draws on each.

### Tekken 2 Content
What the add-on takes from your own Tekken 2 disc (given to `Setup TMST.cmd`), in one package. The setup makes
everything whether it is switched on or not, so switching it on needs only a restart.

#### Switches
- **Tekken 2 Content** in the TMST group switches the whole package.
- Its parts are in the group right below it, **TMST / Tekken 2 Content**, each on its own.
- With the package off, none of its parts runs; switching a part on switches the package on too.

#### Music
- Tekken 2's music and the ten Tekken 1 stage themes it plays again, Console and Arcade.
- They are the Jukebox's **TEKKEN 1** and **TEKKEN 2** categories (switch the Jukebox on too).
- **Off:** those categories are hidden; a fighter given such music plays the game's own until it is on again (the
  choice stays saved).

#### Stages
Tekken 2's 23 stages, drawn by the game's stage engine as Tekken 3 arcade's are.
- **Needs:** **Arcade Stages** switched on.
- **Picking one:** in VS and Practice (Stage Select on), **TEKKEN 2** is a game on the STAGE line, after the others;
  press **Down** for its stages (RANDOM, then the 23).
- **Version:** GAME OPTION's **TEKKEN 2 STAGES** row, **CLASSIC** or **REMAKE**.
- **Light:** on both versions the fighters are lit by daylight, dimmed only on the dark stages (Church, Pagoda,
  Serene Forest, Taj Mahal, Mirror and Eternal Darkness).

**Classic**
- Tekken 2's own panorama and floor; tekken3.zip is not needed.
- The dark stages lit round the fighters, Mirror Darkness with its mirror, Hong Kong's running lights.

**Remake**
- **18 stages remade:** scenery, animations and new painted backgrounds.
- **Not remade yet:** Cambodian Wetlands, Athens, Nowhere Desert, Hills and Eternal Darkness are the Classic's for
  now.
- **What it is built from:**
  - the backgrounds open from patches that only your Tekken 2 disc opens;
  - the stages are built with the Expanded setup's tekken3.zip; without it, every stage is the Classic.
- **Life on the stages:**
  - dust where a fighter falls;
  - butterflies, fireflies and dragonflies;
  - falling petals, leaves and feathers, newspapers in the wind;
  - grass on the ground, lit windows and candles, the Taj Mahal's rain and oil lamps;
  - birds, Grand Canyon's eagle, crabs, rabbits and pigeons;
  - Akihabara's ship and Hong Kong's helicopter.
- **Lightning:** over the Taj Mahal and at the Church's windows. Its flashes and thunder come from TTT1 Stages'
  storm (the Expanded's TTT stage packs); without them, there is no lightning.
- **Writing:** the signs are written in each country's language.
- **Unknown:** Szechwan, Hong Kong, Akihabara, the Great Wall, Launch Pad, Kyoto and Arizona have the top of their
  background trimmed, so that Unknown's wolf and the add-on's letters keep their room in the video memory.

#### Wireframe Mode
Tekken 2's hidden Wireframe Mode: your fighter drawn in glowing green lines, no fill, and the camera just behind its
head, looking the way the body faces.
- **Switching it on:** not a button at the selector as in Tekken 2, but GAME OPTION's **TEKKEN 2 WIREFRAME MODE**
  row, **OFF** or **ON** (kept beside the memory card, in `t2-wireframe.txt`).
- **Modes:** Arcade, Team Battle, Tournament, Time Attack, Survival and Practice, with one human player. A
  challenger's fight in the Arcade, or a Team Battle of two players, keeps the game's own look; VS, Tekken Ball and
  Tekken Force too.
- **The game's own camera and the solid fighter:** at the fighters' entrance, during throws and from the KO on (the
  replay, the winner's pose); in Photo Mode the fighter is solid too.
- **The camera follows:** it turns the way the game turns the fighter, smoothly, so that a combo's blows do not
  swing it; TTT1 guests too.
- Your fighter's shadow stays.

## Installation

You need Windows 10/11, a Tekken 3 Expanded that has been set up once with its own `Setup Tekken 3.cmd` (the game
starts), access to this repository and about 2.1 GB of free space: the Jukebox's decoded music takes it in
`addons\tmst\workspace\` (Tekken 1 and 2 about 1.3 GB, TTT about 470 MB). Below, `C:\Games\tekken3-expanded` is the
Expanded's folder: change it if yours is elsewhere.

1. **Close the game.** A running game cannot be rebuilt.
2. **Put the add-on in place**, exactly at `C:\Games\tekken3-expanded\addons\tmst\`:
   ```bash
   git clone https://github.com/ttanko96/t3-expanded-addons.git "C:\Games\tekken3-expanded\addons\tmst"
   ```
   (Or download the ZIP, rename the folder to `tmst` and put it in `addons\`; `addons\tmst\addon.cmake` must exist.)
3. **Double-click `addons\tmst\Setup TMST.cmd`.** It opens the add-on's setup window (the Expanded's look, with a
   progress bar and a setup log), which uses the Expanded's own Python and compilers. Click **Install & play**; it:
   - takes your **Tekken 2** disc (optional; it gives the Jukebox's Tekken 1 and 2 music and the Tekken 2 stages):
     - the window fills it in when it finds one beside the discs the Expanded's setup was given; **Browse** for
       another, or clear the field to go on without it; a disc dropped on `Setup TMST.cmd` is filled in too;
     - USA (SLUS-00213) and Europe (SCES-00255), as a `.cue` with its `.bin` files, a `.bin` or a `.chd`;
   - makes what the mods need on your machine:
     - from the Tekken 2 disc: the Jukebox's Tekken 1 and 2 music and the Tekken 2 stages (the Remake's stages take a
       few minutes the first time);
     - the Jukebox's TTT console and arcade music (from the TTT PS2 disc's and tektagt.zip's music the Expanded's
       setup imported) and its title lists;
   - rebuilds the game with every mod of the add-on (Jukebox, Photo Mode, Tournament Mode, Tekken 2 Content), as the
     Expanded's setup builds it: your mod settings and game data stay.

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

[PolyForm Noncommercial 1.0.0](LICENSE). `tools/ttm_render.c` adapts the C352 replay of
[Tekken 3 Expanded](https://github.com/omarma/tekken3-expanded) by Matthew Stan (PolyForm Noncommercial 1.0.0), itself
an adaptation of MAME's C352 by R. Belmont and superctr (BSD-3-Clause, [licenses/MAME-BSD-3-Clause.txt](licenses/MAME-BSD-3-Clause.txt)).
The add-on's lettering is Fira Sans Medium by The Mozilla Foundation and Telefonica S.A. (SIL Open Font License 1.1,
[tools/data/fonts/OFL.txt](tools/data/fonts/OFL.txt)), drawn into `src/core/tmst_ui_font_data.c` by `tools/ui_font.py`.
The Tekken 2 Remake's backgrounds (`tools/data/t2-remake-patches/`) are painted over Tekken 2's panoramas; they are
kept as patches mixed with a key made from the Tekken 2 disc's own data, so they open only with your disc.
