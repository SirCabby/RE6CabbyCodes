#!/usr/bin/env python3
"""Which object a global pointer of BH6.exe holds: the class whose constructor stores `this` there.

    python3 -I tools/globals.py --image ghidra_proj/BH6.mapped.exe --asm ghidra_proj/BH6.asm 0x17cf454 [...]

MT Framework keeps a pointer to each singleton (sMain, sUnit, the s* systems) in a global, written by the
class's constructor (`mov ds:GLOBAL, esi` with esi the object). For each global this finds every store into
it, and the vtable the same function stores into the object it stores (`mov dword [esi], VTABLE`), which
names the class (tools/mt_classes.py's records). Pure stdlib.
"""
import argparse
import re
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
import mt_classes  # noqa: E402

LINE = re.compile(r"^\s+([0-9a-f]+):\s+(.*)$")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True)
    ap.add_argument("--asm", required=True)
    ap.add_argument("globals", nargs="+")
    a = ap.parse_args()
    img = mt_classes.Image(a.image)
    _, _, recs, _ = mt_classes.records(img)
    gd = mt_classes.getdti_index(img)
    vt_name = {}
    for dti, r in recs.items():
        fn = gd.get(dti)
        if fn:
            for v in mt_classes.vtables_of(img, fn):
                vt_name.setdefault(v, r["name"])
    want = {int(g, 0): g for g in a.globals}
    lines = open(a.asm).read().splitlines()
    for i, line in enumerate(lines):
        m = LINE.match(line)
        if not m:
            continue
        ins = m.group(2)
        for g in want:
            if f"ds:{g:#x}," in ins and ins.startswith("mov "):
                reg = ins.split(",")[-1].strip()
                # the vtable stored into the same register's object nearby (before the store)
                cls = []
                for j in range(max(0, i - 400), i):
                    mm = LINE.match(lines[j])
                    if not mm:
                        continue
                    k = re.match(rf"mov\s+DWORD PTR \[{reg}\],0x([0-9a-f]+)$", mm.group(2))
                    if k and int(k.group(1), 16) in vt_name:
                        cls.append(vt_name[int(k.group(1), 16)])
                print(f"{g:#x}: stored at {m.group(1)} from {reg}; vtables stored before: {cls[-3:]}")


if __name__ == "__main__":
    main()
