"""tools/tekken1_stages.py without a disc: a stage file made up here (its
sections and TIMs as BG/STGnnXXX holds them), read and drawn."""
import struct, sys, unittest
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON / 'tools'))
import tekken1_stages as t1  # noqa: E402
import tekken2_stages as t2  # noqa: E402
from disc_image import DiscError  # noqa: E402


def tim(clut_xy, colours, x, y, w, h, words):
    """A 4-bit TIM with its palette."""
    out = struct.pack('<2I', 0x10, 8)
    out += struct.pack('<I4H', 12 + 32, clut_xy[0], clut_xy[1], 16, 1) + struct.pack('<16H', *colours)
    return out + struct.pack('<I4H', 12 + 2 * w * h, x, y, w, h) + struct.pack(f'<{w * h}H', *words)


def packed(indices):
    return [indices[i] | indices[i + 1] << 4 | indices[i + 2] << 8 | indices[i + 3] << 12
            for i in range(0, len(indices), 4)]


RED, BLUE = 0x001F, 0x7C00
SKY_TOP, SKY_BOTTOM = (0, 0x40, 0xFF), (0xC0, 0xE0, 0xFF)


def stage_file(layers=1, effect=0, more=False):
    """One layer of 96 places of 64 x 16, every other place cell 0 (red, its
    left half transparent) and the rest none; a floor tile (blue, one red
    texel at its top left), tile t turned t % 4. more: two layers besides, of
    128 places of 48: one 100 high (cell 1, 4-bit, red), one Stadium's big
    screen (cell 2, 15-bit, 48 x 21)."""
    floor = b''.join(struct.pack('<4H', 0, 0x0F | (t % 4) << 14, 0, 481 << 6) for t in range(100))
    sec1 = struct.pack('<4H', 0, 0x8001, 64, 0)
    cells = struct.pack('<3H', 0, 480 << 6, 0x0E)               # page 14: VRAM x 896
    ring = bytes([0, 0xFF] * 48)
    lay = struct.pack('<ih6xhhhhh2x2I16x', -1, 0, 20, 64, 16, 96, 9, 0, 0)
    if more:
        cells += struct.pack('<3H', 0, 480 << 6, 0x0D)          # page 13: VRAM x 832, 100 rows
        cells += struct.pack('<3H', 0x8000, 0, 0x10D)            # the big screen: 15-bit
        ring += bytes([0] + [0xFF] * 127) + bytes([0xFF, 0] + [0xFF] * 126)
        lay += struct.pack('<ih6xhhhhh2x2I16x', 100000, 0, 0, 48, 100, 128, 11, 1, 96)
        lay += struct.pack('<ih6xhhhhh2x2I16x', 100000, 0, -40, 48, 21, 128, 11, 2, 224)
        layers = 3
    g = [layers, 0, 0, 0, 0, 0, -100, 73, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, effect, 0]
    sky = struct.unpack('<4H', bytes(SKY_TOP) + b'\xff' + bytes(SKY_BOTTOM) + b'\xff')
    g[8:12] = [s - 0x10000 if s & 0x8000 else s for s in sky]
    sec9 = struct.pack('<22h', *g)
    cell_idx = [0 if u < 32 else 1 for v in range(16) for u in range(64)]
    floor_idx = [2 if (u, v) == (0, 0) else 3 for v in range(64) for u in range(64)]
    tex = tim((0, 480), [0, RED] + [0] * 14, 896, 0, 16, 16, packed(cell_idx))
    tex += tim((0, 481), [0, 0, RED, BLUE] + [0] * 12, 960, 0, 16, 64, packed(floor_idx))
    if more:
        tex += tim((0, 480), [0, RED] + [0] * 14, 832, 0, 16, 100, packed([1] * (64 * 100)))
    else:
        lay *= layers
    parts = [floor, sec1, cells, ring, b'', b'', b'', b'', lay, sec9, tex]
    offs, o = [], 4 * 11
    for p in parts:
        offs.append(o)
        o += len(p)
    return struct.pack('<11I', *offs) + b''.join(parts)


class Stages(unittest.TestCase):
    def test_parse(self):
        st = t1.parse(stage_file(effect=2), 5)
        self.assertEqual(st['name'], 'King George Island')
        self.assertEqual(st['effect'], 'snow')
        self.assertEqual(st['sky'], (SKY_TOP, SKY_BOTTOM))
        lay, = st['layers']
        self.assertEqual((lay['distance'], lay['width'], lay['height'], lay['places'], lay['bottom']),
                         (-1, 64, 16, 96, 20))
        self.assertEqual(lay['cells'][0], (0, 480 << 6, 0x0E))
        self.assertIsNone(lay['cells'][1])
        self.assertEqual(len(st['floor']), 100)

    def test_panorama(self):
        st = t1.parse(stage_file(), 0)
        w, h, img = t1.panorama(st, horizon=100)
        self.assertEqual((w, h), (t1.RING, t1.SCREEN_H))
        px = lambda x, y: tuple(img[3 * (y * w + x):3 * (y * w + x) + 3])
        y = 100 + 20 - 8                                   # inside the layer: bottom 20 below the horizon, 16 high
        self.assertEqual(px(40, y), (0xF8, 0, 0))          # cell 0's right half: red
        self.assertEqual(px(10, y), tuple(t1.sky_row(st['sky'], y, 100)))  # its left half transparent: sky
        self.assertEqual(px(64 + 40, y), tuple(t1.sky_row(st['sky'], y, 100)))  # place 1: none
        self.assertEqual(px(128 + 40, y), (0xF8, 0, 0))    # place 2: cell 0 again
        self.assertEqual(px(0, 0), SKY_TOP)
        self.assertEqual(t1.view((w, h, img), t1.RING - 10)[0], t1.SCREEN_W)

    def test_floor_turns(self):
        st = t1.parse(stage_file(), 0)
        w, h, img = t1.floor_picture(st)
        red = lambda x, y: img[3 * (y * w + x)] == 0xF8
        # tile 0 unturned: its red texel top left; tile 1 a quarter clockwise: top right; 2: bottom right; 3: bottom left
        self.assertTrue(red(0, 0))
        self.assertTrue(red(64 + 63, 0))
        self.assertTrue(red(128 + 63, 63))
        self.assertTrue(red(192, 63))

    def test_rejects(self):
        with self.assertRaises(DiscError):
            t1.parse(b'\0' * 44, 0)
        bad = bytearray(stage_file())               # one layer in the table, three in the globals
        struct.pack_into('<h', bad, struct.unpack_from('<11I', bad)[9], 3)
        with self.assertRaises(DiscError):
            t1.parse(bytes(bad), 0)


class Runtime(unittest.TestCase):
    def test_runtime_agrees(self):
        """src/tekken2/tmst_tekken2_stages.c: Tekken 1's names after Tekken 2's
        (upper case, in order), their count; the packs' folder staged by
        addon.cmake; src/tekken1/tmst_tekken1_stages.c's 15-bit page bits."""
        import re
        c = (ADDON / 'src/tekken2/tmst_tekken2_stages.c').read_text(encoding='utf-8')
        table = re.search(r'names\[ALL_STAGES\] = \{(.*?)\};', c, re.S).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', table)[-len(t1.NAMES):], [n.upper() for n in t1.NAMES])
        self.assertIn(f'T1_STAGES = {len(t1.NAMES)}', c)
        addon = (ADDON / 'addon.cmake').read_text(encoding='utf-8')
        self.assertIn(f'workspace/t1-stages/{t1.CLASSIC}|mods/t1-stages/{t1.CLASSIC}', addon)
        layers = (ADDON / 'src/tekken1/tmst_tekken1_stages.c').read_text(encoding='utf-8')
        self.assertIn(f'DIRECT = {t1.DIRECT:#x}', layers)
        # our floor: its switch back to the engine's, the engine's quads dropped for it
        self.assertIn('"TMST_T1_FLOOR"', layers)
        self.assertIn('tmst_tekken1_own_floor()', c)
        self.assertIn('GROUND_WORDS = 6', layers)          # POLY_F4: tag, colour, 4 corners


class Pack(unittest.TestCase):
    def setUp(self):
        try:
            import numpy  # noqa: F401 - the floor's near halves (tekken2_stages.magnified)
        except ImportError:
            self.skipTest('no numpy')
        self.st = t1.parse(stage_file(effect=3, more=True), 3)
        self.pack = t1.build(self.st)
        self.s = t2.sections(self.pack)

    def test_sections(self):
        self.assertEqual(self.s['number'], t1.NUMBER0 + 3)
        for need in ('T1LY', 'T1GL', 'T2FX', 'T2LT'):
            self.assertIn(need, self.s)
        self.assertEqual(struct.unpack('<22h', self.s['T1GL'][:44]), self.st['globals'])
        night = t2.sections(t1.build(t1.parse(stage_file(), 10)))
        self.assertNotIn('T2LT', night)         # Chicago keeps arena 9's light

    def test_floor_as_the_runtime_reads_it(self):
        """FLOR as src/tekken1/tmst_tekken1_stages.c floor_load() walks it: the
        tiles, the quarters, the counts (hand_over zeroes them), the turns."""
        import struct
        f = self.s['FLOR']
        nv = struct.unpack_from('<H', f, 8 + 4000 + 100)[0]
        at = 8 + 4000 + 100 + ((2 + 4 * nv + 3) & ~3)
        counts = f[at:at + 100]
        self.assertEqual(set(counts), {1})
        at += 100 + 4 * sum(counts)
        turns = [struct.unpack_from('<4H', f, at + 8 * t) for t in range(4)]
        self.assertEqual(turns, [tuple(t) for t in t2.TURNS])
        tiles = struct.unpack_from('<200I', f, 8)
        self.assertTrue(all(w >> 30 == self.st['floor'][i][1] >> 14 & 3 for i, w in enumerate(tiles[::2])))

    def test_layers_read_back(self):
        cells, layers, effect = t1.read_layers(self.s['T1LY'])
        self.assertEqual(effect, 3)
        self.assertEqual([(l['distance'], l['width'], l['height'], l['places']) for l in layers],
                         [(-1, 64, 16, 96), (100000, 48, 100, 128), (100000, 48, 21, 128)])
        self.assertEqual(len(cells), 3)
        tall, screen = layers[1]['cells'][0], layers[2]['cells'][1]
        self.assertEqual(tall[0] & 0x180, 0)
        self.assertEqual(screen[0] & 0x180, t1.DIRECT)
        self.assertIsNone(layers[2]['cells'][0])

    def test_pictures_agree(self):
        disc = t1.panorama(self.st)
        pack = t1.pictures(self.pack)['panorama']
        w, h, a = disc
        b = pack[2]
        differ = {(i // 3 % w, i // 3 // w) for i in range(0, len(a), 3) if a[i:i + 3] != b[i:i + 3]}
        # only the big screen: black in the pack, the effect's in the game
        top = 150 - 40 - 21
        self.assertTrue(differ)
        self.assertTrue(all(48 <= x < 96 and top <= y < top + 21 for x, y in differ))

    def test_upper_half_room(self):
        v = t2.upper_half(self.s['VRAM'])
        rects = []
        n, o = struct.unpack_from('<I', self.s['VRAM'])[0], 4
        for _ in range(n):
            x, y, w, h = struct.unpack_from('<4H', self.s['VRAM'], o)
            rects.append((x, y, w, h))
            o += 8 + 2 * w * h
        for x, y, w, h in rects:
            self.assertTrue(t2.stage_area(x, y) and t2.stage_area(x + w - 1, y + h - 1) or y >= 960,
                            (x, y, w, h))
            self.assertEqual(x // 64, (x + w - 1) // 64)                    # one texture page
            self.assertEqual((y - 512) // 256, (y + h - 1 - 512) // 256)
            wx, wy, ww, wh = t2.WOLF
            self.assertFalse(x < wx + ww and wx < x + w and y < wy + wh and wy < y + h)
        tall = next(r for r in rects if r[3] == 100)
        self.assertEqual(tall[2], 16)
        screen = next(r for r in rects if r[2] == 48)
        self.assertEqual(screen[3], 21)
        self.assertEqual(sum(v[(screen[1] - 512) * 1024 + screen[0]:][:48]), 0)    # black


if __name__ == '__main__':
    unittest.main()
