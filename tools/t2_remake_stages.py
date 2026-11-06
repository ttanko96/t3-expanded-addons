#!/usr/bin/env python3
"""Tekken 2 Content's Stages, Remake: the 23 packs of
addons/tmst/workspace/t2-stages/remake/stageNN.ast (as the Classic's
classic/: NN Tekken 2's stage; the runtime, src/tekken2/tmst_tekken2_stages.c,
draws them when GAME OPTION's TEKKEN 2 STAGES says REMAKE).

    python tools/t2_remake_stages.py [--disc <Tekken 2 disc>] [--tekken3 <tekken3.zip>] [--stage N ...]

- the stages with a Remake (tools/data/t2_remake_stages.json "arcade"; the
  old fork's recipes, tools/t2_remake_stage.py builds them): from the
  Tekken 2 disc and tekken3.zip (the Tekken 3 arcade stage a recipe takes its
  floor and its pictures from); the panorama the user's background
  (users_picture(), from the patches tools/t2_remake_patch.py opens with the
  disc), else Tekken 2's own (classic_picture()), in the Classic's frame
  (t2_remake_stage.classic_cfg(): as high on the floor as the Classic); the
  high half's pictures never on Unknown's wolf nor TTT Hitspark's fixed
  cells (RESERVED); the wolf's area taken only when a pack does not fit
  without it, never for the stages of "wolf_cut", whose background loses
  as few top cell rows as it takes instead (cut_fitted());
- the others (Cambodian Wetlands, Athens, Nowhere Desert, Hills, Eternal
  Darkness: no Remake yet, the user's, 2026-11-03), and every stage without
  tekken3.zip: the Classic's pack as it is.

Sections: the engine's (VRAM, TILE, FARB, FLOR, TRIG, ANIM), the old fork's
that the Expanded's engine does not read (DUST, GULL, FTHR, ...: the
add-on's runtime, later phases), its objects under the add-on's names (OBJS
T2OB, ROTT T2RT, OBJA T2OA: the engine's T3 path would take them as Tekken
3's stage 3 or 11 objects, and turns a pack of more than 4 models away), and
the add-on's own as the Classic's (T2FX the fighters' light, T2MR Mirror
Darkness's mirror). Nothing of the disc goes into git.
"""
from __future__ import annotations

import argparse
import copy
import json
import math
import shutil
import struct
import sys
from pathlib import Path

import numpy as np

ADDON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON / 'tools'))
import t2_remake_stage as gen  # noqa: E402
import tekken2_stages as classic  # noqa: E402
from disc_image import open_disc  # noqa: E402

# Increase whenever the packs this writes change: the setup makes them again.
VERSION = 9
REMAKE = 'remake'
PICTURES = 'pictures'                       # tools/t2_remake_patch.py's (in t2-stages/, never staged)
RENAMED = {b'OBJS': b'T2OB', b'ROTT': b'T2RT', b'OBJA': b'T2OA'}
# tools/tekken2_stages.py's: TTT Hitspark's fixed cells (never), Unknown's
# wolf (only when the pack does not fit without it: R3, the user's, 2026-11-03)
RESERVED = list(classic.HIT_EFFECT_CELLS)
WOLF = classic.WOLF
# The stages whose background loses top cell rows rather than its pictures
# go on the wolf (tools/data/t2_remake_stages.json "wolf_cut", the user's,
# 2026-11-05: cut_fitted()); a stage out of the list is built as before.
WOLF_CUT = frozenset(gen.DATA.get('wolf_cut', []))
MAX_CUT = 4
# What the wolf's pack (the Expanded's tools/ttt1/spirit.py,
# Unknown-TTT1-wolf.glow, measured 2026-11-05) draws in WOLF: all but x
# 768..1023 at y 708..767 and x 676..767 at y 740..767, where the Expanded's
# pack.Relocation may put palettes.
WOLF_DRAWN = ((640, 512, 384, 196), (640, 708, 128, 32), (640, 740, 36, 28))
# The add-on's letters (src/core/tmst_overlay.c place_atlas(): a sheet of
# 192 x 61 (the UI font, Fira Sans, since 2026-11-06; FONT_ROOM's 64 rows
# hold it) in one row of pages, sought from the half's bottom right, y down 8
# and x left 64 at a time, off the stage's rectangles, TTT Hitspark's cells
# and, while Unknown fights, her whole wolf); FONT_ROOM the slots kept free
# for it when a "wolf_cut" stage leaves no place otherwise.
FONT_W, FONT_H = 192, 61
FONT_ROOM = (1024 - FONT_W, 960, FONT_W, 64)


def owners() -> dict:
    """{Tekken 2 stage: the recipe's key} of the stages with a Remake."""
    return {gen.stage_of(k): k for k in gen.DATA['arcade']}


def classic_picture(st: dict) -> np.ndarray:
    """Tekken 2's panorama in the Classic's frame (rows x 64 tall, a place 64
    wide, each of its rows twice, as the Classic's FARB draws it): RGB,
    float32. The picture the recipes' places (classic_cfg(), "frames") are
    measured in."""
    rows = []
    for row in st['pano']:
        cells = []
        for idx, colours in row:
            pal = np.array([(c & 31, c >> 5 & 31, c >> 10 & 31) for c in colours], np.float32) * 8
            cells.append(np.repeat(pal[np.frombuffer(idx, np.uint8).reshape(classic.CELL_H, -1)], 2, axis=0))
        rows.append(np.concatenate(cells, axis=1))
    return np.concatenate(rows, axis=0)


def taller(cfg: dict, wo: int, ho: int, more: int) -> np.ndarray:
    """The old in-game panorama with more rows (a whole number of cells) on
    top, at its own scale: its recipe's crop reaching higher into the
    user's picture (its "grade" stretched to keep its colours where they
    were). Past the picture's top the old way (its first row on, smoothed,
    going to "sky", else to its top rows' colour)."""
    from PIL import Image
    c = copy.deepcopy(cfg)
    x0, y0, x1, y1 = c.get('crop') or (0, 0, *Image.open(gen.hd(c['panorama'])).size)
    c['crop'] = [x0, y0 - round(more * (y1 - y0) / ho), x1, y1]
    c['rows'], c['columns'] = (ho + more) // 64, wo // 64
    if c['crop'][1] < 0 and not c.get('sky'):
        top = np.asarray(Image.open(gen.hd(c['panorama'])).convert('RGB'), np.float32)[max(0, y0):max(0, y0) + 8]
        c['sky'] = [round(v) for v in top.reshape(-1, 3).mean(0)]
    if c.get('grade'):
        g = c['grade'] = dict(c['grade'])
        g['horizon_row'] = (g['horizon_row'] * ho + more) / (ho + more)
        if 'glow_width' in g:
            g['glow_width'] = g['glow_width'] * ho / (ho + more)
    return gen.panorama_picture(c, None, None)


def users_picture(owner: str, size: tuple, pictures: Path, repeat: int = 2) -> tuple[np.ndarray, int] | None:
    """The user's background (tools/t2_remake_patch.py: pictures/) in the
    Classic's frame, size (w, h) classic_picture()'s: the old in-game
    panorama made from it by its recipe as before (crop, erase, squeeze,
    grade, seam, roll: t2_remake_stage.panorama_picture()), then brought by
    the stage's "frame" to where Tekken 2's own lies (x times w / the old
    width, turned by "shift"; y to a * y + b). One cell row of it above
    (ext 64) when the old picture reaches that far up, else none. Rows past
    the old picture's top or foot its first or last row on. None: no
    picture there (the caller takes Tekken 2's own)."""
    from PIL import Image
    cfg, frame = gen.DATA['arcade'][owner], gen.DATA['frames'].get(owner)
    if not cfg.get('panorama') or not frame or not (pictures / cfg['panorama']).is_file():
        return None
    gen.PICTURES[:] = [str(pictures)]
    old = gen.panorama_picture(cfg, None, None)
    ho, wo = old.shape[:2]
    if [wo, ho] != frame['old']:
        raise ValueError(f'{owner}: the old panorama is {wo} x {ho}, its frame was measured on {frame["old"]}')
    w, h = size
    a, b = frame['y']
    sx = w / wo / (2 / repeat)
    ext = 64 if b <= -64 else 0
    above = ext + round(b)                  # the new rows above the old picture's top
    if above > 0:
        more = taller(cfg, wo, ho, 64 * math.ceil(above / (64 * a)))
        b -= a * (more.shape[0] - ho)
        old = np.concatenate([more[:more.shape[0] - ho], old])
    big = np.asarray(Image.fromarray(np.clip(old, 0, 255).astype(np.uint8))
                     .resize((round(wo * sx), round(old.shape[0] * a)), Image.LANCZOS), np.float32)
    rows = np.clip(np.arange(h + ext) - ext - round(b), 0, big.shape[0] - 1)
    cols = np.arange(w) % big.shape[1]
    turned = big[rows][:, cols]
    return np.roll(turned, frame.get('shift', 0), axis=1), ext


def tekken3_zip() -> Path | None:
    """The Expanded setup's tekken3.zip (.setup/last-inputs.json), if it is still there."""
    try:
        path = json.loads((gen.ROOT / '.setup/last-inputs.json').read_text(encoding='utf-8')).get('tekken3')
    except (OSError, ValueError):
        return None
    return Path(path) if path and Path(path).is_file() else None


def sections(pack: bytes) -> list:
    n = struct.unpack_from('<I', pack, 8)[0]
    out = []
    for i in range(n):
        sid, off, size = struct.unpack_from('<4s2I', pack, 16 + 12 * i)
        out.append((sid, pack[off:off + size]))
    return out


ANIMATIONS = (b'ANIM', b'ANI2', b'ANI3', b'ANI4')


def frames_out(parts: list) -> list:
    """The animations' frames out of the VRAM (t2_remake_stage.Vram.ram():
    rectangles from y 1024, which the engine could not upload) into T2AN:
    u32 count, then each: the section it was (4 bytes), u16 x, y, w, h (its
    shown place, halfwords), period, entries, frames, mode (ANIx's; 0
    ANIM's), u16 per entry its frame (padded to 4 bytes), then the frames'
    pixels (w x h each). Its section goes; the runtime
    (src/tekken2/tmst_tekken2_stages.c) uploads the frame in turn, as the
    engine's ANIM does from the VRAM."""
    vram = dict(parts)[b'VRAM']
    n, o = struct.unpack_from('<I', vram)[0], 4
    kept, ram = [], {}
    for _ in range(n):
        x, y, w, h = struct.unpack_from('<4H', vram, o)
        rect = vram[o:o + 8 + 2 * w * h]
        o += 8 + 2 * w * h
        if y >= 1024:
            ram[x, y] = np.frombuffer(rect[8:], '<u2').reshape(h, w)
        else:
            kept.append(rect)
    if not ram:
        return parts
    out, anims = [], []
    for sid, data in parts:
        if sid not in ANIMATIONS:
            out.append((sid, data))
            continue
        x, y, w, h, period, count = struct.unpack_from('<6H', data)
        places = [struct.unpack_from('<2H', data, 12 + 4 * i) for i in range(count)]
        if not any(fy >= 1024 for _, fy in places):
            out.append((sid, data))
            continue
        mode = struct.unpack_from('<H', data, 12 + 4 * count)[0] if sid != b'ANIM' and len(data) >= 14 + 4 * count else 0
        unique = list(dict.fromkeys(places))
        pixels = b''
        for fx, fy in unique:
            frame = np.zeros((h, w), np.uint16)
            for k in range(0, w, 16):
                frame[:, k:k + 16] = ram[fx + k, fy][:h]
            pixels += frame.astype('<u2').tobytes()
        order = struct.pack(f'<{count}H', *(unique.index(p) for p in places))
        anims.append(sid + struct.pack('<8H', x, y, w, h, period, count, len(unique), mode)
                     + order + bytes(-len(order) % 4) + pixels)
    out = [(b'VRAM', struct.pack('<I', len(kept)) + b''.join(kept)) if sid == b'VRAM' else (sid, data)
           for sid, data in out]
    return out + [(b'T2AN', struct.pack('<I', len(anims)) + b''.join(anims))]


# The floor's cells under the fighters' shadows. The engine links a floor
# cell at main rank 1021 - FLOR's rank of it (its 100 bytes after the cells
# and the halves, 8 + 4000 in); Tekken 3 puts the fighters' shadows at rank
# 1007. The old fork's floors carry a Tekken 3 arcade stage's ranks (up to
# 33): a cell of more than 13 was drawn after the shadow, over it (the user:
# "the shadow is swallowed in a square, further on it shows again",
# 2026-11-05; Arizona 49 cells of 100). Its ranks capped at 9 (rank 1012):
# their order among themselves kept, all before the tiles (1010), the shadows
# and what walks on the floor (the critters, 1005) over them.
FLOOR_RANKS_AT, SHADOW_RANK = 8 + 4000, 1007
# 1011 above them: the Church's window light (T2GL, the runtime's glass_draw),
# under the tiles' 1010
FLOOR_RANK_MAX = 1021 - 1012


def floor_under_shadows(flor: bytes) -> bytes:
    """FLOR with no cell drawn after the fighters' shadows."""
    ranks = bytes(min(b, FLOOR_RANK_MAX) for b in flor[FLOOR_RANKS_AT:FLOOR_RANKS_AT + 100])
    return flor[:FLOOR_RANKS_AT] + ranks + flor[FLOOR_RANKS_AT + 100:]


def own_tiles(parts: list) -> list:
    """A stage fixed to the floor (FIXW: the Church): its tiles drawn by the
    add-on (src/tekken2/tmst_tekken2_extras.c own_tiles_draw), not by the
    engine: T2TL their copy, TILE empty (its tile size, no tiles). The
    engine's cone drops every tile as near to the camera as its grid's
    centre; fixed, the stage's centre is not where the fighters are, and the
    scenery near them went as they came to it (the user: "they may go up to
    the objects, only these must not vanish", 2026-11-05)."""
    ids = [sid for sid, _ in parts]
    if b'FIXW' not in ids or b'TILE' not in ids:
        return parts
    out = []
    for sid, data in parts:
        if sid == b'TILE':
            out.append((b'TILE', data[:4] + struct.pack('<I', 0)))
            out.append((b'T2TL', data))
        else:
            out.append((sid, data))
    return out


def rewritten(pack: bytes, extra: list) -> bytes:
    """The pack with its animations' frames out of the VRAM (frames_out()),
    its objects' sections under the add-on's names, its floor under the
    fighters' shadows (floor_under_shadows()) and extra added."""
    parts = [(RENAMED.get(sid, sid), floor_under_shadows(data) if sid == b'FLOR' else data)
             for sid, data in frames_out(sections(pack))] + list(extra)
    parts = own_tiles(parts)
    table, body, off = b'', b'', 16 + 12 * len(parts)
    for sid, data in parts:
        data = data + bytes(-len(data) % 4)
        table += struct.pack('<4s2I', sid, off, len(data))
        body += data
        off += len(data)
    return pack[:8] + struct.pack('<2I', len(parts), off) + table + body


# The fighters' light (T2LT), the Classic's rule (tools/tekken2_stages.py):
# our fights are Tekken 3's arena 9's, whose light is dim and orange; every
# Remake stage takes arena 8's (Tekken 3's daylight: back 0x898, a white
# light) but the dark ones, which keep arena 9's and their FDIM (D15 b, the
# user's, 2026-11-05): Church, Pagoda, Serene Forest, Taj Mahal, Mirror
# Darkness (and Eternal Darkness, the Classic's for now).
DAY_ARENA = classic.DAY_ARENA
DARK_LIGHT = classic.DARK_LIGHT


def ours(stage: int, cfg: dict) -> list:
    """The add-on's sections, as the Classic writes them: T2FX the fighters'
    light (the recipe's "fighter_light", else as the game sets it; Mirror
    Darkness dark as the Classic's), T2LT the arena whose light the fighters
    take (DAY_ARENA, not on DARK_LIGHT), T2MR the mirror."""
    lay = classic.layout(stage)
    dim = round(0x80 * cfg['fighter_light']) if cfg.get('fighter_light') else \
        lay.get('dim', classic.NO_DIM) if cfg.get('night') else classic.NO_DIM
    out = [(b'T2FX', struct.pack('<2H', dim, 0))]
    if stage not in DARK_LIGHT:
        out.append((b'T2LT', struct.pack('<2H', DAY_ARENA, 0)))
    if lay.get('mirror'):
        out.append((b'T2MR', struct.pack('<6i', *lay['mirror'])))
    return out


def high_cells(pack: bytes) -> int:
    """The high half's 16 x 64 cells the pack's VRAM rectangles touch."""
    data = dict(sections(pack))[b'VRAM']
    n, o, used = struct.unpack_from('<I', data)[0], 4, set()
    for _ in range(n):
        x, y, w, h = struct.unpack_from('<4H', data, o)
        o += 8 + 2 * w * h
        used |= {(cx, cy) for cx in range(x // 16, (x + w + 15) // 16) for cy in range(max(y, 512) // 64, (y + h + 63) // 64)
                 if cy >= 8}
    return len(used)


# The storm the lightning's flashes and thunder come from: the Expanded's TTT
# stage 4 (Hwoarang's temple; TTT1 Stages' pack, format 4, STRM and THND),
# the same storm (0x8015C2A8) the old fork took from its TTT1 import
# ("like": workspace/ttt1-import/stages/arcade-baek.ast).
STORM_PACKS = ['build-release/mods/ttt-stages/stage04.ast', 'mods/ttt-stages/stage04.ast',
               'workspace/ttt-stages/pack/stage04.ast']
LIGHT_FRAMES = 48                       # the flash's light on the fighters fades over these (the old LGHT's)
LIKE_NAME = 'storm-like.ast'            # out/: the "like" pack made from it (not staged)


def storm_pack(root: Path | None = None) -> Path | None:
    """The Expanded's TTT stage 4 pack, if its setup made one."""
    for rel in STORM_PACKS:
        p = (root or gen.ROOT) / rel
        if p.is_file():
            return p
    return None


def ttt_sections(pack: bytes) -> dict:
    """A format 4 pack's sections (its table from byte 24)."""
    magic, version, _, n = struct.unpack_from('<4s2HI', pack)
    if magic != b'ASTG' or version != 4:
        raise ValueError('not a TTT stage pack')
    out = {}
    for i in range(n):
        sid, off, size = struct.unpack_from('<4s2I', pack, 24 + 12 * i)
        out[sid] = pack[off:off + size]
    return out


def storm_like(ttt: bytes) -> dict:
    """{LGHT, THND} as the old fork's runtime reads them (its TTT1 pack's),
    from a TTT stage pack's STRM and THND:
    LGHT: s16 thresholds x 5; u16 forced frames x 2; u8 their counters x 2;
      u8 patterns, 0; u8 lengths, u8 next (4 then 5, else 0xFF); the
      patterns (1 the lit sky, 0 the plain one); padded; u16 lights, frames
      the light fades over; u32 rgb per flash type.
    THND: u16 sounds, u16 flash types; u8 sound per type (0xFF none),
      padded; per sound u32 frames, s16 L, R at 44 100 Hz (the TTT pack's
      mono on both sides, as the Expanded plays it)."""
    sec = ttt_sections(ttt)
    s = sec[b'STRM']
    thresholds = struct.unpack_from('<5h', s)
    colours = [s[12 + 4 * k:15 + 4 * k] for k in range(6)]
    seqs, p = [], 36
    for _ in range(6):
        n = s[p]
        seqs.append([x - 256 if x > 127 else x for x in s[p + 1:p + 1 + n]])
        p += 1 + n
    tail = struct.unpack_from('<14H', s, len(s) - 28)
    sounds = [tail[2 * k] for k in range(5)]                    # (sound, voice) per flash type 0..4
    forced = [(tail[10], tail[11]), (tail[12], tail[13])]       # (idle frame, counter)
    pats = [[v for v in q if v != -1] for q in seqs]
    lg = struct.pack('<5h', *thresholds) + struct.pack('<2H', *(f for f, _ in forced)) \
        + bytes(c for _, c in forced) + bytes([len(pats), 0]) + bytes(len(q) for q in pats) \
        + bytes(5 if k == 4 else 0xFF for k in range(len(pats))) + b''.join(bytes(q) for q in pats)
    lg += bytes(-len(lg) % 4) + struct.pack('<2H', len(colours), LIGHT_FRAMES) \
        + b''.join(struct.pack('<I', c[0] | c[1] << 8 | c[2] << 16) for c in colours)
    th = sec[b'THND']
    have = {}
    for i in range(struct.unpack_from('<I', th)[0]):
        sound, _, frames, off = struct.unpack_from('<2H2I', th, 4 + 12 * i)
        have[sound] = np.frombuffer(th, '<i2', frames, off)
    order = list(dict.fromkeys(x for x in sounds if x in have))
    table = bytes(order.index(x) if x in order else 0xFF for x in sounds) + b'\xff'      # type 5: none
    thnd = struct.pack('<2H', len(order), len(table)) + table + bytes(-len(table) % 4)
    for x in order:
        mono = have[x]
        thnd += struct.pack('<I', len(mono)) + np.repeat(mono, 2).astype('<i2').tobytes()
    return {b'LGHT': lg, b'THND': thnd}


def write_like(path: Path, parts: dict) -> None:
    """A format 3 pack holding only the given sections (the generator's "like")."""
    body, table, off = b'', b'', 16 + 12 * len(parts)
    for sid, data in parts.items():
        data = data + bytes(-len(data) % 4)
        table += struct.pack('<4s2I', sid, off, len(data))
        body += data
        off += len(data)
    path.write_bytes(b'ASTG' + struct.pack('<2H2I', 3, 0, len(parts), off) + table + body)


def fitted(owner: str, src, tekken3: Path, temp: Path, cfg: dict) -> tuple[dict, str]:
    """gen.build() off Unknown's wolf, else (only when it does not fit
    without it) on it too: its report and "free" or "used"."""
    try:
        gen.RESERVED[:] = RESERVED + [WOLF]
        return gen.build(owner, src, tekken3, temp, cfg), 'free'
    except SystemExit as error:
        if 'do not fit' not in str(error):
            raise
    gen.RESERVED[:] = RESERVED
    return gen.build(owner, src, tekken3, temp, cfg), 'used'


def on_wolf(pack: bytes) -> int:
    """Texels of the pack's VRAM rectangles (pictures and palettes) on what
    the wolf draws (WOLF_DRAWN)."""
    data = dict(sections(pack)).get(b'VRAM', b'')
    if not data:
        return 0
    n, o, hit = struct.unpack_from('<I', data)[0], 4, 0
    for _ in range(n):
        x, y, w, h = struct.unpack_from('<4H', data, o)
        o += 8 + 2 * w * h
        for wx, wy, ww, wh in WOLF_DRAWN:
            hit += max(0, min(x + w, wx + ww) - max(x, wx)) * max(0, min(y + h, wy + wh) - max(y, wy))
    return hit


def font_place(pack: bytes) -> tuple[int, int] | None:
    """Where the add-on's letters go beside the pack while Unknown fights
    (FONT_W x FONT_H, as tmst_overlay.c place_atlas() seeks it), or None."""
    data = dict(sections(pack)).get(b'VRAM', b'')
    taken = list(classic.HIT_EFFECT_CELLS) + [WOLF]
    if data:
        n, o = struct.unpack_from('<I', data)[0], 4
        for _ in range(n):
            x, y, w, h = struct.unpack_from('<4H', data, o)
            o += 8 + 2 * w * h
            taken.append((x, y, w, h))
    for y in range(1024 - FONT_H, 511, -8):
        if (y - 512) // 256 != (y + FONT_H - 1 - 512) // 256:    # in one row of pages
            continue
        for x in range(1024 - FONT_W, -1, -64):
            if not any(x < rx + rw and rx < x + FONT_W and y < ry + rh and ry < y + FONT_H for rx, ry, rw, rh in taken):
                return x, y
    return None


def kept(cfg: dict) -> bool:
    """The row a recipe animates (its sea, pool, clouds, water) still in its
    cut picture; New York's and Hong Kong's windows from its top row on
    when they began in what was cut (a negative first row would take them
    from the foot: city_lights(), hk_lights())."""
    for spec in (cfg.get('windows'), cfg.get('lights', {}).get('windows')):
        if spec:
            spec['rows'][0] = max(0, spec['rows'][0])
    return all(cfg[k]['row'] >= 0 for k in ('sea', 'pool', 'clouds', 'water') if k in cfg)


def cutter(base: dict, picture: np.ndarray, ext: int, frame: dict, repeat: int):
    """make(cut): the recipe of the picture with its top cut cell rows off
    (gen.classic_cfg(), the rest where it was), None when they may not go."""
    def make(cut: int) -> dict | None:
        if cut >= picture.shape[0] // 64:
            return None
        cfg = gen.classic_cfg(base, picture[64 * cut:], ext - 64 * cut, frame, repeat)
        return cfg if not cut or kept(cfg) else None
    return make


def cut_fitted(owner: str, src, tekken3: Path, temp: Path, make) -> tuple[dict, str, dict, int]:
    """A stage of WOLF_CUT: gen.build() off Unknown's wolf with 0, 1, 2 ...
    (MAX_CUT at most) top cell rows of its background cut, the first that
    fits: its palettes too, none on what the wolf draws, and a place left
    for the add-on's letters while Unknown fights (font_place(); with
    FONT_ROOM kept free when not without it). Else as fitted() with none
    cut. Its report, "free" or "used", the recipe, the rows cut."""
    for cut in range(MAX_CUT + 1):
        cfg = make(cut)
        if cfg is None:
            break
        for room in ((), (FONT_ROOM,)):
            gen.RESERVED[:] = RESERVED + [WOLF, *room]
            try:
                report = gen.build(owner, src, tekken3, temp, cfg)
            except SystemExit as error:
                if 'do not fit' not in str(error):
                    raise
                break
            except ValueError as error:         # pack.Relocation: no room left for a palette run
                if 'no room for' not in str(error):
                    raise
                break
            pack = temp.read_bytes()
            if not on_wolf(pack) and font_place(pack):
                return report, 'free', cfg, cut
    cfg = make(0)
    report, wolf = fitted(owner, src, tekken3, temp, cfg)
    return report, wolf, cfg, 0


def convert(disc: Path, out: Path, tekken3: Path | None, say=print, only=None, storm: Path | None = None) -> list[str]:
    """The 23 packs into out/remake (the backgrounds from out/pictures, where
    tools/t2_remake_patch.py puts them, else Tekken 2's own; the
    lightning's flashes and thunder from storm, TTT's stage 4 pack, when
    given). Returns their names relative to out."""
    folder = out / REMAKE
    pictures = out / PICTURES
    gen.PICTURES[:] = [str(pictures)] if pictures.is_dir() else []      # the Hong Kong sign's words too
    by_stage = owners()
    names = []
    like = out / LIKE_NAME
    like.unlink(missing_ok=True)
    if storm:
        try:
            write_like(like, storm_like(storm.read_bytes()))
        except (OSError, ValueError, KeyError, struct.error) as error:
            say(f'lightning: {storm}: {error}; no flashes')
            like.unlink(missing_ok=True)
    gen.LIKE.clear()
    for cfg in gen.DATA['arcade'].values():
        if cfg.get('lightning', {}).get('like'):
            gen.LIKE[cfg['lightning']['like']] = str(like)
    with open_disc(disc) as d:
        src = classic.Source(d)
        for stage in only or range(len(classic.NAMES)):
            name = classic.pack_name(stage)
            target = folder / name
            owner = by_stage.get(stage)
            if not owner or not tekken3:
                shutil.copyfile(out / classic.CLASSIC / name, target)
                names.append(f'{REMAKE}/{name}')
                continue
            base = gen.DATA['arcade'][owner]
            temp = folder / (name + '.part')
            tries = [(lambda cut: None if cut else base, 'no')]       # Mirror Darkness: no panorama
            if not base.get('night'):
                own = classic_picture(classic.stage_data(src, stage))
                repeat = classic.layout(stage)['repeat']
                theirs = users_picture(owner, own.shape[1::-1], pictures, repeat)
                if not theirs:
                    say(f'Tekken 2 stage {stage:2} {classic.NAMES[stage]}: no Remake picture, Tekken 2’s own panorama')
                pics = ([(theirs, 'the Remake’s')] if theirs else [])
                half = own.shape[1] // 2
                if theirs and half % 64 == 0 and np.array_equal(own[:, :half], own[:, half:]):
                    # Tekken 2's own goes round twice the same (Church, Forest): the
                    # Remake's first half twice, its cells in the VRAM once
                    p, ext = theirs
                    turn = gen.DATA['frames'][owner].get('shift', 0)
                    first = np.roll(p, -turn, axis=1)[:, :half]
                    pics.append(((np.roll(np.concatenate([first, first], axis=1), turn, axis=1), ext),
                                 'the Remake’s half'))
                pics.append(((own, 0), 'Tekken 2’s own'))
                tries = [(cutter(base, p, ext, gen.DATA['frames'][owner], repeat), kind)
                         for (p, ext), kind in pics]
            for k, (make, kind) in enumerate(tries):
                try:
                    if owner in WOLF_CUT:
                        report, wolf, cfg, cut = cut_fitted(owner, src, tekken3, temp, make)
                    else:
                        cfg, cut = make(0), 0
                        report, wolf = fitted(owner, src, tekken3, temp, cfg)
                    break
                except SystemExit as error:
                    if 'do not fit' not in str(error) or k == len(tries) - 1:
                        raise
                    say(f'Tekken 2 stage {stage:2} {classic.NAMES[stage]}: {kind} picture does not fit the VRAM')
            pack = rewritten(temp.read_bytes(), ours(stage, cfg))
            temp.unlink()
            target.write_bytes(pack)
            names.append(f'{REMAKE}/{name}')
            say(f'Tekken 2 stage {stage:2} {classic.NAMES[stage]}: Remake {len(pack)} bytes, '
                f'far {report["far_cells"]}, {high_cells(pack)} high cells, the wolf’s area {wolf}, {kind} picture'
                + (f', its top {cut} cell rows cut' if cut else ''))
    return names


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--disc', type=Path, help='the Tekken 2 disc (default: the one the setup remembered)')
    ap.add_argument('--tekken3', type=Path, help="tekken3.zip (default: the Expanded setup's)")
    ap.add_argument('--root', type=Path, default=classic.EXPANDED, help='the Expanded folder')
    ap.add_argument('--stage', type=int, nargs='*', help='only these stages')
    args = ap.parse_args(argv)
    disc = args.disc or classic.find_disc(ADDON / 'workspace')
    if not disc:
        print('no Tekken 2 disc (give --disc, or run the setup)')
        return 1
    convert(disc, args.root / classic.OUT, args.tekken3 or tekken3_zip(), only=args.stage,
            storm=storm_pack(args.root))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
