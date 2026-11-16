#!/usr/bin/env python3
"""Fill BH6.exe's MT Framework class records in as its static initializers build them, for offline tests.

    python3 -I tools/init_records.py --image ghidra_proj/BH6.mapped.exe --out ghidra_proj/BH6.records.exe

In BH6.exe each class record (MtDTI) is a zero-filled static object that a static initializer builds as the
program starts (tools/mt_classes.py has the layout). An image read from the exe on disk therefore has no
records; one taken from the running game has them all. This writes the image (raw offsets == RVAs) with every
record built as MtDTI::MtDTI builds it - its own vtable, name, parent (the root when none is given), size and
flags, id (the one given; where none is, the name's hash as the constructor makes it: CRC-32's table with no final
inversion, & 0x7FFFFFFF) - each appended to its parent's children and to its id's hash bucket (the 256 the game's
lookup of a class by its key walks: an item's unit, the loader's request), and the root's own vtable, so the mod's
real class walk (src/mt.cpp) and the game's own lookups can run over it (tests/test_discover.cpp). The children's
order is the code's, not the game's (it sorts them at start-up); nothing depends on it. Pure stdlib.
"""
import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(__file__))
import mt_classes  # noqa: E402

B = 0x400000


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    img = mt_classes.Image(a.image)
    ctor, mtobject, recs, odd = mt_classes.records(img)
    out = bytearray(img.d)

    def put(va, v):
        struct.pack_into("<I", out, va - B, v & 0xFFFFFFFF)

    def get(va):
        return struct.unpack_from("<I", out, va - B)[0]

    # The root: the record MtObject's parent defaults to (the constructor's fallback), its vtable stored by its
    # own initializer (mov dword [root], vtable).
    code = img.d[ctor - B:ctor - B + 0x100]
    k = code.find(b"\xc7\x46\x10")  # mov dword [esi+0x10], root
    if k < 0:
        raise SystemExit("the constructor's default parent was not found")
    root = struct.unpack_from("<I", code, k + 3)[0]
    t0, t1 = img.text
    seg = img.d[t0 - B:t1 - B]
    m = seg.find(b"\xc7\x05" + struct.pack("<I", root))
    if m >= 0:
        put(root, struct.unpack_from("<I", seg, m + 6)[0])
    names = {}
    for rec, r in sorted(recs.items(), key=lambda kv: kv[1]["init"]):
        name_va = None
        # the name's address: find it again from the record's initializer (the push before `mov ecx, rec`)
        init = r["init"]
        blob = img.d[init - B:init - B + 0x30]
        j = blob.find(b"\xb9" + struct.pack("<I", rec))
        if j >= 5 and blob[j - 5] == 0x68:
            name_va = struct.unpack_from("<I", blob, j - 4)[0]
        parent = r["parent"] or root
        rid = r["id"]
        if not rid and name_va:  # the name's hash (0xEECA20 from MtDTI::MtDTI: CRC-32, no final inversion)
            o = name_va - B
            rid = (zlib.crc32(img.d[o:img.d.index(b"\0", o)]) ^ 0xFFFFFFFF) & 0x7FFFFFFF
        put(rec + 0x00, r["dtivt"])
        put(rec + 0x04, name_va or 0)
        put(rec + 0x08, 0)
        put(rec + 0x10, parent)
        put(rec + 0x14, 0)
        put(rec + 0x18, (r["size"] & 0x7FFFFF) | ((r["fa"] & 0x3F) << 23) | ((r["fb"] & 7) << 29))
        put(rec + 0x1C, rid)
        names[rec] = name_va
    # children lists: each record appended to its parent's (child, then each next)
    last = {}
    for rec, r in sorted(recs.items(), key=lambda kv: kv[1]["init"]):
        parent = r["parent"] or root
        if parent not in last:
            put(parent + 0x0C, rec)
        else:
            put(last[parent] + 0x08, rec)
        last[parent] = rec
    # the hash buckets (the constructor's `mov eax, [ecx*4 + buckets]`): each record appended to its id's chain
    # (+0x14), in the order the records are built
    k = code.find(b"\x8b\x04\x8d")
    if k < 0:
        raise SystemExit("the constructor's hash buckets were not found")
    buckets = struct.unpack_from("<I", code, k + 3)[0]
    for i in range(256):
        put(buckets + 4 * i, 0)
    for rec, r in sorted(recs.items(), key=lambda kv: kv[1]["init"]):
        slot = buckets + 4 * (get(rec + 0x1C) & 0xFF)
        at = get(slot)
        if not at:
            put(slot, rec)
            continue
        while get(at + 0x14):
            at = get(at + 0x14)
        put(at + 0x14, rec)
    missing = sum(1 for v in names.values() if not v)
    open(a.out, "wb").write(out)
    print(f"{len(recs)} records built under the root {root:#x} ({missing} without a name found; {odd} initializers "
          f"of another shape skipped) -> {a.out}")


if __name__ == "__main__":
    main()
