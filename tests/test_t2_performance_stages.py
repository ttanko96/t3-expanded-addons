"""tools/t2_performance_stages.py (issue #13): made-up packs, the recipe, and
the made packs against the Remake's when the setup has made both."""
import hashlib, struct, sys, tempfile, unittest
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON / 'tools'))
import t2_performance_stages as tp  # noqa: E402

KNOWN = {b'GRAS', b'FFLY', b'FTHR', b'RAIN', b'DIYA', b'DUST', b'T2AN', b'ANI2', b'ANI3', b'ANI4', b'SHIP',
         b'EAGL', b'GULL', b'T2OB', b'T2RT', b'T2OA'}


def pack(parts):
    return tp.packed(b'ASTG' + struct.pack('<2H', 3, 215), parts)


def objs(models, layouts):
    """T2OB: models as (vertices, primitives), then layouts drawing calls."""
    b = struct.pack('<I', len(models))
    for nv, np_ in models:
        b += struct.pack('<2H', nv, np_) + bytes(8 * nv + tp.PRIM * np_)
    b += struct.pack('<I', layouts)
    for i in range(layouts):
        b += struct.pack('<BBHHH', i, 0, 1009, 64, 0)
    return b


def prims(tile):
    """TILE's (T2TL's) primitives, all its tiles'."""
    n, o, total = struct.unpack_from('<I', tile, 4)[0], 8, 0
    for _ in range(n):
        nv, np_ = struct.unpack_from('<2H', tile, o + 4)
        o += 8 + 8 * nv + tp.PRIM * np_
        total += np_
    return total


class Transform(unittest.TestCase):
    def setUp(self):
        self.parts = [(b'VRAM', b'\x01' * 32), (b'T2OB', objs([(3, 2), (4, 1)], 6)), (b'T2RT', b'r' * 8),
                      (b'T2OA', struct.pack('<H', 203) + b'\0\0'), (b'GULL', struct.pack('<16h', *range(16))),
                      (b'GRAS', struct.pack('<hh', 4500, 0)), (b'FTHR', struct.pack('<h', 90) + b'x' * 30),
                      (b'FFLY', struct.pack('<h', 28) + b'y' * 22)]
        self.pack = pack(self.parts)

    def test_no_rule_the_pack_itself(self):
        self.assertEqual(tp.performance(self.pack, None), self.pack)
        self.assertEqual(tp.performance(self.pack, {}), self.pack)

    def test_objects_off(self):
        out = dict(tp.sections(tp.performance(self.pack, {'objects': 'off'})))
        for sid in (b'T2OB', b'T2RT', b'T2OA', b'GULL', b'GRAS'):
            self.assertNotIn(sid, out)
        self.assertEqual(out[b'VRAM'], b'\x01' * 32)
        self.assertEqual(out[b'FTHR'], dict(self.parts)[b'FTHR'])

    def test_drop_and_counts(self):
        out = dict(tp.sections(tp.performance(self.pack, {'drop': ['FFLY'], 'counts': {'FTHR': 30}})))
        self.assertNotIn(b'FFLY', out)
        self.assertEqual(struct.unpack_from('<h', out[b'FTHR'])[0], 30)
        self.assertEqual(out[b'FTHR'][2:32], b'x' * 30)
        # a count only lowers
        out = dict(tp.sections(tp.performance(self.pack, {'counts': {'FTHR': 200}})))
        self.assertEqual(struct.unpack_from('<h', out[b'FTHR'])[0], 90)

    def test_layouts(self):
        out = dict(tp.sections(tp.performance(self.pack, {'layouts': 4, 'drop': ['GRAS']})))
        o = out[b'T2OB']
        at = tp.layouts_at(o)
        self.assertEqual(struct.unpack_from('<I', o, at)[0], 4)
        self.assertEqual(len(o), at + 4 + 8 * 4)
        self.assertEqual(o[:at], dict(self.parts)[b'T2OB'][:at])          # the models as they were
        self.assertIn(b'GULL', out)                                         # the machine stays
        self.assertNotIn(b'GRAS', out)

    def test_pack_readable(self):
        out = tp.performance(self.pack, {'drop': ['GRAS']})
        n, size = struct.unpack_from('<2I', out, 8)
        self.assertEqual(size, len(out))
        self.assertEqual(n, len(self.parts) - 1)
        self.assertEqual(out[:8], self.pack[:8])

    def test_convert(self):
        with tempfile.TemporaryDirectory() as t:
            out = Path(t)
            (out / 'remake').mkdir()
            (out / 'performance').mkdir()
            (out / 'remake/stage02.ast').write_bytes(self.pack)
            (out / 'remake/stage02w.ast').write_bytes(self.pack)
            (out / 'remake/stage03.ast').write_bytes(self.pack)
            (out / 'performance/stage09.ast').write_bytes(b'old')
            before = hashlib.sha256(self.pack).digest()
            names = tp.convert(out, say=lambda *_: None, rules={2: {'drop': ['GRAS']}})
            self.assertEqual(names, ['performance/stage02.ast', 'performance/stage02w.ast', 'performance/stage03.ast'])
            self.assertFalse((out / 'performance/stage09.ast').exists())
            self.assertNotIn(b'GRAS', dict(tp.sections((out / 'performance/stage02w.ast').read_bytes())))
            self.assertEqual((out / 'performance/stage03.ast').read_bytes(), self.pack)
            self.assertEqual(hashlib.sha256((out / 'remake/stage02.ast').read_bytes()).digest(), before)


class Recipe(unittest.TestCase):
    def test_recipe(self):
        rules = tp.recipe()
        # the user's, 2026-11-21: every stage
        self.assertEqual(sorted(rules), list(range(23)))
        self.assertEqual(sorted(s for s, r in rules.items() if r.get('scenery')), [1, 4, 5, 6, 9, 10, 11, 13, 16, 18, 20, 22])
        try:
            import t2_remake_stage as gen
            builders = set(tp.Cuts.builders(gen))
        except ImportError:                     # numpy: the setup's python_packages
            builders = None
        for stage, rule in rules.items():
            self.assertTrue(rule.get('why'), stage)
            self.assertLessEqual(set(rule) - {'why', 'objects', 'drop', 'counts', 'layouts', 'scenery'}, set(), stage)
            self.assertLessEqual({s.encode() for s in rule.get('drop', [])}, KNOWN, stage)
            self.assertLessEqual({s.encode() for s in rule.get('counts', {})}, set(tp.COUNTED), stage)
            if 'objects' in rule:
                self.assertEqual(rule['objects'], 'off')
            cut = rule.get('scenery', {})
            self.assertLessEqual(set(cut) - {'drop', 'keep', 'every', 'faces'}, set(), stage)
            names = set(cut.get('drop', [])) | set(cut.get('keep', {})) | set(cut.get('every', {})) | set(cut.get('faces', {}))
            if builders is not None:
                self.assertLessEqual(names, builders, stage)      # a builder t2_remake_stage.scenery() calls

    def test_cuts(self):
        """Cuts: every builder runs (its random numbers drawn), only its faces left out."""
        import inspect as _inspect
        import types
        src = ('drawn = []\n'
               'def piece(n):\n    drawn.append(n)\n    return [{"n": n}]\n'
               'def tufts(k):\n    drawn.append("tufts")\n    return [{"filler": (1, i)} for i in range(k)]\n')
        scenery_src = ('def scenery():\n    faces = []\n    for n in range(4):\n        faces += piece(n)\n'
                       '    faces += tufts(6)\n    return faces\n')
        gen = types.ModuleType('gen')
        exec(src + scenery_src, gen.__dict__)       # the builders called by their module names, as the generator's
        drawn, piece, scenery = gen.drawn, gen.piece, gen.scenery
        real = _inspect.getsource
        _inspect.getsource = lambda f: scenery_src if f is scenery else real(f)
        try:
            self.assertEqual(tp.Cuts.builders(gen), ['piece', 'tufts'])
            with tp.Cuts(gen) as cuts:
                cuts.rule = {'keep': {'piece': [0, 2]}, 'every': {'tufts': 3}}
                faces = gen.scenery()
                self.assertEqual([f.get('n') for f in faces if 'n' in f], [0, 2])
                self.assertEqual([f['filler'][1] for f in faces if 'filler' in f], [0, 3])
                cuts.rule = {'drop': ['piece']}
                self.assertEqual(len(gen.scenery()), 6)
                cuts.rule = {'every': {'piece': 2}}
                self.assertEqual([f.get('n') for f in gen.scenery() if 'n' in f], [0, 2])
            self.assertIs(gen.piece, piece)                      # back as it was
            self.assertEqual(drawn.count('tufts'), 3)            # every builder ran each time
            self.assertEqual(len(drawn), 15)
        finally:
            _inspect.getsource = real

    def test_faces_and_walls(self):
        """"faces": a builder's faces of some pictures; the Church's walls without the pieces left out."""
        import types
        cuts = tp.Cuts(types.ModuleType('gen'))
        cuts.rule = {'faces': {'tower': ['flags', 'smoke']}}
        self.assertEqual(cuts.kept('tower', [{'tex': 'stone'}, {'tex': 'flags'}, {'tex': 'smoke'}]), [{'tex': 'stone'}])
        cuts.rule = {'drop': ['pillar', 'pew_rows'], 'keep': {'candelabrum': [0, 2]}}
        spec = {'pillars': [1, 2], 'pew_blocks': [3], 'candelabras': ['a', 'b', 'c'], 'altar': {'at': 0}}
        self.assertEqual(cuts.walls_spec(spec), {'candelabras': ['a', 'c'], 'altar': {'at': 0}})
        self.assertEqual(spec['pillars'], [1, 2])                            # the recipe's own spec untouched

    def test_tiles_replaced(self):
        out = dict(tp.sections(tp.performance(pack([(b'VRAM', b'v' * 8), (b'TILE', b't' * 8)]), {'why': 'x'},
                                              {b'TILE': b'n' * 12})))
        self.assertEqual(out[b'TILE'], b'n' * 12)
        self.assertEqual(out[b'VRAM'], b'v' * 8)

    def test_staging_and_setup(self):
        self.assertIn('"workspace/t2-stages/performance|mods/t2-stages/performance"',
                      (ADDON / 'addon.cmake').read_text(encoding='utf-8'))
        setup = (ADDON / 'setup/tmst_setup.py').read_text(encoding='utf-8')
        self.assertIn('tekken2_remake_stages, tekken2_performance_stages', setup)

    def test_runtime(self):
        """The runtime's third version: its folder the staged one, its row's label, TEKKEN 1 STAGES' two."""
        c = (ADDON / 'src/tekken2/tmst_tekken2_stages.c').read_text(encoding='utf-8')
        self.assertIn('{ "classic", "remake", "performance" }', c)
        self.assertIn('{ "CLASSIC", "REMAKE", "PERFORMANCE" }', c)
        self.assertIn(f'"{tp.PERFORMANCE}"', c)
        self.assertIn('T1_VERSIONS = PERFORMANCE', c)
        self.assertIn('"TEKKEN 1 STAGES", version_labels, T1_VERSIONS', c)


class Made(unittest.TestCase):
    """The setup's packs (skipped before it made them)."""
    def setUp(self):
        self.remake = ADDON / 'workspace/t2-stages/remake'
        self.perf = ADDON / 'workspace/t2-stages/performance'
        if not any(self.perf.glob('stage*.ast')):
            self.skipTest('no Performance packs (run the setup)')

    def test_made_as_the_recipe_says(self):
        rules = tp.recipe()
        for path in sorted(self.remake.glob('stage*.ast')):
            mine = self.perf / path.name
            self.assertTrue(mine.is_file(), path.name)
            rule = rules.get(tp.stage_of(path.name))
            want = tp.performance(path.read_bytes(), rule)
            if not (rule or {}).get('scenery'):
                self.assertEqual(mine.read_bytes(), want, path.name)
                continue
            # the scenery built again: the rest as the rule says, the tiles fewer
            got = tp.sections(mine.read_bytes())
            self.assertEqual([p for p in got if p[0] not in tp.TILES],
                             [p for p in tp.sections(want) if p[0] not in tp.TILES], path.name)
            old, new = dict(tp.sections(want)), dict(got)
            k = b'T2TL' if b'T2TL' in old else b'TILE'
            self.assertLess(prims(new[k]), prims(old[k]), path.name)

    def test_the_grass_gone(self):
        for path in sorted(self.perf.glob('stage*.ast')):
            parts = dict(tp.sections(path.read_bytes()))
            self.assertNotIn(b'GRAS', parts, path.name)
            if b'T2OA' in parts:
                self.assertNotEqual(struct.unpack_from('<H', parts[b'T2OA'])[0], 201, path.name)
