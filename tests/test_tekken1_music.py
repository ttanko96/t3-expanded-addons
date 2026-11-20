"""tools/tekken1_music.py without a disc: its pieces against the title lists,
the disc check, the trimming and the files it makes."""
import struct, sys, tempfile, unittest
from pathlib import Path
from unittest import mock

ADDON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON / 'tools'))           # by name: its worker processes import it again
import tekken1_music  # noqa: E402
from disc_image import DiscError, Track  # noqa: E402


def names(file: str) -> list[str]:
    lines = (ADDON / 'data/jukebox' / file).read_text(encoding='utf-8').splitlines()
    return [l.split('=', 1)[0].strip() for l in lines if l.strip() and not l.startswith('#')]


class FakeDisc:
    def __init__(self, exe, audio=range(2, 29)):
        self.exe = exe
        self.tracks = [Track(1, 'MODE2/2352', 100)] + [Track(t, 'AUDIO', 10 * t) for t in audio]

    def read_file(self, name):
        return self.exe if name == tekken1_music.EXE else None

    def audio(self, track):
        pcm = struct.pack('<4h', 0, 0, 0, 0) + struct.pack('<4h', track, -track, 100, 0) + bytes(8)
        return pcm

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass


class Tekken1MusicTest(unittest.TestCase):
    def test_every_t1_line_is_made(self):
        made = {(folder, name) for folder, name, _ in tekken1_music.pieces()}
        listed = {(folder, n) for folder in ('console', 'arcade')
                  for n in names(f'tekken1-{folder}.txt') if n.startswith('t1-')}
        self.assertTrue(listed)
        self.assertEqual(listed - made, set())
        self.assertEqual(len(made), len(tekken1_music.pieces()))
        skipped = {tuple(s.split('/')) for s in tekken1_music.skipped()}
        self.assertEqual(skipped, made - listed)                            # what no list names: not made

    def test_table(self):
        p = {(folder, name): track for folder, name, track in tekken1_music.pieces()}
        self.assertEqual(p['console', 't1-windermere.bgm'], 9)
        self.assertEqual(p['arcade', 't1-windermere.bgm'], 23)
        self.assertEqual(p['console', 't1-music-12.bgm'], p['arcade', 't1-music-12.bgm'])
        self.assertEqual(sorted(set(p.values())), list(range(2, 28)))      # every CD audio track but 28 (silent)

    def test_check(self):
        tekken1_music.check(FakeDisc(b'x' + tekken1_music.TABLE + b'y'))
        with self.assertRaisesRegex(DiscError, 'was not found'):
            tekken1_music.check(FakeDisc(b'x' * 64))
        with self.assertRaisesRegex(DiscError, 'missing'):
            tekken1_music.check(FakeDisc(tekken1_music.TABLE, audio=range(2, 27)))
        with self.assertRaisesRegex(DiscError, 'no TEKKEN.EXE'):
            tekken1_music.check(FakeDisc(None))

    def test_another_versions_table(self):
        t = tekken1_music.TABLE
        other = t[1:12] + t[:1] + t[12:14] + t[15:26] + t[14:15] + t[26:]        # the same tracks, another order
        exe = bytes([255]) * 40 + other + bytes(range(100, 140))
        self.assertEqual(tekken1_music.find_table(exe), other)
        self.assertEqual(tekken1_music.check(FakeDisc(exe)), other)
        self.assertEqual(tekken1_music.pieces(other)[0], ('console', 't1-windermere.bgm', other[0]))
        self.assertEqual(tekken1_music.find_table(exe + bytes([1]) + t), t)              # Europe's bytes win
        another = t[2:12] + t[:2] + t[12:14] + t[14:26] + t[26:]
        self.assertIsNone(tekken1_music.find_table(exe + bytes([1]) + another))          # two of the shape: none

    def test_trimmed(self):
        pcm = tekken1_music.trimmed(FakeDisc(b'').audio(5))
        self.assertEqual(pcm.tolist(), [[5, -5], [100, 0]])
        with self.assertRaises(DiscError):
            tekken1_music.trimmed(bytes(16))

    def test_convert(self):
        disc = FakeDisc(tekken1_music.TABLE)
        with tempfile.TemporaryDirectory() as tmp, \
                mock.patch.object(tekken1_music, 'open_disc', lambda _: disc), \
                mock.patch.object(tekken1_music, 'ProcessPoolExecutor', _Inline):
            out = Path(tmp)
            (out / 'arcade').mkdir()
            (out / 'arcade/t1-fiji.bgm').write_bytes(b'old')
            made = tekken1_music.convert(Path('disc.chd'), out, say=lambda _: None,
                                         skip={'arcade/t1-fiji.bgm'})
            self.assertFalse((out / 'arcade/t1-fiji.bgm').exists())
            self.assertEqual(len(made), len(tekken1_music.pieces()) - 1)
            data = (out / 'console/t1-music-12.bgm').read_bytes()
            self.assertEqual(data[:20], b'T3BGM001' + struct.pack('<3I', 44100, 2, 0))
            self.assertEqual(data, (out / 'arcade/t1-music-12.bgm').read_bytes())


class _Future:
    def __init__(self, value):
        self.value = value

    def result(self):
        return self.value


class _Inline:
    def __init__(self, **_):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass

    def submit(self, fn, *args):
        return _Future(fn(*args))


if __name__ == '__main__':
    unittest.main()
