#!/usr/bin/env python3
"""The Jukebox's title lists: addons/tmst/workspace/jukebox/<category>.txt (the
add-on's own folder, which the Expanded's setup never cleans up) (run by the add-on's setup, setup/tmst_setup.py).

    python tools/jukebox_titles.py --root <expanded folder>

One file per category of the Jukebox (src/jukebox/tmst_jukebox.c), each line

    file = title

for the music this install has now: the .bgm files of the category's
folders, and for Tekken 3 the music the disc's executable names (xa-NN, an
XA stream of TEKKEN3.XAS; cd-track-N, a CD audio track). A new file gets a
line whose title is its name (without .bgm); lines already there are never
changed, reordered or removed, so titles written by hand stay, and so do the
lines of music that is gone (the game leaves those out). A title with
"--disable" in it keeps that music out of the game's list. The game lists the
music in the order of these lines, then the rest by name.

data/jukebox/<category>.txt (this add-on) are the lists every install starts
with: a list not there yet starts as that file, and a new file it names gets
its line from it (its title) rather than its name.

Tekken 1's two lists are only what they name: its music is the Tekken 2
folders' files the Tekken 2 disc plays again, so a new file there gets a
line in the Tekken 2 list, never in Tekken 1's.
"""
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
EXPANDED = ADDON.parents[1]                 # <expanded>/addons/tmst

# id, file, folders under the Expanded's folder; disc mode for Tekken 3.
# No folders: the list is only what it names (Tekken 1, the game's only_listed).
CATEGORIES = [
    ('t1-console', 'tekken1-console.txt', [], None),
    ('t1-arcade', 'tekken1-arcade.txt', [], None),
    ('t2-console', 'tekken2-console.txt', ['addons/tmst/workspace/t2-music/console'], None),
    ('t2-arcade', 'tekken2-arcade.txt', ['addons/tmst/workspace/t2-music/arcade'], None),
    ('t3-console', 'tekken3-console.txt', [], 0),
    ('t3-arcade', 'tekken3-arcade.txt', [], 1),
    ('ttt-console', 'ttt-console.txt', ['addons/tmst/workspace/ttt-music/console'], None),
    ('ttt-arcade', 'ttt-arcade.txt', ['addons/tmst/workspace/ttt-music/arcade'], None),
]
DEFAULTS = ADDON / 'data/jukebox'
LISTS = 'addons/tmst/workspace/jukebox'
HEADER = ('# Jukebox titles, {name}: one line per music file, "file = title".\n'
          '# Write the title you want after the "="; the setup only adds lines for new files.\n'
          '# "--disable" after the "=" (with or without a title) leaves that music out of the list.\n')

# SLUS-00402 (load address 0x80010000, header 0x800): the XA table
# (0x80024D78, 50 x 12 bytes) and the music table (0x80024FD0, per music id
# 4 bytes: the XA of mode 0 (arranged) and its volume, then mode 1 (arcade);
# bit 7 of the XA byte: a CD audio track).
EXE_BASE = 0x80010000 - 0x800
XA_TABLE, XA_COUNT, MUSIC_TABLE, MUSIC_IDS = 0x80024D78, 50, 0x80024FD0, 32


def disc_music(root: Path, mode: int) -> list[str]:
    """The Tekken 3 music the executable names, from the copy the Expanded's
    setup keeps in disc/ (SLUS_004.02)."""
    exe = root / 'disc' / 'SLUS_004.02'
    if not exe.is_file():
        return []
    data = exe.read_bytes()
    xa = set()
    for music in range(MUSIC_IDS):
        b = data[MUSIC_TABLE - EXE_BASE + music * 4 + mode * 2]
        if not b & 0x80 and b < XA_COUNT:
            first, last, channel = struct.unpack_from('<3I', data, XA_TABLE - EXE_BASE + b * 12)
            if channel < 8 and first <= last:
                xa.add(b)
    names = [f'xa-{i:02d}' for i in sorted(xa)]
    if mode == 0:                       # the disc's CD audio, as the game lists it
        names += [f'cd-track-{t}' for t in audio_tracks(root) if t >= 2]
    return names


def audio_tracks(root: Path) -> list[int]:
    cues = sorted((root / 'disc').glob('*.cue'))
    if not cues:
        return []
    tracks = re.findall(r'TRACK\s+(\d+)\s+AUDIO', cues[0].read_text(errors='replace'), re.I)
    return [int(t) for t in tracks]


def music_files(root: Path, folders) -> list[str]:
    names = []
    for folder in folders:
        d = root / folder
        if d.is_dir():
            names += [p.name for p in sorted(d.glob('*.bgm')) if p.name not in names]
    return names


def listed(text: str) -> set[str]:
    names = set()
    for line in text.splitlines():
        line = line.strip()
        if line and not line.startswith('#') and '=' in line:
            name = line.split('=', 1)[0].strip()
            names.add(name)
            if not name.endswith('.bgm'):
                names.add(name + '.bgm')
    return names


def default_lines(file: str) -> dict[str, str]:
    """data/jukebox/<file>'s lines by the music file they name."""
    path = DEFAULTS / file
    lines = {}
    for line in (path.read_text(encoding='utf-8').splitlines() if path.is_file() else []):
        if line.strip() and not line.lstrip().startswith('#') and '=' in line:
            lines[line.split('=', 1)[0].strip()] = line.strip()
    return lines


def update(root: Path) -> int:
    out = root / LISTS
    out.mkdir(parents=True, exist_ok=True)
    added = 0
    for cid, file, folders, mode in CATEGORIES:
        names = disc_music(root, mode) if mode is not None else music_files(root, folders)
        path = out / file
        if path.is_file():
            text = path.read_text(encoding='utf-8')
        elif (DEFAULTS / file).is_file():
            text = (DEFAULTS / file).read_text(encoding='utf-8')
        else:
            text = HEADER.format(name=cid)
        have = listed(text)
        new = [n for n in names if n not in have]
        if not new and path.is_file():
            continue
        if text and not text.endswith('\n'):
            text += '\n'
        defaults = default_lines(file)
        text += ''.join(defaults.get(n, f'{n} = {n[:-4] if n.endswith(".bgm") else n}') + '\n' for n in new)
        path.write_text(text, encoding='utf-8')
        added += len(new)
        print(f'{path.relative_to(root)}: {len(new)} new line(s)')
    print(f'Jukebox titles: {added} new line(s)')
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=EXPANDED, help='the Expanded folder (default: the one this add-on is in)')
    return update(ap.parse_args(argv).root)


if __name__ == '__main__':
    raise SystemExit(main())
