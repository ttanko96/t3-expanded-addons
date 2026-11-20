#!/usr/bin/env python3
"""The Jukebox's TEKKEN 1 CONSOLE / ARCADE music from the player's Tekken
(1) PlayStation disc (tools/tekken1_disc.py, remembered by the add-on's setup
in workspace/tekken1-disc.json) into
addons/tmst/workspace/t1-music/{console,arcade}/*.bgm (run by the add-on's
setup, setup/tmst_setup.py). The Tekken 1 pieces the Tekken 2 disc plays
again stay tools/tekken2_music.py's; the TEKKEN 1 lists name both.

    python tools/tekken1_music.py --disc <Tekken disc> [--root <expanded folder>]

The disc (Europe SCES-00005 tried; USA SLUS-00006 and Japan SLPS-00040
taken alike): its music is CD audio, tracks 2..27.
The program (TEKKEN.EXE, RAM 0x8001DF68) has a table of 2 x 14 track
numbers, per music id the one of Options' BGM SELECT ARRANGE (the first 14,
the Console here) and ORIGINAL (the arcade's, the next 14); the player at
0x8007A8BC takes ids 0..10 by the stage (the stage files' order:
tools/tekken1_stages.py), 0x8007A91C id 11 (looping), 0x8007A970 and
0x8007A9B8 ids 12 and 13 (once; the same track in both modes). Track 28, in
no table, is silence (3 minutes of zeros). The table is found by its bytes
(Europe's), else by its shape (find_table(): another version's order); a
disc with neither is left out.

Which file is which (PIECES): the names of the add-on's title lists
(data/jukebox/tekken1-console|arcade.txt), t1-<stage>.bgm and
t1-music-NN.bgm (the ids no stage has).

.bgm: the frames themselves ('T3BGM001', u32 44 100, u32 frames, u32 loop
frame 0, s16 left and right), each track's silence either side cut, at Tekken
3's music loudness (tools/tekken2_music.py's write()), looping whole.

No disc: nothing is made (and nothing removed). Pure Python with numpy (the
Expanded's setup has it), one process per piece.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

ADDON = Path(__file__).resolve().parents[1]
EXPANDED = ADDON.parents[1]                 # <expanded>/addons/tmst
sys.path.insert(0, str(ADDON / 'tools'))
from disc_image import DiscError, open_disc  # noqa: E402
import jukebox_titles  # noqa: E402
from tekken2_music import JOBS, write  # noqa: E402

OUT = 'addons/tmst/workspace/t1-music'
# Increase whenever the .bgm files this writes change: the setup makes them again.
VERSION = 1

EXE = 'TEKKEN.EXE'
# The program's music table (RAM 0x8001DF68): ARRANGE's 14 tracks, then ORIGINAL's.
TABLE = bytes([9, 4, 7, 3, 12, 10, 13, 8, 5, 6, 11, 2, 14, 15,
               23, 18, 21, 17, 26, 24, 27, 22, 19, 20, 25, 16, 14, 15])
IDS = 14
# Music ids 0..10: the stages (tools/tekken1_stages.py's order); 11..13 no stage's.
STAGES = ('windermere', 'kyoto', 'acropolis', 'marine-stadium', 'monument-valley', 'king-george-island', 'fiji',
          'venezia', 'angkor-wat', 'szechwan', 'chicago')


def name_of(music_id: int) -> str:
    return f't1-{STAGES[music_id]}.bgm' if music_id < len(STAGES) else f't1-music-{music_id}.bgm'


def pieces(table: bytes = TABLE) -> list[tuple[str, str, int]]:
    """(folder, file, CD audio track), by the program's table (Europe's by default)."""
    out = []
    for folder, mode in (('console', 0), ('arcade', 1)):
        out += [(folder, name_of(m), table[mode * IDS + m]) for m in range(IDS)]
    return out


def shaped(w: bytes) -> bool:
    """28 bytes shaped as the music table: 12 + 12 distinct tracks, the last
    two ids' the same in both halves, 26 different tracks in all, 2..99."""
    tracks = w[:12] + w[12:14] + w[14:26]
    return (len(w) == 2 * IDS and w[12:14] == w[26:28] and len(set(tracks)) == 26
            and all(2 <= t <= 99 for t in tracks))


def find_table(exe: bytes) -> bytes | None:
    """The program's music table: Europe's bytes, else the one window of its
    shape (none or several: None)."""
    if TABLE in exe:
        return TABLE
    found = {exe[i:i + 2 * IDS] for i in range(len(exe) - 2 * IDS + 1) if shaped(exe[i:i + 2 * IDS])}
    return found.pop() if len(found) == 1 else None


def skipped() -> list[str]:
    """The pieces not made ('<folder>/<file>'): no title list names them (or
    only --disable) and nothing else plays them (tools/jukebox_titles.py's skipped())."""
    return [f'{folder}/{name}' for folder in ('console', 'arcade')
            for name in jukebox_titles.skipped(f't1-music/{folder}', [n for f, n, _ in pieces() if f == folder])]


def find_disc(state: Path) -> Path | None:
    """The Tekken disc the add-on's setup remembered (workspace/tekken1-disc.json), if it is still there."""
    import json
    try:
        disc = json.loads((state / 'tekken1-disc.json').read_text(encoding='utf-8')).get('disc') or {}
    except (OSError, ValueError):
        return None
    path = disc.get('path')
    return Path(path) if path and Path(path).is_file() else None


def check(disc) -> bytes:
    """The program's music table; DiscError unless there is one and its tracks are CD audio."""
    exe = disc.read_file(EXE)
    if exe is None:
        raise DiscError(f'not a Tekken disc (no {EXE})')
    table = find_table(exe)
    if table is None:
        raise DiscError('a Tekken version whose music table was not found')
    audio = {t.number for t in disc.tracks if t.kind == 'AUDIO'}
    missing = sorted({t for _, _, t in pieces(table)} - audio)
    if missing:
        raise DiscError(f'CD audio track(s) {", ".join(map(str, missing))} missing')
    return table


def trimmed(data: bytes) -> np.ndarray:
    """A CD audio track's frames (16-bit stereo, little-endian), silence either side cut."""
    pcm = np.frombuffer(data[:len(data) // 4 * 4], '<i2').reshape(-1, 2).astype(np.float64)
    audible = np.nonzero(np.abs(pcm).max(1) > 0)[0]
    if not len(audible):
        raise DiscError('the track is silent')
    return pcm[audible[0]:audible[-1] + 1]


def make(disc_path: str, track: int, out: str) -> str:
    """One piece (in a worker process): read from the disc, written."""
    with open_disc(disc_path) as disc:
        try:
            pcm = trimmed(disc.audio(track))
        except DiscError as error:
            raise DiscError(f'CD audio track {track}: {error}') from None
    write(Path(out), pcm)
    return Path(out).name


def convert(disc: Path, out: Path, say=print, jobs: int = JOBS, skip=None) -> list[str]:
    """Every piece of PIECES as out/<folder>/<file> but those of skip (default
    skipped(), their files removed); the files written ('<folder>/<file>').
    DiscError when the disc's music is not the one known."""
    skip = set(skipped() if skip is None else skip)
    with open_disc(disc) as d:
        table = check(d)
        lengths = {t.number: t.frames for t in d.tracks}
    todo: dict[int, list[Path]] = {}                # the same track is made once, then copied
    for folder, name, track in pieces(table):
        if f'{folder}/{name}' in skip:
            (out / folder / name).unlink(missing_ok=True)
            continue
        todo.setdefault(track, []).append(out / folder / name)
    for folder in ('console', 'arcade'):
        (out / folder).mkdir(parents=True, exist_ok=True)
    order = sorted(todo, key=lambda t: -lengths.get(t, 0))     # the longest first
    names = []
    with ProcessPoolExecutor(max_workers=max(1, jobs)) as pool:
        futures = [(t, pool.submit(make, str(disc), t, str(todo[t][0]))) for t in order]
        for track, future in futures:
            future.result()
            first, *others = todo[track]
            for other in others:
                shutil.copyfile(first, other)
            names += [f'{p.parent.name}/{p.name}' for p in todo[track]]
    say(f'Tekken 1 music: {len(names)} piece(s) in {out}')
    return sorted(names)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=EXPANDED, help='the Expanded folder (default: the one this add-on is in)')
    ap.add_argument('--disc', type=Path, help='the Tekken disc (default: the one the add-on setup remembered)')
    ap.add_argument('--jobs', type=int, default=JOBS, help=f'pieces made at a time (default {JOBS})')
    args = ap.parse_args(argv)
    disc = args.disc or find_disc(args.root / 'addons/tmst/workspace')
    if not disc:
        print('Tekken 1 music: no Tekken 1 disc (Setup TMST.cmd asks for it)')
        return 0
    convert(disc, args.root / OUT, jobs=args.jobs)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
