"""The TMST add-on's setup: runs after the Expanded's (never inside it) and
makes, on the player's machine and from the player's own discs, what the
add-on's mods need. Nothing it makes is ever committed.

Each step is a function in STEPS: it checks whether its output is current
(a version stamp under addons/tmst/workspace/) and does its work only when not.
Steps: the Jukebox's title lists.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path

ADDON = Path(__file__).resolve().parents[1]
ROOT = ADDON.parents[1]                     # <expanded>/addons/tmst/setup
# The add-on's own data: in its folder, which the Expanded's setup (its
# free-space step deletes workspace/) never touches.
STATE = ADDON / 'workspace'


def log(message: str) -> None:
    print(f'TMST setup: {message}', flush=True)


def stamp(name: str) -> dict:
    try:
        return json.loads((STATE / f'{name}.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def save_stamp(name: str, data: dict) -> None:
    STATE.mkdir(parents=True, exist_ok=True)
    (STATE / f'{name}.json').write_text(json.dumps(data, indent=1), encoding='utf-8')


def jukebox_titles() -> None:
    """The Jukebox's title lists (tools/jukebox_titles.py): every run, since
    it only adds lines for music that came since."""
    sys.path.insert(0, str(ADDON / 'tools'))
    import jukebox_titles as titles
    titles.update(ROOT)


STEPS: list = [jukebox_titles]


def main() -> int:
    parser = argparse.ArgumentParser(description='TMST add-on setup (after the Expanded setup).')
    parser.parse_args()
    if not (ROOT / 'game.toml').is_file():
        log(f'{ROOT} is not a Tekken 3 Expanded folder')
        return 1
    for step in STEPS:
        step()
    log('done' if STEPS else 'nothing to make yet')
    return 0


if __name__ == '__main__':
    sys.exit(main())
