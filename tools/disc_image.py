"""A PlayStation disc image, read without any tool of its own: a .cue with its
.bin files (one per track or a single-file dump), a .bin alone (its .cue
beside it, when one names it) or a .chd (MAME's compressed disc, version 5,
as chdman makes it).

    with open_disc(path) as disc:
        disc.read(lba)              # 2048 bytes of user data of track 1's sector lba
        disc.read_file('SYSTEM.CNF') # a file of the ISO 9660 file system, or None
        disc.tracks                 # [Track(number, kind, frames)]

CHD codecs read: cdlz (LZMA), cdzl (zlib) and cdzs (zstd, on a Python with
compression.zstd); a hunk in cdfl (FLAC: chdman uses it for audio, almost
never for data) raises DiscError, as a parent CHD (a delta) does.
"""
from __future__ import annotations
import lzma, re, struct, zlib
from dataclasses import dataclass
from pathlib import Path

SECTOR = 2352
SYNC = b'\x00' + b'\xff' * 10 + b'\x00'
EXTENSIONS = ('.cue', '.bin', '.chd')


class DiscError(Exception):
    pass


@dataclass
class Track:
    number: int
    kind: str               # MODE2/2352, MODE1/2352, AUDIO, ...
    frames: int


def user_data(sector: bytes) -> bytes:
    """The 2048 bytes of user data of a raw 2352-byte sector (mode 1 or mode 2 form 1)."""
    if sector[:12] == SYNC and sector[15] == 1:
        return sector[16:16 + 2048]
    return sector[24:24 + 2048]


# ---- .cue / .bin ------------------------------------------------------------

def msf(text: str) -> int:
    m, s, f = (int(x) for x in text.split(':'))
    return (m * 60 + s) * 75 + f


def cue_for(path: Path) -> Path:
    """A .bin chosen in place of its .cue: the .cue beside it that names it, if any."""
    if path.suffix.lower() != '.bin':
        return path
    for cue in sorted(set(path.parent.glob('*.cue')) | set(path.parent.glob('*.CUE'))):
        try:
            text = cue.read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        if any(n.lower() == path.name.lower() for n in re.findall(r'FILE\s+"([^"]+)"', text, re.I)):
            return cue
    return path


class CueDisc:
    def __init__(self, path: Path):
        self.path = path
        self.tracks: list[Track] = []
        if path.suffix.lower() == '.cue':
            self._parse_cue(path)
        else:                                       # a .bin with no .cue: one data track
            self.data_file, self.data_start = path, 0
            self.tracks = [Track(1, 'MODE2/2352', path.stat().st_size // SECTOR)]
        self.handle = open(self.data_file, 'rb')

    def _parse_cue(self, cue: Path) -> None:
        current = None
        entries = []                                # (file, number, kind, INDEX 01, INDEX 00 sector in its file)
        for line in cue.read_text(encoding='utf-8', errors='replace').splitlines():
            w = line.split()
            if not w:
                continue
            key = w[0].upper()
            if key == 'FILE':
                m = re.search(r'"([^"]+)"', line)
                name = m[1] if m else w[1]
                current = cue.parent / name
                if not current.is_file():
                    raise DiscError(f'{cue.name} names {name}, which is not beside it')
            elif key == 'TRACK' and current:
                entries.append([current, int(w[1]), w[2].upper(), 0, None])
            elif key == 'INDEX' and entries and int(w[1]) in (0, 1):
                entries[-1][4 - int(w[1])] = msf(w[2])
        if not entries:
            raise DiscError(f'{cue.name} names no track')
        for i, (file, number, kind, start, _) in enumerate(entries):
            nxt = entries[i + 1] if i + 1 < len(entries) else None
            # A track ends where the next one's pregap (INDEX 00) starts, in the same file.
            end = (nxt[3] if nxt[4] is None else nxt[4]) if nxt and nxt[0] == file else file.stat().st_size // SECTOR
            self.tracks.append(Track(number, kind, end - start))
        self.data_file, self.data_start = entries[0][0], entries[0][3]

    def raw(self, lba: int) -> bytes:
        self.handle.seek((self.data_start + lba) * SECTOR)
        data = self.handle.read(SECTOR)
        if len(data) != SECTOR:
            raise DiscError(f'sector {lba} is past the end of {self.data_file.name}')
        return data

    def close(self) -> None:
        self.handle.close()


# ---- .chd (version 5) -------------------------------------------------------

FRAME = 2448                                        # a CD frame in a CHD: sector + subcode
SUBCODE = 96
# Map entry types (MAME src/lib/util/chd.h).
TYPE_0, TYPE_3, NONE, SELF, PARENT = 0, 3, 4, 5, 6
RLE_SMALL, RLE_LARGE, SELF_0, SELF_1, PARENT_SELF, PARENT_0, PARENT_1 = 7, 8, 9, 10, 11, 12, 13


class Bits:
    """MSB-first bit reader; past the end it reads zeros (as MAME's)."""
    def __init__(self, data: bytes):
        self.value, self.left = int.from_bytes(data, 'big'), len(data) * 8

    def read(self, n: int) -> int:
        if n == 0:
            return 0
        self.left -= n
        if self.left >= 0:
            return (self.value >> self.left) & ((1 << n) - 1)
        out = (self.value << -self.left) & ((1 << n) - 1) if self.left > -n else 0
        self.value, self.left = 0, 0
        return out

    def peek(self, n: int) -> int:
        value, left = self.value, self.left
        out = self.read(n)
        self.value, self.left = value, left
        return out


def huffman_map_decoder(bits: Bits, codes: int = 16, maxbits: int = 8):
    """MAME's huffman_decoder<16, 8>::import_tree_rle: the code lengths, then canonical codes."""
    lengths, numbits = [], 4
    while len(lengths) < codes:
        n = bits.read(numbits)
        if n != 1:
            lengths.append(n)
        else:
            n = bits.read(numbits)
            if n == 1:
                lengths.append(1)
            else:
                lengths += [n] * (bits.read(numbits) + 3)
    if len(lengths) != codes:
        raise DiscError('the CHD map is damaged (its code table)')
    histo = [0] * 33
    for n in lengths:
        histo[n] += 1
    start = 0
    for length in range(32, 0, -1):
        nxt = (start + histo[length]) >> 1
        if length != 1 and nxt * 2 != start + histo[length]:
            raise DiscError('the CHD map is damaged (its codes)')
        histo[length], start = start, nxt
    table = [None] * (1 << maxbits)
    for symbol, n in enumerate(lengths):
        if n:
            code = histo[n]
            histo[n] += 1
            shift = maxbits - n
            for i in range(code << shift, (code + 1) << shift):
                table[i] = (symbol, n)

    def decode() -> int:
        entry = table[bits.peek(maxbits)]
        if entry is None:
            raise DiscError('the CHD map is damaged (a code)')
        bits.read(entry[1])
        return entry[0]
    return decode


def zstd_decompress(data: bytes, size: int) -> bytes:
    try:
        from compression import zstd             # Python 3.14+
    except ImportError:
        raise DiscError('this CHD is compressed with zstd, which this Python cannot read: '
                        'convert it again with chdman createcd (default codecs)') from None
    return zstd.decompress(data)[:size]


def lzma_decompress(data: bytes, size: int) -> bytes:
    # MAME's LZMA codec: raw LZMA1, lc 3 lp 0 pb 2, the dictionary sized for the hunk.
    d = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[
        {'id': lzma.FILTER_LZMA1, 'dict_size': 1 << 20, 'lc': 3, 'lp': 0, 'pb': 2}])
    return d.decompress(data, size)


def inflate(data: bytes, size: int) -> bytes:
    return zlib.decompressobj(-15).decompress(data, size)


CD_CODECS = {b'cdlz': lzma_decompress, b'cdzl': inflate, b'cdzs': zstd_decompress}


class ChdDisc:
    def __init__(self, path: Path):
        self.path = path
        self.handle = open(path, 'rb')
        head = self.handle.read(124)
        if head[:8] != b'MComprHD':
            raise DiscError(f'{path.name} is not a CHD file')
        version = struct.unpack('>I', head[12:16])[0]
        if version != 5:
            raise DiscError(f'{path.name} is a version {version} CHD: convert it again with a recent chdman '
                            '(chdman createcd) to make a version 5 one')
        self.codecs = [head[16 + 4 * i:20 + 4 * i] for i in range(4)]
        (self.logical, self.mapoffset, self.metaoffset, self.hunkbytes,
         self.unitbytes) = struct.unpack('>QQQII', head[32:64])
        if head[104:124].strip(b'\0'):
            raise DiscError(f'{path.name} is a delta CHD (it needs its parent): use the full one')
        self.hunks = -(-self.logical // self.hunkbytes)
        self._read_map()
        self.tracks = self._read_tracks()
        if not self.tracks:
            raise DiscError(f'{path.name} is not a CD image (no track list)')
        first = self.tracks[0]
        self.data_start = first.pregap if first.pgtype.startswith('V') else 0
        self.cache: dict[int, bytes] = {}

    def _read_map(self) -> None:
        f = self.handle
        if self.codecs[0] == b'\0\0\0\0':            # an uncompressed CHD: a 4-byte hunk number each
            f.seek(self.mapoffset)
            raw = f.read(4 * self.hunks)
            self.map = [(NONE, self.hunkbytes, struct.unpack_from('>I', raw, 4 * i)[0] * self.hunkbytes)
                        for i in range(self.hunks)]
            return
        f.seek(self.mapoffset)
        h = f.read(16)
        mapbytes = struct.unpack('>I', h[0:4])[0]
        firstoffs = int.from_bytes(h[4:10], 'big')
        lengthbits, selfbits, parentbits = h[12], h[13], h[14]
        bits = Bits(f.read(mapbytes))
        decode = huffman_map_decoder(bits)
        kinds, last, repeat = [], 0, 0
        for _ in range(self.hunks):
            if repeat:
                kinds.append(last)
                repeat -= 1
                continue
            v = decode()
            if v == RLE_SMALL:
                kinds.append(last)
                repeat = 2 + decode()
            elif v == RLE_LARGE:
                kinds.append(last)
                repeat = 2 + 16 + (decode() << 4)
                repeat += decode()
            else:
                kinds.append(v)
                last = v
        self.map = []
        offset, last_self, last_parent = firstoffs, 0, 0
        for n, kind in enumerate(kinds):
            if TYPE_0 <= kind <= TYPE_3:
                length = bits.read(lengthbits)
                self.map.append((kind, length, offset))
                offset += length
                bits.read(16)                        # crc
            elif kind == NONE:
                self.map.append((NONE, self.hunkbytes, offset))
                offset += self.hunkbytes
                bits.read(16)
            elif kind == SELF:
                last_self = bits.read(selfbits)
                self.map.append((SELF, 0, last_self))
            elif kind == PARENT:
                last_parent = bits.read(parentbits)
                self.map.append((PARENT, 0, last_parent))
            elif kind in (SELF_0, SELF_1):
                if kind == SELF_1:
                    last_self += 1
                self.map.append((SELF, 0, last_self))
            elif kind == PARENT_SELF:
                last_parent = n * self.hunkbytes // self.unitbytes
                self.map.append((PARENT, 0, last_parent))
            elif kind in (PARENT_0, PARENT_1):
                if kind == PARENT_1:
                    last_parent += self.hunkbytes // self.unitbytes
                self.map.append((PARENT, 0, last_parent))
            else:
                raise DiscError('the CHD map is damaged (a hunk type)')

    def _read_tracks(self) -> list:
        tracks, at = [], self.metaoffset
        while at:
            self.handle.seek(at)
            h = self.handle.read(16)
            tag, length, nxt = h[:4], int.from_bytes(h[5:8], 'big'), struct.unpack('>Q', h[8:16])[0]
            data = self.handle.read(length).rstrip(b'\0').decode('ascii', 'replace')
            if tag in (b'CHT2', b'CHTR', b'CHCD'):
                fields = dict(re.findall(r'(\w+):(\S+)', data))
                t = Track(int(fields.get('TRACK', len(tracks) + 1)), fields.get('TYPE', ''), int(fields.get('FRAMES', 0)))
                t.pregap, t.pgtype = int(fields.get('PREGAP', 0)), fields.get('PGTYPE', '')
                tracks.append(t)
            at = nxt
        for t in tracks:
            t.kind = 'AUDIO' if t.kind == 'AUDIO' else t.kind.replace('_RAW', '/2352')
        return sorted(tracks, key=lambda t: t.number)

    def hunk(self, n: int) -> bytes:
        if n in self.cache:
            return self.cache[n]
        kind, length, offset = self.map[n]
        if kind == SELF:
            data = self.hunk(offset)
        elif kind == PARENT:
            raise DiscError(f'{self.path.name} needs its parent CHD')
        elif kind == NONE:
            self.handle.seek(offset)
            data = self.handle.read(self.hunkbytes)
        else:
            self.handle.seek(offset)
            data = self._cd_decompress(self.codecs[kind], self.handle.read(length))
        if len(self.cache) > 64:
            self.cache.clear()
        self.cache[n] = data
        return data

    def _cd_decompress(self, codec: bytes, src: bytes) -> bytes:
        """MAME's chd_cd_decompressor: the sectors' and the subcodes' streams, then sync headers back."""
        base = CD_CODECS.get(codec)
        if base is None:
            raise DiscError(f'{self.path.name} has data compressed with {codec.decode("ascii", "replace")}, '
                            'which this setup cannot read: convert it again with chdman createcd -c cdlz,cdzl')
        frames = self.hunkbytes // FRAME
        ecc_bytes = (frames + 7) // 8
        size_bytes = 2 if self.hunkbytes < 65536 else 3
        base_len = int.from_bytes(src[ecc_bytes:ecc_bytes + size_bytes], 'big')
        start = ecc_bytes + size_bytes
        sectors = base(src[start:start + base_len], frames * SECTOR)
        sub_codec = zstd_decompress if codec == b'cdzs' else inflate
        subcodes = sub_codec(src[start + base_len:], frames * SUBCODE)
        out = bytearray()
        for i in range(frames):
            sector = bytearray(sectors[i * SECTOR:(i + 1) * SECTOR])
            if src[i // 8] & (1 << (i % 8)):         # sync header (and ECC) left out: the sync back
                sector[:12] = SYNC
            out += sector + subcodes[i * SUBCODE:(i + 1) * SUBCODE]
        return bytes(out)

    def raw(self, lba: int) -> bytes:
        frame = self.data_start + lba
        per_hunk = self.hunkbytes // FRAME
        h = self.hunk(frame // per_hunk)
        at = (frame % per_hunk) * FRAME
        return h[at:at + SECTOR]

    def close(self) -> None:
        self.handle.close()


# ---- the disc, whichever image ---------------------------------------------

class Disc:
    def __init__(self, image):
        self.image, self.tracks = image, image.tracks

    def __enter__(self):
        return self

    def __exit__(self, *error):
        self.image.close()
        return False

    def read(self, lba: int) -> bytes:
        return user_data(self.image.raw(lba))

    def _find(self, lba: int, size: int, name: str):
        name = name.upper()
        for at in range(0, min(size, 64 * 2048), 2048):
            s = self.read(lba + at // 2048)
            i = 0
            while i < 2048 and s[i]:
                n = s[i + 32]
                entry = s[i + 33:i + 33 + n].decode('ascii', 'replace').upper().split(';')[0]
                if entry == name:
                    return struct.unpack_from('<I', s, i + 2)[0], struct.unpack_from('<I', s, i + 10)[0]
                i += s[i]
        return None

    def read_file(self, path: str) -> bytes | None:
        pvd = self.read(16)
        if pvd[1:6] != b'CD001':
            return None
        lba, size = struct.unpack_from('<I', pvd, 156 + 2)[0], struct.unpack_from('<I', pvd, 156 + 10)[0]
        for part in path.replace('\\', '/').strip('/').split('/'):
            found = self._find(lba, size, part)
            if not found:
                return None
            lba, size = found
        out = b''.join(self.read(lba + i) for i in range(-(-size // 2048)))
        return out[:size]


def open_disc(path) -> Disc:
    path = Path(path)
    if not path.is_file():
        raise DiscError(f'{path} does not exist')
    suffix = path.suffix.lower()
    if suffix == '.chd':
        return Disc(ChdDisc(path))
    if suffix in ('.cue', '.bin'):
        return Disc(CueDisc(cue_for(path)))
    raise DiscError(f'{path.name}: a .cue, .bin or .chd file is needed')
