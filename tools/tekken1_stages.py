#!/usr/bin/env python3
"""Tekken 1 Content's Stages, Classic: the 11 Tekken (1) stages as packs the
Expanded's stage engine reads (src/tekken3_arcade_stages.c, its Tekken 3
path), from the player's Tekken PlayStation disc (tools/tekken1_disc.py,
remembered by the add-on's setup in workspace/tekken1-disc.json) into
addons/tmst/workspace/t1-stages/classic/stageNN.ast (NN: Tekken's stage
number, 00..10), with pictures to check them by in t1-stages/preview/ (run
by the add-on's setup, setup/tmst_setup.py).

    python tools/tekken1_stages.py --disc <Tekken disc> [--root <expanded folder>] [--no-previews]

Classic: Tekken's own background and floor, as the PlayStation draws them,
nothing added (the Remake comes later, in t1-stages/remake).

The disc (Europe SCES-00005; USA and Japan taken when their files read alike): BG/STGnnXXX, a stage a file, the program's
names at TEKKEN.EXE 0x8001D4CC ("/bg/STG10NGT" .. "/bg/STG00GRS"). A file:
eleven u32 offsets, then
- 0, the floor: 10 x 10 tiles, 8 bytes each, as Tekken 2's (u | v << 8;
  texture page with a turn in bits 14-15, 1 a quarter clockwise; 0; CLUT);
  a tile 6000 world units a side (hrfield.c, 0x8005ADF4), the 10 x 10 again
  and again round the fight;
- 1: 4 u16 (stage * 800, 0x8001, 64, 0);
- 2, the background's cells: 3 u16 each (u | v << 8, CLUT, texture page;
  Stadium's big screen a 15-bit page: VRAM 832,128, the effect's);
- 3, the layers' rings: a byte a place (the cell, from the layer's first;
  0xFF none), each layer's own;
- 4, 5 (SPRT, 20 bytes) and 6, 7 (DR_TPAGE, 12 bytes): the program's
  primitives, two frames' worth; zeros on the disc;
- 8, the layers, 48 bytes each, drawn first to last (the first farthest;
  hrbg.c, the loop at 0x8005CAB4): +0 s32 distance (the camera's moving
  shifts it by 1/distance, sideways and up; -1 never), +4 s16 its first
  place's shift (with the globals' +8), +12 s16 its bottom below the
  horizon, +14 s16 a cell's width (32..64 pixels), +16 s16 its height, +18
  s16 places round the ring (width * places = 6144: the camera's turn, 4096,
  is 6144 pixels of a 512-pixel screen), +20 s16 sprites a frame (9, 11),
  +24 u32 its first cell, +28 u32 its ring's offset in 3, +32.. the
  primitives' offsets in 4..7;
- 9, the globals, 22 s16 (0x8005B5A4): +0 the layers ("su", < 16), +8 a
  shift added to every layer's, +16/+18 and +20/+22 the sky's colours at its
  top and bottom (RGB bytes), +32/+34 and +36/+38 the effect's, +40 the
  effect ("BG effect", 0x8005BBF4):
  0 none;
  1 Monument Valley's sunset: the sky's colours turning from the first pair
    to the effect's in 1800 frames, its horizon -14 to -12 (0x8005C8B4);
  2 King George Island's snow: 100 white flakes (TILE 1 x 1 .. 2 x 1) at
    random places, falling 1 .. 2.5 pixels a frame (16.16), a pixel left or
    right at random each frame, round a 512 x 240 field that turns and rises
    with the camera (init 0x8005D394, a frame 0x8005D60C);
  3 Stadium's big screen: the fight itself, the frame buffer (512 x 220 from
    y 20) drawn small in 32 strips into VRAM 832,128 (48 x 21) every frame,
    which its layer 2 shows (init 0x8005D094, a frame 0x8005D338);
- 10, the textures: TIMs, 4-bit, each with its own 16-colour palette
  (VRAM y 480..), images at x 512..1023 of the lower half, and the stage's
  name (x 588, y 90).

The pack (format 3, the engine's Tekken 3 path, as tools/tekken2_stages.py
makes Tekken 2's: its VRAM slots, FLOR and TRIG; its number 50 + the stage,
which the runtime makes the arena it gives the fight):
- VRAM: the layers' cells and the floor's tiles in the upper half's 64 x 64
  slots (tekken2_stages SLOTS: never on Unknown's wolf nor TTT Hitspark's
  cells), a cell taller than 64 in slots one under another, its palette
  below; the big screen's 48 x 21 halfwords (15-bit) in three slots side by
  side, black until the runtime draws the fight there;
- TILE: none; FARB: the sky's bottom colour on the whole screen, its top
  colour above (the runtime draws the gradient and the layers over it);
- FLOR: Tekken 2's recipe: a tile a cell of 2 500, turned, the near halves
  each a quarter of its tile twice as large; lit as Tekken 3's stage 0;
- TRIG: computed;
- the add-on's own (the engine reads only its own sections):
  T1LY the layers: u16 cells, layers, effect, 0; each cell u16 texture page
  (bit 11 the upper half; bits 7-8: 0 4-bit, 2 15-bit, the big screen),
  u16 CLUT, u8 u, v, 0, 0; each layer s32 distance, s16 shift (its own and the globals'), bottom,
  width, height, places, sprites, then its ring, a byte a place (the cell,
  0xFF none), to a multiple of 4;
  T1GL the globals, the disc's 22 s16 (the sky's and the effect's colours,
  the effect);
  T2FX, T2LT as Tekken 2's packs (the fighters' light: undimmed; daylight's
  arena, but at night: Chicago keeps arena 9's).

Nothing from the disc goes into git; the packs are staged beside the game as
mods/t1-stages/classic (addon.cmake TMST_STAGED). Pure Python but the floor's
near halves (numpy), a few seconds.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from array import array
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
EXPANDED = ADDON.parents[1]                 # <expanded>/addons/tmst
sys.path.insert(0, str(ADDON / 'tools'))
from disc_image import DiscError, open_disc  # noqa: E402
from tekken1_disc import REGIONS  # noqa: E402
import tekken2_stages as t2  # noqa: E402

OUT = 'addons/tmst/workspace/t1-stages'
CLASSIC, PREVIEW = 'classic', 'preview'
# Increase whenever the packs this writes change: the setup makes them again.
VERSION = 2

FILES = ('STG00GRS', 'STG01JNG', 'STG02PAL', 'STG03QJO', 'STG04DST', 'STG05ICE', 'STG06ISL',
         'STG07SAN', 'STG08ANG', 'STG09CHN', 'STG10NGT')
# The stages' own names, as their name pictures write them (section 10).
NAMES = ('Windermere', 'Kyoto', 'Acropolis', 'Stadium', 'Monument Valley', 'King George Island', 'Fiji',
         'Venezia', 'Angkor Wat', 'Szechwan', 'Chicago')
EFFECTS = {0: None, 1: 'sunset', 2: 'snow', 3: 'screen'}
SECTIONS = 11
FLOOR_N = 10
FLOOR_TILE = 6000                           # world units a tile
RING = 6144                                 # pixels round a layer's ring
LAYER = 48                                  # bytes a layer
SCREEN_W, SCREEN_H = 512, 240               # the program's sprites cover 512 pixels
NUMBER0 = 50                                # a pack's number: 50 + the stage (the runtime gives the arena's)
NIGHT = {10}                                # Chicago: the fighters keep arena 9's light (no T2LT)
DIRECT = 0x100                              # a texture page's bits 7-8: 15-bit
NO_CELL = 0xFF


# --- the disc -------------------------------------------------------------

def check_disc(disc) -> str:
    """The region of a Tekken disc; DiscError otherwise."""
    for name, region in REGIONS.items():
        if disc.read_file(name):
            return region
    raise DiscError('not a Tekken disc (no SCES_000.05, SLUS_000.06 or SLPS_000.40)')


def stage_file(disc, stage: int) -> bytes:
    data = disc.read_file(f'BG/{FILES[stage]}.')
    if data is None:
        raise DiscError(f'not a Tekken disc (no BG/{FILES[stage]})')
    return data


def vram_of(data: bytes, start: int) -> array:
    """The PlayStation's VRAM (1024 x 512 halfwords) with a stage's TIMs."""
    vram = array('H', bytes(2 * 1024 * 512))
    p = start
    while p + 8 <= len(data) and struct.unpack_from('<I', data, p)[0] == 0x10:
        flags = struct.unpack_from('<I', data, p + 4)[0]
        p += 8
        for _ in range(2 if flags & 8 else 1):
            n, x, y, w, h = struct.unpack_from('<I4H', data, p)
            if n < 12 or p + n > len(data):
                raise DiscError('a stage texture the disc does not hold whole')
            words = array('H', data[p + 12:p + 12 + 2 * w * h])
            for row in range(h):
                if y + row < 512 and x + w <= 1024:
                    vram[(y + row) * 1024 + x:(y + row) * 1024 + x + w] = words[row * w:(row + 1) * w]
            p += n
    return vram


def colour(lo: int, hi: int) -> tuple:
    """Two u16 of the globals: RGB bytes."""
    b = struct.pack('<2H', lo & 0xFFFF, hi & 0xFFFF)
    return b[0], b[1], b[2]


def parse(data: bytes, stage: int = 0) -> dict:
    """A stage file: its floor, background layers, globals and VRAM."""
    offs = struct.unpack_from(f'<{SECTIONS}I', data, 0)
    if offs[0] != 4 * SECTIONS or list(offs) != sorted(offs) or offs[-1] > len(data):
        raise DiscError(f'stage {stage}: not a Tekken stage file')
    if offs[1] - offs[0] != 8 * FLOOR_N * FLOOR_N or offs[10] - offs[9] != 44 or (offs[9] - offs[8]) % LAYER:
        raise DiscError(f'stage {stage}: a stage file of sections not known')
    floor = [struct.unpack_from('<4H', data, offs[0] + 8 * i) for i in range(FLOOR_N * FLOOR_N)]
    cells = [struct.unpack_from('<3H', data, offs[2] + 6 * i) for i in range((offs[3] - offs[2]) // 6)]
    g = struct.unpack_from('<22h', data, offs[9])
    layers = []
    for k in range((offs[9] - offs[8]) // LAYER):
        o = offs[8] + LAYER * k
        dist, = struct.unpack_from('<i', data, o)
        shift, = struct.unpack_from('<h', data, o + 4)
        bottom, width, height, places, sprites = struct.unpack_from('<5h', data, o + 12)
        first, ring = struct.unpack_from('<2I', data, o + 24)
        if not (32 <= width <= 64 and width * places == RING and 0 < height <= 256):
            raise DiscError(f'stage {stage}: layer {k} of {places} places of {width} x {height}')
        m = data[offs[3] + ring:offs[3] + ring + places]
        layers.append(dict(distance=dist, shift=shift, bottom=bottom, width=width, height=height, places=places,
                           sprites=sprites, cells=[None if b == NO_CELL else cells[first + b] for b in m]))
    if g[0] != len(layers):
        raise DiscError(f'stage {stage}: {len(layers)} layers, the globals say {g[0]}')
    return dict(stage=stage, name=NAMES[stage], floor=floor, layers=layers, globals=g, shift=g[4],
                sky=(colour(g[8], g[9]), colour(g[10], g[11])), effect=EFFECTS.get(g[20], g[20]),
                effect_sky=(colour(g[16], g[17]), colour(g[18], g[19])), vram=vram_of(data, offs[10]))


def read_stages(disc) -> list:
    check_disc(disc)
    return [parse(stage_file(disc, s), s) for s in range(len(FILES))]


# --- the disc's pictures ------------------------------------------------------

def texel(vram: array, page: int, u: int, v: int) -> int:
    """A 4-bit texel's index of a texture page."""
    tx, ty = (page & 15) * 64, (page >> 4 & 1) * 256
    return vram[((ty + v) % 512) * 1024 + (tx + u // 4) % 1024] >> ((u & 3) * 4) & 15


def palette(vram: array, clut: int) -> tuple:
    x, y = (clut & 63) * 16, clut >> 6 & 511
    return tuple(vram[y * 1024 + x:y * 1024 + x + 16])


def rgb(c: int) -> bytes:
    return bytes(((c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3))


def png(path: Path, w: int, h: int, pixels: bytes) -> None:
    raw = b''.join(b'\0' + pixels[r * w * 3:(r + 1) * w * 3] for r in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def sky_row(sky, y: int, horizon: int) -> bytes:
    """The sky's gradient: its top colour at the screen's top, its bottom at the horizon."""
    (r0, g0, b0), (r1, g1, b1) = sky
    a = min(max(y / max(horizon, 1), 0.0), 1.0)
    return bytes((round(r0 + (r1 - r0) * a), round(g0 + (g1 - g0) * a), round(b0 + (b1 - b0) * a)))


def draw_rings(sky, layers, shift, sprite, horizon: int = 150, height: int = SCREEN_H) -> tuple:
    """(w, h, rgb): every layer's ring, first to last, its first place at x 0,
    the horizon at y horizon, as at a level camera at the fight's centre;
    sprite(cell, w, h) gives a cell's colours (15-bit, 0 transparent) row by
    row, or None (not drawn)."""
    w = RING
    img = bytearray(b''.join(sky_row(sky, y, horizon) * w for y in range(height)))
    for lay in layers:
        x0 = -(lay["shift"] + shift) % RING          # the layer shifted left by its shift (hrbg.c)
        top = horizon + lay['bottom'] - lay['height']
        for p, cell in enumerate(lay['cells']):
            if cell is None:
                continue
            pix = sprite(cell, lay['width'], lay['height'])
            if pix is None:
                continue
            for yy in range(lay['height']):
                y = top + yy
                if not 0 <= y < height:
                    continue
                for xx in range(lay['width']):
                    c = pix[yy * lay['width'] + xx]
                    if c == 0:              # transparent, as the PlayStation's sprites
                        continue
                    o = 3 * (y * w + (x0 + p * lay['width'] + xx) % w)
                    img[o:o + 3] = rgb(c)
    return w, height, bytes(img)


def disc_sprite(vram: array):
    def sprite(cell, w, h):
        uv, clut, page = cell
        if page & 0x180 != 0:               # not 4-bit: the big screen (the effect draws it)
            return None
        pal = palette(vram, clut)
        return [pal[texel(vram, page, (uv & 255) + x, (uv >> 8) + y)] for y in range(h) for x in range(w)]
    return sprite


def panorama(st: dict, horizon: int = 150, height: int = SCREEN_H) -> tuple:
    """The disc's stage drawn as the PlayStation draws it (draw_rings)."""
    return draw_rings(st['sky'], st['layers'], st['shift'], disc_sprite(st['vram']), horizon, height)


def view(pano: tuple, x: int = 0) -> tuple:
    """A 512-pixel wide window of a panorama."""
    w, h, img = pano
    out = bytearray()
    for y in range(h):
        row = img[3 * y * w:3 * (y + 1) * w]
        row = row + row
        out += row[3 * x:3 * (x + SCREEN_W)]
    return SCREEN_W, h, bytes(out)


def floor_picture(st: dict) -> tuple:
    """(w, h, rgb): the 10 x 10 tiles from above, 64 x 64 each, turned."""
    vram = st['vram']
    w = h = 64 * FLOOR_N
    img = bytearray(3 * w * h)
    for t, (uv, page, _, clut) in enumerate(st['floor']):
        pal = palette(vram, clut)
        turn = page >> 14 & 3
        r0, c0 = divmod(t, FLOOR_N)
        for yy in range(64):
            for xx in range(64):
                # the tile turned a quarter clockwise per step
                u, v = ((xx, yy), (yy, 63 - xx), (63 - xx, 63 - yy), (63 - yy, xx))[turn]
                c = pal[texel(vram, page & 0xFF, (uv & 255) + u, (uv >> 8) + v)]
                o = 3 * ((r0 * 64 + yy) * w + c0 * 64 + xx)
                img[o:o + 3] = rgb(c)
    return w, h, bytes(img)


# --- the pack ---------------------------------------------------------------

class Vram(t2.Vram):
    """tekken2_stages' upper half, its slots taken wherever free: a cell taller
    than 64 in slots one under another, the big screen in slots side by side."""

    def __init__(self):
        super().__init__()
        self.taken = set()
        self.index = {xy: i for i, xy in enumerate(t2.SLOTS)}

    def _take(self, run: list, rect: tuple) -> tuple:
        self.taken.update(run)
        self.rects.append(rect)
        return t2.SLOTS[run[0]]

    def _runs(self, step: tuple, n: int):
        for i, (x, y) in enumerate(t2.SLOTS):
            run = [self.index.get((x + step[0] * k, y + step[1] * k)) for k in range(n)]
            if None in run or self.taken & set(run):
                continue
            if (x // 64 != (x + step[0] * (n - 1)) // 64) or (y // 256 != (y + step[1] * (n - 1)) // 256):
                continue                    # one texture page
            yield run

    def slot(self, idx: bytes) -> tuple:
        if idx not in self.slots:
            run = next(self._runs((0, 0), 1), None)
            if run is None:
                raise ValueError('no room for another 64 x 64 texture in the upper half')
            x, y = t2.SLOTS[run[0]]
            words = array('H', (idx[i] | idx[i + 1] << 4 | idx[i + 2] << 8 | idx[i + 3] << 12
                                for i in range(0, len(idx), 4)))
            self.slots[idx] = self._take(run, (x, y, 16, 64, words))
        return self.slots[idx]

    def tall(self, idx: bytes, h: int) -> tuple:
        """A 64-wide, h-high 4-bit picture in slots one under another."""
        key = ('tall', idx)
        if key not in self.slots:
            n = -(-h // 64)
            run = next(self._runs((0, 64), n), None)
            if run is None:
                raise ValueError(f'no room for a 64 x {h} texture in the upper half')
            x, y = t2.SLOTS[run[0]]
            words = array('H', (idx[i] | idx[i + 1] << 4 | idx[i + 2] << 8 | idx[i + 3] << 12
                                for i in range(0, len(idx), 4)))
            self.slots[key] = self._take(run, (x, y, 16, h, words))
        return self.slots[key]

    def wide(self, w: int, h: int) -> tuple:
        """w x h halfwords (15-bit), black, in slots side by side."""
        run = next(self._runs((16, 0), -(-w // 16)), None)
        if run is None:
            raise ValueError(f'no room for {w} x {h} halfwords in the upper half')
        x, y = t2.SLOTS[run[0]]
        return self._take(run, (x, y, w, h, array('H', bytes(2 * w * h))))


def cell_texels(vram: array, cell: tuple, w: int, h: int) -> tuple:
    """A 4-bit cell's indices, 64 wide (its w, then 0), and its palette."""
    uv, clut, page = cell
    idx = bytes(texel(vram, page, (uv & 255) + x, (uv >> 8) + y) if x < w else 0
                for y in range(h) for x in range(64))
    return idx, palette(vram, clut)


def floor_entries(st: dict) -> list:
    """The floor's tiles as tekken2_stages' flor() takes them."""
    vram, out = st['vram'], []
    for uv, page, _, clut in st['floor']:
        idx = bytes(texel(vram, page & 0xFF, (uv & 255) + x, (uv >> 8) + y) for y in range(64) for x in range(64))
        out.append((idx, palette(vram, clut), page >> 14 & 3, clut))
    return out


def layers_section(st: dict, vram: Vram) -> bytes:
    """T1LY (see the docstring): the cells into the upper half, the layers' rings onto them."""
    cells, table, rings = {}, [], []
    for lay in st['layers']:
        ring = bytearray()
        for cell in lay['cells']:
            if cell is None:
                ring.append(NO_CELL)
                continue
            key = (cell, lay['width'], lay['height'])
            if key not in cells:
                uv, clut, page = cell
                if page & 0x180 == DIRECT:  # the big screen: room for the runtime's picture
                    x, y = vram.wide(lay['width'], lay['height'])
                    table.append(struct.pack('<2H4B', t2.page_word(x, y) | DIRECT, 0, x % 64, y % 256, 0, 0))
                elif page & 0x180:
                    raise ValueError(f'stage {st["stage"]}: a cell of texture page {page:#x}')
                else:
                    idx, colours = cell_texels(st['vram'], cell, lay['width'], lay['height'])
                    x, y = vram.tall(idx, lay['height'])
                    table.append(struct.pack('<2H4B', t2.page_word(x, y), vram.clut(colours),
                                             (x % 64) * 4, y % 256, 0, 0))
                cells[key] = len(table) - 1
            if cells[key] >= NO_CELL:
                raise ValueError(f'stage {st["stage"]}: more than {NO_CELL} cells')
            ring.append(cells[key])
        rings.append(struct.pack('<i6h', lay['distance'], lay['shift'] + st['shift'], lay['bottom'], lay['width'],
                                 lay['height'], lay['places'], lay['sprites']) + t2.pad4(bytes(ring)))
    effect = st['globals'][20]
    return struct.pack('<4H', len(table), len(rings), effect, 0) + b''.join(table) + b''.join(rings)


def farb(st: dict) -> bytes:
    """FARB: one column of eight cells of the sky's bottom colour over the
    whole screen, its top colour above."""
    top, bottom = st['sky']
    head = struct.pack('<6h', 1, 8, 1, 0, 45, 0)
    colours = bytes(bottom) + bytes(top) + b'\1' + bytes(bottom) + b'\1\0'
    return t2.pad4(head + colours + bytes(2 * 8) + bytes(2 * 8) + bytes([0x74]) * 8)


def build(st: dict) -> bytes:
    """The stage's format 3 pack."""
    vram = Vram()
    lay = layers_section(st, vram)          # the big screen first: its three slots side by side
    floor = t2.flor(dict(st, floor=floor_entries(st), layout={}), vram)
    sections = [(b'VRAM', vram.section()), (b'TILE', struct.pack('<2I', t2.TILE_SIZE, 0)), (b'FARB', farb(st)),
                (b'FLOR', floor), (b'TRIG', t2.trig()), (b'T1LY', lay),
                (b'T1GL', struct.pack('<22h', *st['globals'])), (b'T2FX', struct.pack('<2H', t2.NO_DIM, 0))]
    if st['stage'] not in NIGHT:
        sections.append((b'T2LT', struct.pack('<2H', t2.DAY_ARENA, 0)))
    head = 16 + 12 * len(sections)
    table, body, off = b'', b'', head
    for sid, data in sections:
        data = t2.pad4(data)
        table += struct.pack('<4s2I', sid, off, len(data))
        body += data
        off += len(data)
    return struct.pack('<4s2H2I', b'ASTG', t2.FORMAT, NUMBER0 + st['stage'], len(sections), off) + table + body


# --- reading a pack back ------------------------------------------------------

def read_layers(section: bytes) -> tuple:
    """T1LY: (cells [(page, clut, u, v)], layers as parse() gives them, effect)."""
    n, m, effect, _ = struct.unpack_from('<4H', section, 0)
    o = 8
    cells = [struct.unpack_from('<2H2B', section, o + 8 * i) for i in range(n)]
    o += 8 * n
    layers = []
    for _ in range(m):
        dist, shift, bottom, width, height, places, sprites = struct.unpack_from('<i6h', section, o)
        o += 16
        ring = section[o:o + places]
        o += places + (-places % 4)
        layers.append(dict(distance=dist, shift=shift, bottom=bottom, width=width, height=height, places=places,
                           sprites=sprites, cells=[None if b == NO_CELL else cells[b] for b in ring]))
    return cells, layers, effect


def pictures(pack: bytes) -> dict:
    """{'view', 'panorama', 'floor'}: (w, h, rgb) as the runtime and the engine
    read the pack (the big screen black)."""
    s = t2.sections(pack)
    v = t2.upper_half(s['VRAM'])
    _, layers, _ = read_layers(s['T1LY'])
    g = struct.unpack('<22h', s['T1GL'][:44])
    sky = (colour(g[8], g[9]), colour(g[10], g[11]))

    def sprite(cell, w, h):
        page, clut, u, vv = cell
        x0, y0 = (page & 15) * 64, 512 + (page >> 4 & 1) * 256
        if page & DIRECT:
            return [0x8000] * (w * h)       # black, opaque
        return [t2.sample(v, x0, y0, u + x, vv + y, clut) for y in range(h) for x in range(w)]

    pano = draw_rings(sky, layers, 0, sprite)
    return {'view': view(pano), 'panorama': pano, 'floor': t2.pictures(pack)['floor']}


# --- the setup's side --------------------------------------------------------

def find_disc(state: Path) -> Path | None:
    """The Tekken disc the add-on's setup remembered (workspace/tekken1-disc.json), if it is still there."""
    import json
    try:
        disc = json.loads((state / 'tekken1-disc.json').read_text(encoding='utf-8')).get('disc') or {}
    except (OSError, ValueError):
        return None
    path = disc.get('path')
    return Path(path) if path and Path(path).is_file() else None


def pack_name(stage: int) -> str:
    return f'stage{stage:02d}.ast'


def slug(stage: int) -> str:
    return f'{stage:02d}-' + NAMES[stage].lower().replace(' ', '-')


def describe(st: dict, pack: bytes) -> str:
    near = sorted({l['distance'] for l in st['layers']})
    return (f'Tekken stage {st["stage"]:2} {st["name"]}: {len(pack)} bytes, {len(st["layers"])} layers '
            f'(distances {near}), effect {st["effect"]}')


def convert(disc: Path, out: Path, say=print, previews: bool = True) -> list[str]:
    """The 11 Classic packs (and their pictures) into out/classic (out/preview).
    Returns the packs' names relative to out. A folder of its own made next to
    it first, then put in its place."""
    classic, temp = out / CLASSIC, out / (CLASSIC + '.new')
    if temp.exists():
        for f in temp.iterdir():
            f.unlink()
    temp.mkdir(parents=True, exist_ok=True)
    pv = out / PREVIEW
    if previews:
        pv.mkdir(parents=True, exist_ok=True)
    names = []
    with open_disc(disc) as d:
        for st in read_stages(d):
            pack = build(st)
            (temp / pack_name(st['stage'])).write_bytes(pack)
            names.append(f'{CLASSIC}/{pack_name(st["stage"])}')
            if previews:
                png(pv / f'{slug(st["stage"])}-disc.png', *view(panorama(st)))
                for kind, picture in pictures(pack).items():
                    png(pv / f'{slug(st["stage"])}-{kind}.png', *picture)
            say(describe(st, pack))
    if classic.exists():
        for f in classic.iterdir():
            f.unlink()
        classic.rmdir()
    temp.replace(classic)
    return names


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--disc', type=Path, help='the Tekken disc (default: the one the setup remembered)')
    ap.add_argument('--root', type=Path, default=EXPANDED, help='the Expanded folder')
    ap.add_argument('--no-previews', action='store_true', help='no pictures')
    args = ap.parse_args(argv)
    disc = args.disc or find_disc(ADDON / 'workspace')
    if not disc:
        print('no Tekken disc (give --disc, or run the setup)')
        return 1
    convert(disc, args.root / OUT, previews=not args.no_previews)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
