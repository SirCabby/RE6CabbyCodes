#!/usr/bin/env python3
"""Print the FSM scripts (and other XFS files) inside an RE6 archive.

    python3 -I tools/fsm_dump.py "$GAME_DIR/nativePC/arc/DX9/s1200Soft.arc" [name-part ...] [--raw]

An FSM (type 0x66B45610, `soft\\stage\\sNNNN\\fsm\\...`) is printed as its nodes, the orders each runs (AreaHit,
EnemyAdd, Timer, RSC ...) with their parameters, and its links with their conditions (`cFSMOrder.RSC[0] == 1` ...).
With --raw (or for any other XFS file: a set list `sNNNN_emNN`, type 0x15302EF4) the object tree is printed whole.

XFS (MT Framework 2, as RE6 writes it): "XFS\\0", u16 version (0x0F), u16, u32, u32, u32 classes, u32 the class
table's size; then a u32 offset (from 0x18) to each class: u32 its hash, u32 its fields, and 24 bytes a field - u32
its name (from 0x18), u8 type (1 an object inline, 2 an object pointer, 3 bool, 4 u8, 5 u16, 6 u32, 9 s16, 0xA s32,
0xC float, 0xE a string, 0x14 a vector), u8 flags (0x20: an array), u16 size, 16 bytes unused. The objects follow at
0x18 + the table's size: u16 (class << 1 | 1; 0xFFFE a null), u16 its serial, u32 its size (counted from this field),
then its fields in their class's order, each a u32 count and that many values (strings inline, NUL-ended; objects
nested). Pure stdlib.
"""
import argparse
import struct
import sys
import zlib

FSM = 0x66B45610


def arc_files(path):
    with open(path, 'rb') as f:
        data = f.read()
    magic, _ver, count = struct.unpack_from('<4sHH', data, 0)
    if magic != b'ARC\0':
        raise SystemExit(f'{path}: not an MT Framework ARC')
    for i in range(count):
        off = 8 + i * 0x50
        name = data[off:off + 64].split(b'\0')[0].decode('ascii', 'replace')
        thash, csize, rsize, doff = struct.unpack_from('<IIII', data, off + 64)
        raw = data[doff:doff + csize]
        real = rsize & 0x1FFFFFFF
        yield name, thash, raw if csize == real else zlib.decompress(raw)


class Obj:
    def __init__(self, cls, serial):
        self.cls, self.serial, self.props = cls, serial, {}


def parse(b):
    if b[:4] != b'XFS\0':
        raise ValueError('not XFS')
    ncls, defsize = struct.unpack_from('<II', b, 0x10)

    def cstr(o):
        e = b.index(b'\0', o)
        return b[o:e].decode('cp932', 'replace'), e + 1

    classes = []
    for o in struct.unpack_from(f'<{ncls}I', b, 0x18):
        a = 0x18 + o
        h, n = struct.unpack_from('<II', b, a)
        fields = []
        for k in range(n):
            no, t, at, sz = struct.unpack_from('<IBBH', b, a + 8 + 24 * k)
            fields.append((cstr(0x18 + no)[0], t, at, sz))
        classes.append((h, fields))
    pos = [0x18 + defsize]

    def take(fmt):
        v = struct.unpack_from(fmt, b, pos[0])
        pos[0] += struct.calcsize(fmt)
        return v[0] if len(v) == 1 else v

    def obj():
        tag, serial = take('<HH')
        if not tag & 1:
            return None
        start = pos[0]
        size = take('<I')
        o = Obj(classes[tag >> 1], serial)
        for name, t, at, sz in o.cls[1]:
            vals = [value(t, sz) for _ in range(take('<I'))]
            o.props[name] = vals if at & 0x20 or len(vals) != 1 else vals[0]
        pos[0] = start + size
        return o

    def value(t, sz):
        if t in (1, 2):
            return obj()
        if t == 0xE:
            s, pos[0] = cstr(pos[0])
            return s
        fmt = {3: '<B', 4: '<B', 5: '<H', 6: '<I', 9: '<h', 0xA: '<i', 0xC: '<f', 0x14: '<4f'}.get(t)
        if fmt:
            return take(fmt)
        pos[0] += sz
        return None

    return obj()


OPS = {0: 'and', 1: 'is set', 2: '==', 3: '==', 4: '<', 5: '<=', 6: '>', 7: '>=', 8: '>=', 16: 'and'}


def cond(n):
    if not isinstance(n, Obj):
        return '?'
    p = n.props
    kids = p.get('mpChildList') or []
    kids = kids if isinstance(kids, list) else [kids]
    if 'mOperator' in p:
        parts = [cond(k) for k in kids]
        op = OPS.get(p['mOperator'], f'op{p["mOperator"]}')
        return f'{op} {parts[0]}' if len(parts) == 1 else '(' + f' {op} '.join(parts) + ')'
    if 'mVariable' in p:
        v = p['mVariable']
        s = f"{v.props.get('mOwnerName')}.{v.props.get('mPropertyName')}" if isinstance(v, Obj) else '?'
        return s + (f"[{p.get('mIndex')}]" if p.get('mIsArray') else '')
    return repr(p.get('mValue'))


def params(o):
    if not isinstance(o, Obj):
        return repr(o)
    out = []
    for k, v in o.props.items():
        if isinstance(v, Obj):
            out.append(f'{k}={{{params(v)}}}')
        elif isinstance(v, list):
            out.append(f'{k}=[{len(v)}]' if v and isinstance(v[0], Obj) else f'{k}={v!r}')
        else:
            out.append(f'{k}={v!r}')
    return ' '.join(out)


def fsm(root):
    conds = {}
    ct = root.props.get('mpConditionTree')
    for t in (ct.props.get('mpTreeList') or []) if isinstance(ct, Obj) else []:
        nm = t.props.get('mName')
        conds[nm.props.get('mId') if isinstance(nm, Obj) else None] = cond(t.props.get('mpRootNode'))

    def cluster(c, ind):
        for n in c.props.get('mpNodeList') or []:
            p = n.props
            print(f"{'  ' * ind}node {p.get('mId')} '{p.get('mName')}'")
            for pr in p.get('mpProcessList') or []:
                print(f"{'  ' * ind}  do {pr.props.get('mContainerName')}: {params(pr.props.get('mpParameter'))}")
            for l in p.get('mpLinkList') or []:
                lp = l.props
                when = conds.get(lp.get('mConditionId'), '?') if lp.get('mExistCondition') else 'always'
                print(f"{'  ' * ind}  -> node {lp.get('mDestinationNodeId')} when {when}")
            if isinstance(p.get('mpSubCluster'), Obj):
                cluster(p['mpSubCluster'], ind + 1)

    cluster(root.props['mpRootCluster'], 0)


def tree(o, ind=0):
    pad = '  ' * ind
    for k, v in o.props.items():
        if isinstance(v, Obj):
            print(f'{pad}{k}: {v.cls[0]:08x}')
            tree(v, ind + 1)
        elif isinstance(v, list) and v and isinstance(v[0], Obj):
            print(f'{pad}{k}: [{len(v)}]')
            for e in v:
                print(f'{pad}  {e.cls[0]:08x}' if isinstance(e, Obj) else f'{pad}  {e!r}')
                if isinstance(e, Obj):
                    tree(e, ind + 2)
        else:
            print(f'{pad}{k} = {v!r}')


def main():
    a = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    a.add_argument('arc')
    a.add_argument('names', nargs='*', help='only the files whose name holds one of these')
    a.add_argument('--raw', action='store_true')
    args = a.parse_args()
    for name, thash, body in arc_files(args.arc):
        if body[:4] != b'XFS\0' or (args.names and not any(s in name for s in args.names)):
            continue
        print(f'== {name} (type {thash:08X})')
        root = parse(body)
        if thash == FSM and not args.raw:
            fsm(root)
        else:
            tree(root)


if __name__ == '__main__':
    sys.exit(main())
