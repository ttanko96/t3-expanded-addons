#!/usr/bin/env python3
"""The Jukebox's TEKKEN TAG TOURNAMENT CONSOLE music: the 14 PS2 stage pieces
of the Expanded's mods/ttt-ps2-music/ps2-music.ttp (made by its setup from the
player's TTT PS2 disc, tools/ttt_stages/ps2_music_install.py), decoded into
addons/tmst/workspace/ttt-music/console/ps2-<stage>.bgm (run by the add-on's
setup, setup/tmst_setup.py).

    python tools/ttt_ps2_music.py --root <expanded folder>

ps2-music.ttp: 'TTP2', u32 format, u32 pieces, then per piece u32 PS2 BGM
number, offset of its first sector, sectors, loop sector, volume (0x3FFF =
full); each 2 048-byte sector is PS-ADPCM, 1 024 bytes left then 1 024 right,
44 100 Hz (the Expanded's src/tekken3_ttt_music.c plays it the same way).

.bgm (src/jukebox/tmst_jukebox.c): 'T3BGM001', u32 44 100, u32 frames, u32 loop
frame, then the frames, s16 left and right. The driver's volume is applied
here, so the pieces keep the game's levels between them.

The PS2 game's other music (its BGM table, SLUS_200.01 0x50CC00: 10 bytes per
number, u16 volume at +2, u16 TEKKEN.BIN file at +6, s16 loop sector at +8,
< 0 the start), the pieces ps2-music.ttp leaves out, comes from the disc the
Expanded's setup was given (.setup/last-inputs.json's ttt_disc), each read at
its sectors and checked by sha256 (EXTRAS): ps2-embu, ps2-ending and four
pieces ps2-bgm-NN by number. The 2 to 4 s jingles (0x17..0x1B) are left out.

No pack and no disc: nothing is made (and nothing removed). Decoding is pure Python, one
process per piece: a few seconds.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from array import array
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
EXPANDED = ADDON.parents[1]                 # <expanded>/addons/tmst

# Where the Expanded keeps the pack: beside the game (its build), then its setup's own copy.
PACKS = ['build-release/mods/ttt-ps2-music/ps2-music.ttp', 'mods/ttt-ps2-music/ps2-music.ttp',
         'workspace/ttt-stages/ps2-music-pack/ps2-music.ttp']
OUT = 'addons/tmst/workspace/ttt-music/console'
# Increase whenever the .bgm files this writes change: the setup makes them again.
VERSION = 2

# PS2 BGM number -> stage (tools/ttt_stages/ps2_music_install.py's PIECES).
STAGES = {0x09: 'paul', 0x0A: 'law', 0x0B: 'yoshimitsu', 0x0C: 'xiaoyu', 0x0D: 'hwoarang',
          0x0E: 'lei', 0x0F: 'ogre', 0x10: 'school', 0x11: 'jin', 0x12: 'nina',
          0x13: 'eddy', 0x14: 'king', 0x15: 'heihachi', 0x16: 'unknown'}
SECTOR, HALF = 2048, 1024
SECTOR_FRAMES = HALF // 16 * 28              # 1 792
FULL = 0x3FFF
F0, F1 = (0, 60, 115, 98, 122), (0, 0, -52, -55, -60)

# The rest of the BGM table: number, name, first LBA, sectors, loop sector, volume, sha256 of the sectors.
# 0x01 plays before a fight (the Embu, as the Expanded's tools/ps2/ttt_disc_fingerprint.py names its
# sectors); 0x05 the name entry; 0x06 (and 0x07, the same file) the endings (tools/ps2/ttt_to_cine.py: piece 7);
# 0x04 = 0x03. 0x02, 0x03 and 0x08 are made too, their titles --disable in data/jukebox/ttt-console.txt.
EXTRAS = [
    (0x01, 'embu', 111953, 922, 0, 13107, '41e8f65f2162f17f09d42efa0a140f957f4483f3a9f2bf7715cab94a69e4f578'),
    (0x02, 'bgm-02', 104579, 2290, 0, 10649, 'be8c0c371506fa241dfeab36ea81dcb7f4da1cc3b118fe9c5f542f7e241c05cd'),
    (0x03, 'bgm-03', 144319, 394, 0, 9011, '9d33e5b4beaa17a350720b44d4996fcd08727689ee0921df4228b7af29b6bca5'),
    (0x05, 'bgm-05', 107120, 338, 0, 10649, 'ab1e3292cbcf58d76f5e98ced18bc57d1b34b36f8f02da2bb4de0e4a6e9e6f6f'),
    (0x06, 'ending', 107458, 4495, 0, 15564, '3675d45faf3b3203bb30704a77156f460839ea08425c95b0b20e23769cd0f741'),
    (0x08, 'bgm-08', 106869, 251, 0, 8192, 'f9ddaa8d24e31a72ed5edbaa4862988f66d307d00afa79f0e1402b5c4569e722'),
]


def find_pack(root: Path) -> Path | None:
    for rel in PACKS:
        if (root / rel).is_file():
            return root / rel
    return None


def find_disc(root: Path) -> Path | None:
    """The TTT PS2 disc the Expanded's setup was last given, if it is still there."""
    try:
        disc = json.loads((root / '.setup/last-inputs.json').read_text(encoding='utf-8')).get('ttt_disc')
    except (OSError, ValueError):
        return None
    return Path(disc) if disc and Path(disc).is_file() else None


def disc_image(disc: Path) -> tuple[Path, int, int]:
    """(image file, sector size, data offset) of a .cue (its first FILE), .bin or .iso."""
    if disc.suffix.lower() == '.cue':
        line = next(l for l in disc.read_text(errors='replace').splitlines() if l.strip().upper().startswith('FILE'))
        disc = disc.parent / line.split('"')[1]
    with disc.open('rb') as f:
        raw = f.read(12) == b'\0' + b'\xff' * 10 + b'\0'
    return disc, (2352 if raw else 2048), (24 if raw else 0)


def disc_sectors(disc: Path, lba: int, count: int) -> bytes:
    path, size, offset = disc_image(disc)
    out = bytearray()
    with path.open('rb') as f:
        for n in range(lba, lba + count):
            f.seek(n * size + offset)
            out += f.read(SECTOR)
    return bytes(out)


def pieces(pack: Path) -> list[tuple[int, int, int, int, int]]:
    """(number, offset, sectors, loop sector, volume) of each piece."""
    with pack.open('rb') as f:
        head = f.read(12)
        if len(head) < 12 or head[:4] != b'TTP2':
            raise ValueError(f'{pack}: not a TTP2 pack')
        count = struct.unpack_from('<I', head, 8)[0]
        table = f.read(20 * count)
    out = [struct.unpack_from('<5I', table, 20 * i) for i in range(count)]
    for number, offset, sectors, loop, volume in out:
        if not sectors or loop >= sectors:
            raise ValueError(f'{pack}: piece 0x{number:02X} is not whole')
    return out


def decode(data: bytes, volume: int) -> array:
    """One channel's PS-ADPCM blocks (16 bytes, 28 samples) as s16, at volume."""
    out = array('h')
    append = out.append
    h0 = h1 = 0
    # The 16 nibbles at each shift, the volume applied at the end.
    nibbles = [[(((n << 12) ^ 0x8000) - 0x8000) >> s for n in range(16)] for s in range(13)]
    for at in range(0, len(data), 16):
        b = data[at]
        shift, filt = b & 15, b >> 4
        nib = nibbles[shift if shift <= 12 else 9]
        f0, f1 = F0[filt if filt <= 4 else 4], F1[filt if filt <= 4 else 4]
        for byte in data[at + 2:at + 16]:
            for n in (byte & 15, byte >> 4):
                v = nib[n] + ((h0 * f0 + h1 * f1 + 32) >> 6)
                if v > 32767:
                    v = 32767
                elif v < -32768:
                    v = -32768
                h1 = h0
                h0 = v
                append(v * volume // FULL)
    return out


def convert_piece(pack: str, piece: tuple, out: str) -> str:
    number, offset, sectors, loop, volume = piece
    with open(pack, 'rb') as f:
        f.seek(offset)
        data = f.read(sectors * SECTOR)
    if len(data) != sectors * SECTOR:
        raise ValueError(f'{pack}: piece 0x{number:02X} is cut short')
    return write_piece(data, sectors, loop, volume, out)


def extra_piece(disc: str, extra: tuple, out: str) -> str | None:
    """One of EXTRAS from the disc; None when its sectors are not the ones expected."""
    number, name, lba, sectors, loop, volume, sha = extra
    try:
        data = disc_sectors(Path(disc), lba, sectors)
    except (OSError, ValueError, StopIteration, IndexError):
        return None
    if hashlib.sha256(data).hexdigest() != sha:
        return None
    return write_piece(data, sectors, loop, volume, out)


def write_piece(data: bytes, sectors: int, loop: int, volume: int, out: str) -> str:
    left = decode(b''.join(data[s:s + HALF] for s in range(0, len(data), SECTOR)), volume)
    right = decode(b''.join(data[s + HALF:s + SECTOR] for s in range(0, len(data), SECTOR)), volume)
    frames = array('h', bytes(4 * len(left)))
    frames[0::2] = left
    frames[1::2] = right
    if sys.byteorder != 'little':
        frames.byteswap()
    path = Path(out)
    temp = path.with_name(path.name + '.new')
    with temp.open('wb') as f:
        f.write(b'T3BGM001' + struct.pack('<III', 44100, sectors * SECTOR_FRAMES, loop * SECTOR_FRAMES))
        frames.tofile(f)
    temp.replace(path)
    return path.name


def name_of(number: int) -> str:
    return f'ps2-{STAGES.get(number, f"{number:02x}")}.bgm'


def convert(pack: Path | None, out: Path, disc: Path | None = None, say=print) -> list[str]:
    """Every piece of pack as out/ps2-<stage>.bgm, and EXTRAS from disc as
    out/ps2-<name>.bgm; the names written."""
    out.mkdir(parents=True, exist_ok=True)
    jobs = pieces(pack) if pack else []
    with ProcessPoolExecutor() as pool:
        names = pool.map(convert_piece, [str(pack)] * len(jobs), jobs, [str(out / name_of(p[0])) for p in jobs])
        extras = pool.map(extra_piece, [str(disc)] * len(EXTRAS), EXTRAS,
                          [str(out / f'ps2-{e[1]}.bgm') for e in EXTRAS]) if disc else []
        names, extras = list(names), list(extras)
    if disc and not all(extras):
        say(f'TTT PS2 music: {disc.name} is not the TTT USA disc (SLUS-20001, 1.00) expected, its other music left out')
    names += [n for n in extras if n]
    say(f'TTT PS2 music: {len(names)} piece(s) in {out}')
    return names


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=EXPANDED, help='the Expanded folder (default: the one this add-on is in)')
    root = ap.parse_args(argv).root
    pack, disc = find_pack(root), find_disc(root)
    if not pack and not disc:
        print('TTT PS2 music: no mods/ttt-ps2-music/ps2-music.ttp and no TTT PS2 disc (the Expanded setup takes it)')
        return 0
    convert(pack, root / OUT, disc)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
