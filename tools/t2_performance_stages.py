#!/usr/bin/env python3
"""Tekken 2 Content's Stages, Performance: the Remake's packs with less on
them, for slower computers (issue #13; the user's, 2026-11-21).

    python tools/t2_performance_stages.py [--root <the Expanded folder>] [--disc <Tekken 2 disc>] [--tekken3 <zip>]

From addons/tmst/workspace/t2-stages/remake/stageNN.ast (and the
stageNNw.ast of Unknown's fights) into .../performance/ under the same
names; the runtime (src/tekken2/tmst_tekken2_stages.c) draws them when
GAME OPTION's TEKKEN 2 STAGES says PERFORMANCE. The Remake's packs are only
read. What each stage leaves out is tools/data/t2_performance_stages.json,
measured (docs: handoff_issue13): the grass (its 3D tufts round the
fighters, the dearest of the extras), the birds and other animals, Hong
Kong's helicopter, Akihabara's ship, some of the particles, some pieces of
the scenery. A stage the recipe does not name, and a Remake pack that is
the Classic's copy (no tekken3.zip), is copied as it is.

Pieces of a stage's scenery ("scenery" in the recipe: London's phone box,
...): the stage built again by the Remake's generator
(tools/t2_remake_stages.py convert(), the same disc, tekken3.zip, pictures
and storm), every builder of t2_remake_stage.scenery() run as before (the
random numbers drawn alike: the other pieces as they were) but the faces
of the pieces left out dropped (Cuts); of that pack only its tiles (TILE,
T2TL) are taken, its other sections checked to be the Remake's.
"""
from __future__ import annotations

import argparse
import ast
import collections
import inspect
import json
import re
import shutil
import struct
import sys
import textwrap
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON / 'tools'))
import tekken2_stages as classic  # noqa: E402

# Increase whenever the packs this writes change: the setup makes them again.
VERSION = 3
REMAKE = 'remake'
PERFORMANCE = 'performance'
RECIPE = ADDON / 'tools/data/t2_performance_stages.json'
# "objects": "off": the models, their rotations, the machine and its own
# section (src/tekken2/tmst_tekken2_extras.c objects_load(): without T2OA
# there are none)
OBJECTS = (b'T2OB', b'T2RT', b'T2OA', b'EAGL', b'GRAS', b'SHIP', b'GULL')
# the scenery's sections: what a stage built again without some pieces gives
# (COLL: the Church's walls, its pieces' as they are left)
TILES = (b'TILE', b'T2TL', b'COLL')
# t2_remake_stage.church_walls(): each builder's pieces in the scenery's spec
WALLS = {'pillar': 'pillars', 'pew_rows': 'pew_blocks', 'candelabrum': 'candelabras', 'altar': 'altar',
         'votive_rack': 'votive', 'thurible': 'thurible'}
BUILD = 'performance-build'               # out/: the stages built again (removed after)
# the sections whose first int16 is how many there are
COUNTED = (b'FTHR', b'FFLY', b'RAIN')
# T2OB: u32 models, each u16 vertices, u16 primitives, the vertices (8 bytes)
# and primitives (PRIM bytes); u32 drawing calls (8 bytes each)
PRIM = 40


def recipe(path: Path = RECIPE) -> dict:
    """{stage: its rule} (the recipe's "stages")."""
    data = json.loads(path.read_text(encoding='utf-8'))
    return {int(k): v for k, v in data['stages'].items()}


def sections(pack: bytes) -> list:
    n = struct.unpack_from('<I', pack, 8)[0]
    out = []
    for i in range(n):
        sid, off, size = struct.unpack_from('<4s2I', pack, 16 + 12 * i)
        out.append((sid, pack[off:off + size]))
    return out


def packed(head: bytes, parts: list) -> bytes:
    """A format 3 pack of parts after head's first 8 bytes (magic, format,
    number), as tools/t2_remake_stages.py rewritten() writes it."""
    table, body, off = b'', b'', 16 + 12 * len(parts)
    for sid, data in parts:
        data = data + bytes(-len(data) % 4)
        table += struct.pack('<4s2I', sid, off, len(data))
        body += data
        off += len(data)
    return head[:8] + struct.pack('<2I', len(parts), off) + table + body


def layouts_at(objs: bytes) -> int:
    """Where T2OB's count of drawing calls is."""
    p = 4
    for _ in range(struct.unpack_from('<I', objs)[0]):
        nv, np_ = struct.unpack_from('<2H', objs, p)
        p += 4 + 8 * nv + PRIM * np_
    if p + 4 > len(objs):
        raise ValueError('T2OB: shorter than its models')
    return p


def first_layouts(objs: bytes, n: int) -> bytes:
    """T2OB with only its first n drawing calls (the birds come first, then
    the critters, then the grass: tmst_tekken2_extras.c objects())."""
    p = layouts_at(objs)
    have = struct.unpack_from('<I', objs, p)[0]
    if n >= have:
        return objs
    return objs[:p] + struct.pack('<I', n) + objs[p + 4:p + 4 + 8 * n]


def lowered(data: bytes, n: int) -> bytes:
    have = struct.unpack_from('<h', data)[0]
    return struct.pack('<h', min(have, n)) + data[2:] if have > n else data


def performance(pack: bytes, rule: dict | None, tiles: dict | None = None) -> bytes:
    """The pack with what the rule leaves out (the pack itself without one);
    tiles {section: data}: its scenery's sections instead (scenery_tiles())."""
    if not rule:
        return pack
    tiles = tiles or {}
    drop = {s.encode() for s in rule.get('drop', [])}
    if rule.get('objects') == 'off':
        drop |= set(OBJECTS)
    counts = {k.encode(): v for k, v in rule.get('counts', {}).items()}
    parts = []
    for sid, data in sections(pack):
        if sid in drop:
            continue
        if sid in counts and sid in COUNTED and len(data) >= 2:
            data = lowered(data, counts[sid])
        if sid == b'T2OB' and 'layouts' in rule:
            data = first_layouts(data, rule['layouts'])
        parts.append((sid, tiles.get(sid, data)))
    return packed(pack, parts)


def stage_of(name: str) -> int | None:
    m = re.fullmatch(r'stage(\d\d)w?\.ast', name)
    return int(m.group(1)) if m else None


class Cuts:
    """While on, t2_remake_stage.scenery()'s builders (the calls "faces +=
    builder(...)" in it) leave out the faces rule says: "drop" [names],
    "keep" {name: [the calls kept, counted from 0 in each scenery()]},
    "every" {name: n}: every n-th call, or of a call's tufts ("filler")
    every n-th; "faces" {name: [pictures]}: its faces of these pictures
    (the Great Wall's beacon tower's fire and smoke). Every builder still
    runs whole: the random numbers it draws, and so every other piece, as
    without. church_walls() (COLL) gets the spec without the pieces left
    out (WALLS)."""

    def __init__(self, gen):
        self.gen, self.rule, self.saved, self.calls = gen, {}, {}, collections.Counter()

    @staticmethod
    def builders(gen) -> list[str]:
        names = set()
        for node in ast.walk(ast.parse(textwrap.dedent(inspect.getsource(gen.scenery)))):
            if isinstance(node, ast.AugAssign) and isinstance(node.value, ast.Call) \
                    and isinstance(node.value.func, ast.Name):
                names.add(node.value.func.id)
        return sorted(n for n in names if inspect.isfunction(getattr(gen, n, None)))

    def kept(self, name: str, faces: list) -> list:
        i = self.calls[name]
        self.calls[name] += 1
        r = self.rule
        if name in r.get('drop', []):
            return []
        if name in r.get('keep', {}):
            return faces if i in r['keep'][name] else []
        if name in r.get('every', {}):
            n = r['every'][name]
            if any('filler' in f for f in faces):
                return [f for f in faces if 'filler' not in f or f['filler'][1] % n == 0]
            return faces if i % n == 0 else []
        if name in r.get('faces', {}):
            return [f for f in faces if f.get('tex') not in r['faces'][name]]
        return faces

    def walls_spec(self, spec: dict) -> dict:
        """The scenery's spec as church_walls() is to see it: the pieces left out gone."""
        spec, r = dict(spec), self.rule
        for name, key in WALLS.items():
            if key not in spec:
                continue
            if name in r.get('drop', []):
                del spec[key]
            elif isinstance(spec[key], list) and name in r.get('keep', {}):
                spec[key] = [v for i, v in enumerate(spec[key]) if i in r['keep'][name]]
            elif isinstance(spec[key], list) and name in r.get('every', {}):
                spec[key] = spec[key][::r['every'][name]]
        return spec

    def __enter__(self):
        gen = self.gen
        for name in self.builders(gen) + ['scenery'] + (['church_walls'] if hasattr(gen, 'church_walls') else []):
            self.saved[name] = getattr(gen, name)

        def wrap(name, fn):
            return lambda *a, **k: self.kept(name, fn(*a, **k))

        def scenery(*a, **k):
            self.calls.clear()
            return self.saved['scenery'](*a, **k)
        for name, fn in self.saved.items():
            if name not in ('scenery', 'church_walls'):
                setattr(gen, name, wrap(name, fn))
        gen.scenery = scenery
        if 'church_walls' in self.saved:
            gen.church_walls = lambda spec, *a, **k: self.saved['church_walls'](self.walls_spec(spec), *a, **k)
        return self

    def __exit__(self, *exc):
        for name, fn in self.saved.items():
            setattr(self.gen, name, fn)


def scenery_tiles(out: Path, disc: Path, tekken3: Path, storm: Path | None, say=print,
                  rules: dict | None = None) -> dict:
    """{pack name: {TILE/T2TL: data}}: the stages whose rule has "scenery"
    built again by the Remake's generator with Cuts, into out/BUILD (removed
    after); a pack whose other sections are not the Remake's is left out
    (said)."""
    import t2_remake_stages as rs
    rules = recipe() if rules is None else rules
    stages = sorted(s for s, r in rules.items() if r.get('scenery'))
    if not stages:
        return {}
    folder = out / BUILD
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    saved = rs.REMAKE
    rs.REMAKE = BUILD
    try:
        with Cuts(rs.gen) as cuts:
            for stage in stages:
                cuts.rule = rules[stage]['scenery']
                rs.convert(disc, out, tekken3, say=lambda *_: None, only=[stage], storm=storm)
        found = {}
        for path in sorted(folder.glob('stage*.ast')):
            theirs = out / REMAKE / path.name
            if not theirs.is_file():
                continue
            mine = sections(path.read_bytes())
            if [p for p in mine if p[0] not in TILES] != [p for p in sections(theirs.read_bytes()) if p[0] not in TILES]:
                say(f'Tekken 2 Performance: {path.name} built again is not the Remake’s but in its scenery: '
                    'the Remake’s scenery kept')
                continue
            found[path.name] = {sid: d for sid, d in mine if sid in TILES}
        return found
    finally:
        rs.REMAKE = saved
        shutil.rmtree(folder, ignore_errors=True)


def convert(out: Path, say=print, rules: dict | None = None, tiles: dict | None = None) -> list[str]:
    """Every pack of out/remake into out/performance (the folder's others
    gone); tiles: scenery_tiles()'s (a stage whose rule has "scenery" and
    none there keeps the Remake's, said). Returns their names relative to
    out."""
    rules = recipe() if rules is None else rules
    tiles = tiles or {}
    src, dst = out / REMAKE, out / PERFORMANCE
    dst.mkdir(parents=True, exist_ok=True)
    names, made = [], set()
    for path in sorted(src.glob('stage*.ast')):
        stage = stage_of(path.name)
        if stage is None:
            continue
        pack = path.read_bytes()
        rule = rules.get(stage)
        if rule and rule.get('scenery') and path.name not in tiles:
            say(f'Tekken 2 stage {stage:2} {classic.NAMES[stage]}: Performance {path.name}: '
                'its scenery the Remake’s (not built again)')
        result = performance(pack, rule, tiles.get(path.name))
        if result == pack:
            shutil.copyfile(path, dst / path.name)
        else:
            (dst / path.name).write_bytes(result)
            say(f'Tekken 2 stage {stage:2} {classic.NAMES[stage]}: Performance {path.name}, '
                f'{len(pack)} -> {len(result)} bytes ({rule.get("why", "")})')
        names.append(f'{PERFORMANCE}/{path.name}')
        made.add(path.name)
    for old in dst.glob('*.ast'):
        if old.name not in made:
            old.unlink()
    return names


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=classic.EXPANDED, help='the Expanded folder')
    ap.add_argument('--disc', type=Path, help='the Tekken 2 disc (default: the one the setup remembered)')
    ap.add_argument('--tekken3', type=Path, help="tekken3.zip (default: the Expanded setup's)")
    args = ap.parse_args(argv)
    out = args.root / classic.OUT
    if not (out / REMAKE).is_dir():
        print(f'no Remake packs in {out / REMAKE} (run the setup)')
        return 1
    import t2_remake_stages as rs
    disc = args.disc or classic.find_disc(ADDON / 'workspace')
    tekken3 = args.tekken3 or rs.tekken3_zip()
    tiles = scenery_tiles(out, disc, tekken3, rs.storm_pack(args.root)) if disc and tekken3 else {}
    convert(out, tiles=tiles)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
