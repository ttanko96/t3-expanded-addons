"""The TMST add-on's setup: runs after the Expanded's (never inside it) and
makes, on the player's machine and from the player's own discs, what the
add-on's mods need. Nothing it makes is ever committed.

Each step is a function in STEPS: it checks whether its output is current
(a version stamp under addons/tmst/workspace/) and does its work only when not.
Steps: the Jukebox's TTT console and arcade music, then its title lists.
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


def music_pack_step(music, name: str, label: str, source: str, convert) -> None:
    """A Jukebox music folder made from one of the Expanded's music packs (and,
    for a tool with find_disc, the disc the Expanded's setup was given): again
    when one of them or the tool's VERSION changed or a file is gone; nothing
    without any."""
    pack = music.find_pack(ROOT)
    disc = music.find_disc(ROOT) if hasattr(music, 'find_disc') else None
    if not pack and not disc:
        log(f'{label}: no pack (the Expanded setup makes it from {source}), skipped')
        return
    want = {'version': music.VERSION}
    for key, path in (('pack', pack), ('disc', disc)):
        if path:
            st = path.stat()
            want[key] = {'path': str(path), 'size': st.st_size, 'mtime': int(st.st_mtime)}
    have = stamp(name)
    out = ROOT / music.OUT
    if have.get('stamp') == want and all((out / n).is_file() for n in have.get('files', [])):
        return
    log(f'{label}: making the pieces')
    names = convert(pack, out, disc)
    if names is not None:
        save_stamp(name, {'stamp': want, 'files': names})


def ttt_ps2_music() -> None:
    """The Jukebox's TTT console music (tools/ttt_ps2_music.py), from the
    Expanded's PS2 stage music pack and the TTT PS2 disc it was made from."""
    sys.path.insert(0, str(ADDON / 'tools'))
    import ttt_ps2_music as music
    music_pack_step(music, 'ttt-ps2-music', 'TTT PS2 music', 'the TTT PS2 disc',
                    lambda pack, out, disc: music.convert(pack, out, disc, say=log))


def expanded_cc() -> str | None:
    """The Expanded's pinned clang on Windows (its setup gets it when missing,
    as for the build: dev/build.py), else the one installed."""
    sys.path.insert(0, str(ROOT / 'launcher'))
    try:
        import setup_backend
        toolchain = setup_backend.tools()
    except Exception as error:   # noqa: BLE001 - any failure: look for an installed compiler
        log(f'the Expanded setup gives no compiler ({error})')
        toolchain = None
    if toolchain and (Path(toolchain) / 'bin/clang.exe').is_file():
        return str(Path(toolchain) / 'bin/clang.exe')
    return None


def ttt_arcade_music() -> None:
    """The Jukebox's TTT arcade music (tools/ttt_arcade_music.py), rendered
    from the Expanded's arcade stage music pack with a C compiler."""
    sys.path.insert(0, str(ADDON / 'tools'))
    import ttt_arcade_music as music

    def convert(pack, out, disc):
        cc = music.compiler(expanded_cc())
        if not cc:
            log('TTT arcade music: no C compiler, skipped')
            return None
        return music.convert(pack, out, cc, say=log)
    music_pack_step(music, 'ttt-arcade-music', 'TTT arcade music', 'tektagt.zip', convert)


def jukebox_titles() -> None:
    """The Jukebox's title lists (tools/jukebox_titles.py): every run, since
    it only adds lines for music that came since."""
    sys.path.insert(0, str(ADDON / 'tools'))
    import jukebox_titles as titles
    titles.update(ROOT)


STEPS: list = [ttt_ps2_music, ttt_arcade_music, jukebox_titles]   # the music first: the titles list it


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
