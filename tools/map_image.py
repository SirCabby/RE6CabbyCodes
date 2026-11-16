#!/usr/bin/env python3
"""Lay BH6.exe out as it is in memory: each section at its RVA (raw offsets == RVAs), the headers first.

    python3 -I tools/map_image.py --exe ghidra_proj/BH6.exe --out ghidra_proj/BH6.mapped.exe

BH6.exe (Steam build 10112070) is not packed: the file on disk is the code that runs. The other tools
(mt_classes.py, sig.py, ...) read an image whose file offset is its RVA, so this writes one - the same
shape a memory dump has (DumpImage = 1 writes one from a running game). Pure stdlib.
"""
import argparse
import struct


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    d = open(a.exe, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    size_image = struct.unpack_from("<I", d, pe + 24 + 56)[0]
    size_headers = struct.unpack_from("<I", d, pe + 24 + 60)[0]
    out = bytearray(size_image)
    out[:size_headers] = d[:size_headers]
    sect = pe + 24 + optsz
    for i in range(nsec):
        name = d[sect + 40 * i:sect + 40 * i + 8].rstrip(b"\0").decode("latin-1")
        vs, va, rs, ro = struct.unpack_from("<IIII", d, sect + 40 * i + 8)
        n = min(rs, vs) if vs else rs
        out[va:va + n] = d[ro:ro + n]
        print(f"{name:8s} va {va:#x} vsize {vs:#x} raw {ro:#x}+{rs:#x}")
    open(a.out, "wb").write(out)
    print(f"wrote {a.out} ({len(out):#x} bytes)")


if __name__ == "__main__":
    main()
