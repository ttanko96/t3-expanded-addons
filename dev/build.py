"""TEMPORARY developer build of Tekken 3 Expanded with the TMST add-on, until
the Expanded's own add-on hook exists (docs/API_GAPS.md G11).

    build.cmd                 the game you play (build-release), with the add-on
    build.cmd --without       the same, without the add-on (the plain Expanded)
    build.cmd <ref>           any tag, branch or commit of the Expanded, in a
                              worktree of its own: dev/build/<ref>/ (compat check)

It builds exactly as the Expanded's setup does (launcher/setup_backend.py:
build_game), with one argument more: -DCMAKE_PROJECT_INCLUDE=dev/tmst_inject.cmake.
The Expanded's tracked files are never touched. The setup must have run once
(generated/ holds the game made from the player's disc).
"""
from __future__ import annotations
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

DEV = Path(__file__).resolve().parent
ADDON = DEV.parent
ROOT = ADDON.parents[1]                    # <expanded>/addons/tmst/dev
INJECT = DEV / 'tmst_inject.cmake'


def fail(message: str) -> None:
    sys.exit(f'TMST build: {message}')


def expanded_setup():
    """The Expanded's setup module, for its toolchain and its mods/ keeping.
    Internal to the Expanded (docs/COMPAT.md): checked, never edited."""
    sys.path.insert(0, str(ROOT / 'launcher'))
    try:
        import setup_backend
    except Exception as error:   # noqa: BLE001 - any import failure means: run their setup
        fail(f'cannot load the Expanded setup ({error}); run Setup Tekken 3 first')
    for name in ('tools', 'kept_mods'):
        if not hasattr(setup_backend, name):
            fail(f'this Expanded setup has no {name}() (docs/COMPAT.md)')
    return setup_backend


def environment(setup):
    toolchain = setup.tools()      # downloads the pinned compilers when missing (Windows)
    env = dict(os.environ)
    env.update(PYTHONUTF8='1', PYTHONNOUSERSITE='1',
               CMAKE_BUILD_PARALLEL_LEVEL=str(min(8, max(2, os.cpu_count() or 2))))
    if toolchain:
        env['PATH'] = str(toolchain / 'bin') + os.pathsep + env.get('PATH', '')
        env.update(PSXRECOMP_TOOLCHAIN_DIR=str(toolchain), RETCOMM_TOOLCHAIN_DIR=str(toolchain))
    return toolchain, env


def configure_args(toolchain, source: Path, build: Path, with_addon: bool) -> list[str]:
    cmake = str(toolchain / 'bin/cmake.exe') if toolchain else (shutil.which('cmake') or 'cmake')
    args = [cmake, '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_SUPPRESS_REGENERATION=ON', '-DPython3_EXECUTABLE=' + sys.executable]
    if toolchain:
        args += ['-DCMAKE_C_COMPILER=' + str(toolchain / 'bin/clang.exe'),
                 '-DCMAKE_CXX_COMPILER=' + str(toolchain / 'bin/clang++.exe'),
                 '-DCMAKE_MAKE_PROGRAM=' + str(toolchain / 'bin/ninja.exe'), '-DPSX_STATIC_RUNTIME=ON']
    args += ['-DPSX_DEBUG_TOOLS=OFF', '-DPSX_DEBUG_SERVER_LITE=OFF', '-DPSX_NETPLAY=OFF',
             '-DPSXRECOMP_BIOS_STEMS=OpenBIOS', '-DPSXRECOMP_FORCE_SETUP_HOST=OFF',
             '-DPSXRECOMP_REQUIRE_GAME_C=ON', '-DTEKKEN3_BUILD_PC_PORT=OFF']
    # The TTT1 characters as the player's setup chose them (kept in the cache).
    cache = build / 'CMakeCache.txt'
    ttt1 = 'ON'
    if cache.is_file():
        m = re.search(r'^TEKKEN3_TTT1_CHARACTERS:BOOL=(\w+)', cache.read_text(errors='replace'), re.M)
        if m: ttt1 = m.group(1)
    args.append('-DTEKKEN3_TTT1_CHARACTERS=' + ttt1)
    # The cache keeps CMAKE_PROJECT_INCLUDE: set or clear it every time, so that
    # the Expanded's own Update & play builds what was built last.
    args.append('-DCMAKE_PROJECT_INCLUDE=' + (INJECT.as_posix() if with_addon else ''))
    return args


def run(args, env):
    print('>', ' '.join(str(a) for a in args), flush=True)
    if subprocess.run(args, env=env).returncode:
        fail('the build stopped (see above)')


def worktree(ref: str) -> tuple[Path, Path]:
    """A detached worktree of the Expanded at ref, with the player's generated
    game (and workspace, if kept) linked in: git never sees those folders."""
    safe = re.sub(r'[^A-Za-z0-9._-]', '_', ref)
    base = DEV / 'build' / safe
    source, build = base / 'src', base / 'out'
    if not (ROOT / 'generated').is_dir():
        fail('no generated/ in the Expanded folder: run Setup Tekken 3 first')
    subprocess.run(['git', '-C', str(ROOT), 'fetch', '--tags', '--quiet'])
    if source.is_dir():
        run(['git', '-C', str(source), 'checkout', '--detach', '--quiet', ref], os.environ)
    else:
        base.mkdir(parents=True, exist_ok=True)
        run(['git', '-C', str(ROOT), 'worktree', 'add', '--detach', str(source), ref], os.environ)
    for folder in GENERATED_FOLDERS:
        target, link = ROOT / folder, source / folder
        if target.is_dir() and not link.exists():
            if os.name == 'nt':
                subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)], check=True,
                               stdout=subprocess.DEVNULL)
            else:
                link.symlink_to(target, target_is_directory=True)
    for file in GENERATED_FILES:
        if (ROOT / file).is_file():
            shutil.copy2(ROOT / file, source / file)
    return source, build


# What the Expanded's setup makes beside its tracked files and its build reads
# (git status --ignored): linked or copied into another ref's worktree.
GENERATED_FOLDERS = ('generated', 'psxrecomp/generated', 'workspace', 'disc')
GENERATED_FILES = ('psxrecomp/runtime/include/overlay_codegen_hash.h',)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('ref', nargs='?', help='a tag, branch or commit of the Expanded (default: the game you play)')
    parser.add_argument('--without', action='store_true', help='build without the add-on')
    a = parser.parse_args()

    setup = expanded_setup()
    toolchain, env = environment(setup)
    with_addon = not a.without
    if a.ref:
        source, build = worktree(a.ref)
        run(configure_args(toolchain, source, build, with_addon), env)
        run([configure_args(toolchain, source, build, with_addon)[0], '--build', str(build),
             '--target', 'psx-runtime', '--parallel', env['CMAKE_BUILD_PARALLEL_LEVEL']], env)
        print(f'TMST build: {a.ref} built in {build}')
        return
    build = ROOT / 'build-release'
    if not (build / 'CMakeCache.txt').is_file():
        fail('no build-release yet: run Setup Tekken 3 first')
    # The framework empties build-release/mods at every link: the Expanded's
    # setup keeps the player's mod settings and data aside meanwhile.
    with setup.kept_mods():
        run(configure_args(toolchain, ROOT, build, with_addon), env)
        run([configure_args(toolchain, ROOT, build, with_addon)[0], '--build', str(build),
             '--target', 'psx-runtime', '--parallel', env['CMAKE_BUILD_PARALLEL_LEVEL']], env)
    print('TMST build: build-release ' + ('with' if with_addon else 'WITHOUT') + ' the add-on')


if __name__ == '__main__':
    main()
