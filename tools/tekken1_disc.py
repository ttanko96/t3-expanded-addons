"""The player's Tekken (1) PlayStation disc, for what the add-on imports from it
(tools/tekken1_stages.py takes its stages).

Supported, as Tekken 2's (tools/tekken2_disc.py): Tekken USA (SLUS-00006),
Europe (SCES-00005) and Japan (SLPS-00040), every version. Europe's is
tried; the others' stages are taken when their stage files read as
Europe's, their music by the program's table (tools/tekken1_music.py).
A .cue with its .bin files, a .bin (with its .cue beside it, or a
single-track one) or a .chd (tools/disc_image.py). The disc is told by the
program its SYSTEM.CNF boots.
"""
from __future__ import annotations
import re
from pathlib import Path
from disc_image import DiscError, EXTENSIONS, open_disc

VERSION = 1
REGIONS = {'SCES_000.05': 'Europe', 'SLUS_000.06': 'USA', 'SLPS_000.40': 'Japan'}
EXPECTED = ('Expected: Tekken for PlayStation, USA (SLUS-00006), Europe (SCES-00005) or Japan '
            '(SLPS-00040), as a .cue with its .bin files, a .bin or a .chd.')


def identify(path) -> dict:
    """{'path', 'region', 'serial', 'tracks'} of a supported disc; DiscError otherwise."""
    path = Path(path)
    if path.suffix.lower() not in EXTENSIONS:
        raise DiscError(f'{path.name} is not a disc image. {EXPECTED}')
    with open_disc(path) as disc:
        cnf = disc.read_file('SYSTEM.CNF')
        if cnf is None:
            raise DiscError(f'{path.name} is not a PlayStation disc. {EXPECTED}')
        m = re.search(rb'BOOT\s*=\s*cdrom:\\?(?:.*\\)?([A-Z]{4}_\d{3}\.\d{2})', cnf, re.I)
        boot = m[1].decode('ascii').upper() if m else '?'
        if boot not in REGIONS:
            raise DiscError(f'{path.name} boots {boot.replace("_", "-").replace(".", "")}, not Tekken. {EXPECTED}')
        return {'path': str(path.resolve()), 'region': REGIONS[boot],
                'serial': boot.replace('_', '-').replace('.', ''), 'tracks': len(disc.tracks)}


def search(folders) -> list[Path]:
    """Disc images in the given folders (not below) whose name says Tekken, not
    a later one (Tekken 2, 3, Tag, ...)."""
    found = []
    for folder in folders:
        try:
            files = [f for f in sorted(Path(folder).iterdir())
                     if f.suffix.lower() in EXTENSIONS and re.match(r'tekken\b(?![ _-]*(\d|ii|tag|dark))', f.stem, re.I)]
        except OSError:
            continue
        found += [f for f in files if f.suffix.lower() != '.bin'] or files
    return found
