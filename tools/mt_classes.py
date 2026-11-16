#!/usr/bin/env python3
"""List MT Framework's class records (MtDTI) of BH6.exe (Resident Evil 6, MT Framework 2).

    python3 -I tools/mt_classes.py --image ghidra_proj/BH6.mapped.exe [--grep uPlayer] [--tree NAME]
        [--vtable NAME] [--children NAME]

The image is laid out as in memory (raw offsets == RVAs: tools/map_image.py, or a DumpImage dump).

In BH6.exe the records are not data the linker wrote: each is a static object in .data (.bss) that a
static initializer builds as the program starts -

    push flagsB; push flagsA; push id; push size; push parent (0: the root); push name
    mov ecx, record; call MtDTI::MtDTI; ... mov dword [record], record's own vtable

MtDTI::MtDTI (thiscall, 6 arguments) lays a record out as
    +0x00 vtable   (the record's own: makes an instance of the class)
    +0x04 name
    +0x08 next     (the next sibling)
    +0x0C child    (the first derived class)
    +0x10 parent   (the root record when none is given)
    +0x14 link     (the next record in the id's hash bucket)
    +0x18 size     (bits 0..22) | flagsA << 23 (6 bits) | flagsB << 29
    +0x1C id       (given, or crc32(name) & 0x7FFFFFFF)
so this tool finds every such initializer in the code and rebuilds the tree from their arguments (the
mod itself reads the records the game has built, in memory). Every class answers its record from its
vtable slot 4 (getDTI: `mov eax, record; ret`), which gives its vtable.
Pure stdlib.
"""
import argparse
import re
import struct

B = 0x400000


class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        sect = pe + 24 + optsz
        self.secs = {}
        for i in range(nsec):
            name = d[sect + 40 * i:sect + 40 * i + 8].rstrip(b"\0").decode("latin-1")
            vs, va = struct.unpack_from("<II", d, sect + 40 * i + 8)
            self.secs[name] = (B + va, B + va + vs)
        self.text, self.rdata, self.data = self.secs[".text"], self.secs[".rdata"], self.secs[".data"]

    def u32(self, va):
        return struct.unpack_from("<I", self.d, va - B)[0]

    def u8(self, va):
        return self.d[va - B]

    def cstr(self, va):
        o = va - B
        if not 0 <= o < len(self.d):
            return None
        e = self.d.find(b"\0", o, o + 256)
        return self.d[o:e].decode("latin-1") if e >= o else None

    def inr(self, r, va, n=4):
        return r[0] <= va and va + n <= r[1]


def find_ctor(img):
    """MtDTI::MtDTI: the function MtObject's record's initializer calls."""
    s = img.d.find(b"\0MtObject\0") + 1 + B
    # push name; mov ecx, record; call ctor
    m = re.search(b"\x68" + re.escape(struct.pack("<I", s)) + b"\xb9(....)\xe8(....)", img.d, re.S)
    if not m:
        raise SystemExit("MtObject's record initializer not found")
    at = B + m.start() + 10
    return at + 5 + struct.unpack("<i", m.group(2))[0], struct.unpack("<I", m.group(1))[0]


PUSH = re.compile(rb"(?:\x6a(.)|\x68(....))", re.S)


def records(img):
    ctor, mtobject = find_ctor(img)
    t0, t1 = img.text
    seg = img.d[t0 - B:t1 - B]
    out = {}
    odd = 0
    # every `mov ecx, imm32; call ctor`, then the six pushes before it
    for m in re.finditer(rb"\xb9(....)\xe8(....)", seg, re.S):
        at = t0 + m.start() + 5
        if at + 5 + struct.unpack("<i", m.group(2))[0] != ctor:
            continue
        rec = struct.unpack("<I", m.group(1))[0]
        # walk back over six pushes (2 or 5 bytes each)
        args = []
        p = m.start()
        for _ in range(6):
            if p >= 5 and seg[p - 5] == 0x68:
                args.append(struct.unpack_from("<I", seg, p - 4)[0])
                p -= 5
            elif p >= 2 and seg[p - 2] == 0x6A:
                args.append(struct.unpack_from("<b", seg, p - 1)[0] & 0xFFFFFFFF)
                p -= 2
            else:
                break
        if len(args) != 6:
            odd += 1
            continue
        name_va, parent, size, rid, fa, fb = args  # arg1 was pushed last
        # the record's own vtable, stored right after: mov dword [rec], imm32 (C7 05 rec vt)
        tail = seg[m.end():m.end() + 40]
        vt = 0
        k = tail.find(b"\xc7\x05" + struct.pack("<I", rec))
        if k >= 0:
            vt = struct.unpack_from("<I", tail, k + 6)[0]
        out[rec] = dict(name=img.cstr(name_va) or "", parent=parent, size=size & 0x7FFFFF, id=rid, fa=fa, fb=fb,
                        dtivt=vt, init=at - 5 - (m.start() - p))
    return ctor, mtobject, out, odd


def getdti_index(img):
    """record -> getDTI function, from every `mov eax, imm32; ret` in the code."""
    t0, t1 = img.text
    seg = img.d[t0 - B:t1 - B]
    out = {}
    for m in re.finditer(rb"\xb8(....)\xc3", seg, re.S):
        out.setdefault(struct.unpack("<I", m.group(1))[0], t0 + m.start())
    return out


def vtables_of(img, fn):
    r0, r1 = img.rdata
    seg = img.d[r0 - B:r1 - B]
    return [r0 + m.start() - 16 for m in re.finditer(re.escape(struct.pack("<I", fn)), seg) if m.start() % 4 == 0]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True)
    ap.add_argument("--grep")
    ap.add_argument("--tree")
    ap.add_argument("--vtable")
    ap.add_argument("--children")
    a = ap.parse_args()
    img = Image(a.image)
    ctor, mtobject, recs, odd = records(img)
    root = None
    byname = {}
    for dti, r in recs.items():
        byname.setdefault(r["name"], dti)
    children = {}
    for dti, r in recs.items():
        children.setdefault(r["parent"], []).append(dti)

    def chain(dti):
        names = []
        while dti in recs and len(names) < 40:
            names.append(recs[dti]["name"])
            dti = recs[dti]["parent"]
        return names

    gd = getdti_index(img)

    def vt(dti):
        fn = gd.get(dti)
        v = vtables_of(img, fn) if fn else []
        return v[0] if v else 0

    if a.vtable:
        dti = byname[a.vtable]
        v = vt(dti)
        r = recs[dti]
        print(f"{a.vtable}: record {dti:#x}, vtable {v:#x}, size {r['size']:#x}, id {r['id']:#x}, "
              f"chain {' < '.join(chain(dti))}")
        par = r["parent"]
        pv = vt(par) if par in recs else 0
        i = 0
        while v and img.inr(img.text, img.u32(v + 4 * i), 1):
            f = img.u32(v + 4 * i)
            pf = img.u32(pv + 4 * i) if pv and img.inr(img.text, img.u32(pv + 4 * i), 1) else None
            print(f"  [{i:3d}] +{4*i:#05x} {f:#x}{'' if pf == f else '  (own)' if pf is not None else '  (new slot)'}")
            i += 1
        return
    if a.tree or a.children:
        dti = byname[a.tree or a.children]

        def walk(d, depth):
            r = recs[d]
            print(f"{'  ' * depth}{r['name']}  size {r['size']:#x}  record {d:#x}  vtable {vt(d):#x}")
            if a.children and depth >= 1:
                return
            for c in sorted(children.get(d, []), key=lambda x: recs[x]["name"]):
                walk(c, depth + 1)
        walk(dti, 0)
        return
    print(f"# MtDTI::MtDTI {ctor:#x}; MtObject's record {mtobject:#x}; {len(recs)} records ({odd} initializers of "
          f"another shape skipped)")
    for dti, r in sorted(recs.items(), key=lambda kv: kv[1]["name"]):
        if a.grep and not re.search(a.grep, r["name"]):
            continue
        print(f"{r['name']:40s} record {dti:#x} vtable {vt(dti):#x} size {r['size']:#7x} id {r['id']:#010x}  "
              f"{' < '.join(chain(dti)[1:4])}")


if __name__ == "__main__":
    main()
