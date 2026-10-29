"""tools/jukebox_titles.py, on a temporary Expanded folder (no disc)."""
import importlib.util, tempfile, unittest
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('jukebox_titles', ADDON / 'tools/jukebox_titles.py')
jukebox_titles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(jukebox_titles)


class JukeboxTitlesTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        music = self.root / 'addons/tmst/workspace/t2-music/arcade'
        music.mkdir(parents=True)
        for name in ('t2-paul.bgm', 't2-new.bgm'):
            (music / name).write_bytes(b'T3BGM001')
        self.defaults = jukebox_titles.DEFAULTS
        jukebox_titles.DEFAULTS = self.root / 'defaults'
        jukebox_titles.DEFAULTS.mkdir()
        (jukebox_titles.DEFAULTS / 'tekken2-arcade.txt').write_text('t2-paul.bgm = Paul (T2 Arcade)\n', encoding='utf-8')

    def tearDown(self) -> None:
        jukebox_titles.DEFAULTS = self.defaults
        self.tmp.cleanup()

    def lines(self, name: str) -> list[str]:
        text = (self.root / 'addons/tmst/workspace/jukebox' / name).read_text(encoding='utf-8')
        return [line for line in text.splitlines() if line and not line.startswith('#')]

    def test_defaults_then_names(self) -> None:
        jukebox_titles.update(self.root)
        self.assertEqual(self.lines('tekken2-arcade.txt'), ['t2-paul.bgm = Paul (T2 Arcade)', 't2-new.bgm = t2-new'])

    def test_user_lines_stay(self) -> None:
        lists = self.root / 'addons/tmst/workspace/jukebox'
        lists.mkdir(parents=True)
        (lists / 'tekken2-arcade.txt').write_text('# mine\nt2-paul = Mine --disable\ngone.bgm = Gone\n', encoding='utf-8')
        jukebox_titles.update(self.root)
        self.assertEqual(self.lines('tekken2-arcade.txt'), ['t2-paul = Mine --disable', 'gone.bgm = Gone', 't2-new.bgm = t2-new'])

    def test_tekken1_only_what_it_names(self) -> None:
        jukebox_titles.update(self.root)
        self.assertEqual(self.lines('tekken1-arcade.txt'), [])


if __name__ == '__main__':
    unittest.main()
