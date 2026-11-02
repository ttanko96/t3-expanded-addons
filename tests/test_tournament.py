"""src/tournament: the pools, the bracket and the progress file, built alone with
the C compiler at hand (skipped without one) through tests/c/tournament_main.c."""
import os, shutil, subprocess, tempfile, unittest
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
SOURCES = ['tmst_bracket.c', 'tmst_tournament_pools.c', 'tmst_tournament_progress.c']
# The roster keys the Expanded has today: the stock fighters, Panda and Tiger, its guests.
STOCK = ['paul', 'law', 'lei', 'king', 'yoshimitsu', 'nina', 'hwoarang', 'xiaoyu', 'eddy', 'jin', 'julia', 'kuma',
         'bryan', 'heihachi', 'ogre', 'mokujin', 'gunjack', 'gon', 'anna', 'drb', 'trueogre', 'panda', 'tiger']
GUESTS = ['alex', 'angel', 'armorking', 'baek', 'bruce', 'devil', 'ganryu', 'jack2', 'jun', 'kazuya', 'kunimitsu',
          'lee', 'michelle', 'pjack', 'roger', 'tetsujin', 'unknown', 'wang']


def compiler():
    for name in (os.environ.get('CC'), 'clang', 'gcc', 'cc'):
        if name and shutil.which(name):
            return shutil.which(name)
    return None


@unittest.skipUnless(compiler(), 'no C compiler')
class TournamentTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        src = ADDON / 'src/tournament'
        cls.exe, cls.locked = (Path(cls.tmp.name) / n for n in ('t_open.exe', 't_locked.exe'))
        for exe, flag in ((cls.exe, ['-DTMST_TOURNAMENT_ALL_OPEN=1']), (cls.locked, [])):
            subprocess.run([compiler(), '-O2', '-Wall', '-Werror', *flag, '-I', str(src),
                            str(ADDON / 'tests/c/tournament_main.c'), *(str(src / s) for s in SOURCES), '-o', str(exe)],
                           check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, *args, exe=None):
        return subprocess.run([str(exe or self.exe), *map(str, args)], check=True, capture_output=True,
                              text=True).stdout.splitlines()

    # ---- pools -------------------------------------------------------------------
    def pools(self):
        out = {}; self.out = {}
        for line in self.run_exe('pools'):
            tid, boss, keys = line.split('|')
            out[tid] = (boss, keys.split() if not keys.startswith('*') else None)
            if keys.startswith('*'): self.out[tid] = [k[1:] for k in keys.split()[1:]]
        return out

    def test_pools(self):
        p = self.pools()
        self.assertEqual(list(p), ['t1', 't2', 't3', 'ttt', 'free'])
        self.assertEqual({t: p[t][0] for t in p},
                         {'t1': 'heihachi', 't2': 'kazuya+devil', 't3': 'ogre+trueogre', 'ttt': 'unknown', 'free': '-'})
        self.assertEqual([len(p[t][1]) for t in ('t1', 't2', 't3')], [17, 25, 23])
        self.assertIsNone(p['ttt'][1]); self.assertIsNone(p['free'][1])
        self.assertEqual(self.out, {'ttt': ['gon', 'drb'], 'free': []})
        self.assertTrue(set(p['t1'][1]) < set(p['t2'][1]))
        for t in ('t1', 't2', 't3'):
            keys = p[t][1]
            self.assertEqual(len(keys), len(set(keys)), t)
            bosses = p[t][0].split('+')
            for b in bosses: self.assertIn(b, keys, t)
            self.assertTrue(set(keys) <= set(STOCK + GUESTS), set(keys) - set(STOCK + GUESTS))
            self.assertGreaterEqual(len(keys) - 1 - len(bosses), 15, t)   # the player and the bosses out: 15 others left

    # ---- bracket -----------------------------------------------------------------
    def bracket(self, seed, results, player=3, boss=7, n=30):
        lines = self.run_exe('bracket', seed, player, boss, n, results)
        if lines == ['refused']: return None
        slots = list(map(int, lines[0].split()[1:]))
        steps = [l.split() for l in lines[1:-1]]
        final = list(map(int, steps[-1][2:]))
        return slots, steps, final, lines[-1].split()

    def test_slots(self):
        for seed in range(1, 60):
            slots, *_ = self.bracket(seed, '')
            self.assertEqual(len(slots), 16)
            self.assertEqual(len(set(slots)), 16, slots)
            self.assertIn(3, slots); self.assertNotIn(7, slots)
            self.assertTrue(all(0 <= s < 30 for s in slots))

    def test_player_place_varies(self):
        self.assertGreater(len({self.bracket(seed, '')[0].index(3) for seed in range(1, 80)}), 8)

    def test_pool_too_small(self):
        self.assertIsNone(self.bracket(1, '', n=16))        # 16 minus player and boss: 14
        self.assertIsNotNone(self.bracket(1, '', n=17))

    def test_champion_path(self):
        for seed in range(1, 40):
            slots, steps, final, end = self.bracket(seed, 'WWWWW')
            played = [s for s in steps if s[0] == 'match']
            self.assertEqual([int(s[1]) for s in played][-1], 15)
            self.assertEqual(len(played), 5)
            self.assertEqual(end[0], 'champion')
            self.assertEqual(final[15], 3)
            self.assertEqual(int(played[-1][3]), 7)          # the boss in the Grand Final
            self.check_consistent(slots, final)

    def test_boss_only_in_grand_final(self):
        for seed in range(1, 40):
            slots, steps, final, _ = self.bracket(seed, 'LLLLL')
            self.assertNotIn(7, final[:15])
            self.assertEqual(sum(s[0] == 'match' for s in steps), 1)

    def test_loss_each_round(self):
        for k in range(5):
            for seed in range(1, 15):
                slots, steps, final, end = self.bracket(seed, 'W' * k + 'L')
                self.assertEqual(end[0], 'out')
                lost = int(end[3])
                played = [int(s[1]) for s in steps if s[0] == 'match']
                self.assertEqual(len(played), k + 1)
                self.assertEqual(lost, played[-1])
                self.assertNotEqual(final[lost], 3)
                self.assertTrue(all(w >= 0 for w in final))
                self.check_consistent(slots, final)

    def test_round_drawn_after_player(self):
        slots, steps, *_ = self.bracket(5, 'W')
        step = [s for s in steps if s[0] == 'step'][0]
        self.assertEqual(int(step[1]), 0xff)                 # the whole first round, the player's match included
        winners = list(map(int, step[2:]))
        self.assertTrue(all(w >= 0 for w in winners[:8]) and all(w < 0 for w in winners[8:]))

    def check_consistent(self, slots, w):
        def fighters(m):
            if m < 8: return slots[2 * m], slots[2 * m + 1]
            if m < 12: return w[(m - 8) * 2], w[(m - 8) * 2 + 1]
            if m < 14: return w[8 + (m - 12) * 2], w[9 + (m - 12) * 2]
            if m == 14: return w[12], w[13]
            return w[14], 7
        for m in range(16):
            self.assertIn(w[m], fighters(m), m)
        for m in range(8):                                   # sides stay apart until the final
            left = m < 4
            self.assertEqual(slots.index(w[m]) < 8, left)

    def test_draws_even(self):
        wins = 0
        for seed in range(1, 200):
            slots, steps, final, _ = self.bracket(seed, '')
            wins += final[0] == slots[0]
        self.assertTrue(70 < wins < 130, wins)

    # ---- progress ----------------------------------------------------------------
    def test_progress_all_open_while_testing(self):
        path = Path(self.tmp.name) / 'open.txt'
        self.assertEqual(self.run_exe('progress', path, 'load'), ['won 0 open 0 1 2 3 4'])
        env = dict(os.environ, TMST_TOURNAMENT_ALL_OPEN='1')     # the headless tests' way, the default build
        out = subprocess.run([str(self.locked), 'progress', str(path), 'load'], check=True, capture_output=True,
                             text=True, env=env).stdout.splitlines()
        self.assertEqual(out, ['won 0 open 0 1 2 3 4'])
        self.assertEqual(self.run_exe('progress', path, 'load', exe=self.locked), ['won 0 open 0'])

    def test_progress_chain(self):
        path = Path(self.tmp.name) / 'tournament.txt'
        if path.exists(): path.unlink()
        run = lambda op: self.run_exe('progress', path, op, exe=self.locked)[0]
        self.assertEqual(run('load'), 'won 0 open 0')
        self.assertEqual(run('win:0'), 'won 1 open 0 1 opens 1')
        self.assertEqual(run('win:0'), 'won 1 open 0 1 opens -1')
        self.assertEqual(run('win:1'), 'won 3 open 0 1 2 opens 2')
        self.assertEqual(run('win:2'), 'won 7 open 0 1 2 3 opens 3')
        self.assertEqual(run('win:3'), 'won 15 open 0 1 2 3 4 opens 4')
        self.assertEqual(run('win:4'), 'won 31 open 0 1 2 3 4 opens -1')
        self.assertIn('won = t1, t2, t3, ttt, free', path.read_text())

    def test_progress_text(self):
        path = Path(self.tmp.name) / 'hand.txt'
        path.write_text('# edited\n  won =  t2 ,ttt, bogus\n')
        self.assertEqual(self.run_exe('progress', path, 'load', exe=self.locked), ['won 10 open 0 1 2 3 4'])


if __name__ == '__main__':
    unittest.main()
