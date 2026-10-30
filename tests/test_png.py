"""src/core/tmst_png.c, built alone with the C compiler at hand (skipped
without one), its files decoded with Python's zlib."""
import os, shutil, struct, subprocess, tempfile, unittest, zlib
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]


def compiler():
    for name in (os.environ.get('CC'), 'clang', 'gcc', 'cc'):
        if name and shutil.which(name):
            return shutil.which(name)
    return None


def pattern(kind, w, h):
    """The same pixels as tests/c/png_main.c, as RGB rows."""
    rows, seed = [], 12345
    for y in range(h):
        row = bytearray()
        for x in range(w):
            if kind == 0:
                v = (x * 255 // max(w - 1, 1)) << 16 | (y * 255 // max(h - 1, 1)) << 8 | ((x ^ y) & 255)
            elif kind == 1:
                seed = (seed * 1103515245 + 12345) & 0xFFFFFFFF
                v = seed >> 8
            else:
                v = 0xFFFFFF if (x // 8 + y // 8) & 1 else 0x102030
            row += bytes(((v >> 16) & 255, (v >> 8) & 255, v & 255))
        rows.append(bytes(row))
    return rows


def decode(data):
    """RGB rows of an 8-bit RGB PNG, checking every chunk's CRC."""
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    at, idat, w, h = 8, b'', None, None
    while at < len(data):
        n, kind = struct.unpack('>I4s', data[at:at + 8])
        body = data[at + 8:at + 8 + n]
        assert struct.unpack('>I', data[at + 8 + n:at + 12 + n])[0] == zlib.crc32(kind + body)
        if kind == b'IHDR':
            w, h, depth, colour = struct.unpack('>IIBB', body[:10])
            assert (depth, colour) == (8, 2)
        elif kind == b'IDAT':
            idat += body
        at += 12 + n
    raw, stride, rows, up = zlib.decompress(idat), w * 3, [], bytes(w * 3)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 3] if i >= 3 else 0
            b, c = up[i], up[i - 3] if i >= 3 else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line)); up = bytes(line)
    return w, h, rows


@unittest.skipUnless(compiler(), 'no C compiler')
class PngTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.tmp.name) / 'png_main.exe'
        subprocess.run([compiler(), '-O2', '-I', str(ADDON / 'src/core'), str(ADDON / 'tests/c/png_main.c'),
                        str(ADDON / 'src/core/tmst_png.c'), '-o', str(cls.exe)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def roundtrip(self, kind, w, h):
        path = Path(self.tmp.name) / f'p{kind}_{w}x{h}.png'
        subprocess.run([str(self.exe), str(path), str(w), str(h), str(kind)], check=True)
        data = path.read_bytes()
        self.assertEqual(decode(data), (w, h, pattern(kind, w, h)))
        return len(data)

    def test_gradient(self):
        self.roundtrip(0, 368, 480)

    def test_noise(self):
        self.roundtrip(1, 97, 33)

    def test_tiles_compress(self):
        self.assertLess(self.roundtrip(2, 736, 480), 736 * 480 * 3 // 20)

    def test_one_pixel(self):
        self.roundtrip(0, 1, 1)


if __name__ == '__main__':
    unittest.main()
