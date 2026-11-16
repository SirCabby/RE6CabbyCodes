#!/usr/bin/env python3
"""proton_videos.py - the game's movies under Proton: which of them Valve's transcodes cover, and the fix for
the ones a WoW64 Proton plays blank. Pure stdlib.

    python3 -I tools/proton_videos.py [--game DIR] [--cache FILE] [--fix]

RE6's pre-rendered movies are WMV (nativePC/movie/*.wmv: the logos, the title backgrounds, the cutscene
movie e5120 - Chris's event e2190 and Leon's e1210 - e5110, and projector, a screen in Ada's stage 1000).
Proton decodes no WMV: its media converter (Proton's wine, dlls/winegstreamer/media-converter/videoconv.c)
hashes the movie and plays Valve's Ogg Theora transcode of it out of transcoded_video.foz, in the game's
shader cache (Steam downloads it). With none for that hash it plays a blank video, touches
"placeholder-video-used" beside it and dumps the movie into fozmediav1/video.foz for Valve.

The hash: MurmurHash3 128 (seed 0x4AA61F63) of the movie's first 48 MiB and then 8 MiB every 48 MiB - the x86
variant when the game's unix side is 32-bit, the x64 one under WoW64 (Proton Experimental 11.0 runs RE6 so:
its dump of e5120 is keyed by the x64 hash, 2026-11-13). Valve's file of 2026-11-12 holds four movies under
their x86 key only (e5120, projector and the Japanese logo and title), so a WoW64 Proton plays those blank.
--fix adds each transcode under the movie's other key: the same entry, header and data, under a second name
(a Fossilize database is a list of entries, read to its end). The file is written beside the old one and
moved over it, so a running game opens either whole (the converter opens it at each movie): the fix holds
from the next movie played. The old file is the new one's first part: `truncate -s <old size>` undoes it.
Steam may download the file again (it did on 2026-11-12): run this again then.
"""
import argparse
import os
import struct
import sys

DEFAULT_GAME = os.path.expanduser('~/.local/share/Steam/steamapps/common/Resident Evil 6')
APP_ID = 221040

# videoconv.c: "Changing any of these values will invalidate the transcoded cache."
SEED = 0x4AA61F63
CHUNK = 8 * 1024 * 1024
STRIDE = CHUNK * 6
# media-converter.h: the video databases' tags.
TAG_VIDEODATA, TAG_OGVDATA, TAG_STREAM, TAG_MKVDATA = 0, 1, 2, 3
TRANSCODE_TAGS = (TAG_MKVDATA, TAG_OGVDATA)
MAGIC = b'\x81FOSSILIZEDB'
M64 = (1 << 64) - 1
M32 = (1 << 32) - 1


def hashed_bytes(path):
    """The bytes the converter hashes: its pad reader with HASH_STRIDE (videoconv.c pad_reader_read)."""
    out = []
    with open(path, 'rb') as f:
        size = os.fstat(f.fileno()).st_size
        off = 0
        while off < size:
            got = min(CHUNK, size - off)
            if off + got < STRIDE:
                take, nxt = got, off + got
            elif off < STRIDE:
                take, nxt = STRIDE - off, STRIDE
            else:
                take, nxt = got, off + STRIDE
            f.seek(off)
            out.append(f.read(take))
            off = nxt
    # Only the last piece can end short of a 16-byte block, so the pieces hash as one stream (murmur3.c mixes
    # a short read in as the tail).
    assert all(len(p) % 16 == 0 for p in out[:-1])
    return b''.join(out)


def _rotl64(x, r):
    return ((x << r) | (x >> (64 - r))) & M64


def _fmix64(k):
    k ^= k >> 33
    k = (k * 0xFF51AFD7ED558CCD) & M64
    k ^= k >> 33
    k = (k * 0xC4CEB9FE1A85EC53) & M64
    return k ^ (k >> 33)


def murmur3_x64_128(data, seed=SEED):
    """The key the converter files a movie under when its unix side is 64-bit (WoW64)."""
    c1, c2 = 0x87C37B91114253D5, 0x4CF5AD432745937F
    h1 = h2 = seed
    n16 = len(data) & ~15
    for k1, k2 in struct.iter_unpack('<QQ', data[:n16]):
        k1 = (k1 * c1) & M64
        k1 = ((k1 << 31) | (k1 >> 33)) & M64
        h1 ^= (k1 * c2) & M64
        h1 = ((h1 << 27) | (h1 >> 37)) & M64
        h1 = ((h1 + h2) * 5 + 0x52DCE729) & M64
        k2 = (k2 * c2) & M64
        k2 = ((k2 << 33) | (k2 >> 31)) & M64
        h2 ^= (k2 * c1) & M64
        h2 = ((h2 << 31) | (h2 >> 33)) & M64
        h2 = ((h2 + h1) * 5 + 0x38495AB5) & M64
    tail = data[n16:]
    if len(tail) > 8:
        k2 = int.from_bytes(tail[8:], 'little')
        h2 ^= (_rotl64((k2 * c2) & M64, 33) * c1) & M64
    if tail:
        k1 = int.from_bytes(tail[:8], 'little')
        h1 ^= (_rotl64((k1 * c1) & M64, 31) * c2) & M64
    h1 ^= len(data)
    h2 ^= len(data)
    h1 = (h1 + h2) & M64
    h2 = (h2 + h1) & M64
    h1, h2 = _fmix64(h1), _fmix64(h2)
    h1 = (h1 + h2) & M64
    h2 = (h2 + h1) & M64
    return '%016x%016x' % (h2, h1)


def _rotl32(x, r):
    return ((x << r) | (x >> (32 - r))) & M32


def _fmix32(h):
    h ^= h >> 16
    h = (h * 0x85EBCA6B) & M32
    h ^= h >> 13
    h = (h * 0xC2B2AE35) & M32
    return h ^ (h >> 16)


def murmur3_x86_128(data, seed=SEED):
    """The key the converter files a movie under when its unix side is 32-bit."""
    c1, c2, c3, c4 = 0x239B961B, 0xAB0E9789, 0x38B34AE5, 0xA1E38B93
    h1 = h2 = h3 = h4 = seed
    n16 = len(data) & ~15
    for k1, k2, k3, k4 in struct.iter_unpack('<IIII', data[:n16]):
        k1 = (k1 * c1) & M32
        h1 ^= (((k1 << 15) | (k1 >> 17)) & M32) * c2 & M32
        h1 = ((((h1 << 19) | (h1 >> 13)) & M32) + h2) * 5 + 0x561CCD1B & M32
        k2 = (k2 * c2) & M32
        h2 ^= (((k2 << 16) | (k2 >> 16)) & M32) * c3 & M32
        h2 = ((((h2 << 17) | (h2 >> 15)) & M32) + h3) * 5 + 0x0BCAA747 & M32
        k3 = (k3 * c3) & M32
        h3 ^= (((k3 << 17) | (k3 >> 15)) & M32) * c4 & M32
        h3 = ((((h3 << 15) | (h3 >> 17)) & M32) + h4) * 5 + 0x96CD1C35 & M32
        k4 = (k4 * c4) & M32
        h4 ^= (((k4 << 18) | (k4 >> 14)) & M32) * c1 & M32
        h4 = ((((h4 << 13) | (h4 >> 19)) & M32) + h1) * 5 + 0x32AC3B17 & M32
    tail = data[n16:]
    if len(tail) > 12:
        h4 ^= _rotl32((int.from_bytes(tail[12:], 'little') * c4) & M32, 18) * c1 & M32
    if len(tail) > 8:
        h3 ^= _rotl32((int.from_bytes(tail[8:12], 'little') * c3) & M32, 17) * c4 & M32
    if len(tail) > 4:
        h2 ^= _rotl32((int.from_bytes(tail[4:8], 'little') * c2) & M32, 16) * c3 & M32
    if tail:
        h1 ^= _rotl32((int.from_bytes(tail[:4], 'little') * c1) & M32, 15) * c2 & M32
    n = len(data) & M32
    h1, h2, h3, h4 = h1 ^ n, h2 ^ n, h3 ^ n, h4 ^ n
    h1 = (h1 + h2 + h3 + h4) & M32
    h2, h3, h4 = (h2 + h1) & M32, (h3 + h1) & M32, (h4 + h1) & M32
    h1, h2, h3, h4 = _fmix32(h1), _fmix32(h2), _fmix32(h3), _fmix32(h4)
    h1 = (h1 + h2 + h3 + h4) & M32
    h2, h3, h4 = (h2 + h1) & M32, (h3 + h1) & M32, (h4 + h1) & M32
    return '%08x%08x%08x%08x' % (h4, h3, h2, h1)


def read_foz(path):
    """A Fossilize database's entries: {(tag, key): (offset of the entry, payload size)}; and the file's size."""
    entries = {}
    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        head = f.read(16)
        if head[:12] != MAGIC:
            sys.exit(f'{path}: not a Fossilize database')
        pos = 16
        while pos + 56 <= size:
            f.seek(pos)
            name = f.read(40).decode('ascii')
            psize = struct.unpack('<I', f.read(4))[0]
            entries[(int(name[:8], 16), name[8:])] = (pos, psize)
            pos += 56 + psize
    if pos != size:
        sys.exit(f'{path}: an entry runs past the end ({pos} of {size} bytes)')
    return entries, size


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', default=DEFAULT_GAME)
    ap.add_argument('--cache', help='transcoded_video.foz (default: the game\'s shader cache)')
    ap.add_argument('--fix', action='store_true', help='add the missing keys')
    args = ap.parse_args()

    shader = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(args.game))), 'shadercache', str(APP_ID))
    cache = args.cache or os.path.join(shader, 'transcoded_video.foz')
    entries, old_size = read_foz(cache)
    print(f'{cache}: {len(entries)} entries, {old_size} bytes')

    movie_dir = os.path.join(args.game, 'nativePC', 'movie')
    keys = {}  # key -> movie
    adds = []  # (movie, transcode's tag, its key, the key it is added under)
    print(f'{"movie":<14} {"x86 key (32-bit)":<34} {"x64 key (WoW64)":<34}')
    for name in sorted(os.listdir(movie_dir)):
        if not name.lower().endswith('.wmv'):
            continue
        data = hashed_bytes(os.path.join(movie_dir, name))
        both = (murmur3_x86_128(data), murmur3_x64_128(data))
        found = {k: next((t for t in TRANSCODE_TAGS if (t, k) in entries), None) for k in both}
        for k in both:
            keys[k] = name
        print(f'{name:<14} ' + ' '.join(f'{k} {"in " if found[k] is not None else "-  "}' for k in both))
        have = [k for k in both if found[k] is not None]
        if have and len(have) < 2:
            src = have[0]
            adds.append((name, found[src], src, both[1] if src == both[0] else both[0]))
        elif not have:
            print(f'  {name}: no transcode at all - Proton plays it blank (only Valve can add one)')

    dump = os.path.join(shader, 'fozmediav1', 'video.foz')
    if os.path.exists(dump):
        missed = [k for (t, k) in read_foz(dump)[0] if t == TAG_STREAM]
        print('Proton has missed (fozmediav1/video.foz): ' +
              (', '.join(f'{keys.get(k, "?")} ({k})' for k in missed) or 'none'))

    if not adds:
        print('nothing to add: every movie with a transcode has it under both keys')
        return
    for name, tag, src, dst in adds:
        print(f'{"adding" if args.fix else "would add"}: {name} under {dst} (the transcode of {src})')
    if not args.fix:
        print('run with --fix to add them')
        return

    tmp = cache + f'.tmp{os.getpid()}'
    try:
        with open(cache, 'rb') as src_f, open(tmp, 'wb') as out:
            while True:
                block = src_f.read(1 << 24)
                if not block:
                    break
                out.write(block)
            if out.tell() != old_size:
                sys.exit(f'{cache} changed while it was copied')
            for name, tag, src, dst in adds:
                pos, psize = entries[(tag, src)]
                src_f.seek(pos + 40)
                header_and_data = src_f.read(16 + psize)
                out.write(b'%08x' % tag + dst.encode('ascii') + header_and_data)
            out.flush()
            os.fsync(out.fileno())
        os.chmod(tmp, os.stat(cache).st_mode & 0o7777)
        new_entries, new_size = read_foz(tmp)
        for name, tag, src, dst in adds:
            assert new_entries[(tag, dst)][1] == entries[(tag, src)][1]
        assert len(new_entries) == len(entries) + len(adds)
        os.replace(tmp, cache)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    print(f'{cache}: {len(new_entries)} entries, {new_size} bytes (undo: truncate -s {old_size} it)')


if __name__ == '__main__':
    main()
