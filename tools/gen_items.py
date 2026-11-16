#!/usr/bin/env python3
"""gen_items.py - src/items.h from the game's own item names. Pure stdlib; reads the game folder only.

    python3 -I tools/gen_items.py [--game DIR] [--lang eng] [--out src/items.h]

The names: nativePC/arc/DX9/Msg_<lang>.arc, file soft\\message\\mes_item_<lang> (a GMD inside an MT
Framework 2 ARC, zlib-compressed). An item's id is category << 8 | number, and the game reads its name as
message (category - 1) * 100 + number of that file (BH6.exe 0x4E85E0; the item-get banner 0x9A7D30 the
same). The rest - which ids the inventory takes, what kind each is, how many one slot holds of a case
item, what each weapon's modes load, which keep a count for each of their modes and what each such count is
called - is the game's own code and data, transcribed (build 10112070: 0x574B80 a slot's most, 0x574E90 the
rounds of each mode, 0x575010 the modes, 0x574CA0 the stackable ones, the modes' names 0x159AE48 - messages of
soft\\message\\mes_system_<lang> - the weapon classes uWp<nnnn>); docs/inventory.md has the detail.
"""
import argparse
import os
import struct
import sys
import zlib

DEFAULT_GAME = os.path.expanduser('~/.local/share/Steam/steamapps/common/Resident Evil 6')
GMD_TYPE = 0x242BB29A  # rGUIMessage's type hash in an ARC

# What the inventory holds (its 24 cItems): category 1 (weapons, grenades, the tablets, the spray),
# 2 (ammo), 3 (herbs). The skill points (4) and key items (5) are kept elsewhere.
# The weapons, named; Add offers none (the user, 2026-11-14: a character's weapons come from the Records
# WEAPONS window, which puts them in its saved inventory). Among them Jake's fists, the scenes' own weapons
# (turrets, gadgets, the VTOL's and the helicopter's, Piers' arm, carrying Deborah) and the Triple Shot's
# second copy (in no loadout).
WEAPONS = (0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, 0x109, 0x10A, 0x10B, 0x10C, 0x10D,
           0x10E, 0x10F, 0x110, 0x111, 0x112, 0x117, 0x118, 0x119, 0x11A, 0x11B, 0x11C, 0x11E, 0x11F, 0x121,
           0x122, 0x123, 0x124, 0x125, 0x126, 0x127, 0x128)
GRENADES = {0x113: 5, 0x114: 5, 0x115: 5, 0x116: 5}  # id: a stack's most
TABLETS = 0x11D  # the tablet case (its own slot; never added)
SPRAY = {0x120: 3}
AMMO = {0x201: 150, 0x202: 40, 0x203: 60, 0x204: 60, 0x205: 40, 0x206: 80, 0x207: 60, 0x208: 30, 0x209: 30,
        0x20A: 30, 0x20B: 1, 0x20C: 60, 0x20D: 40, 0x20E: 80}
HERBS = {0x303: 1, 0x304: 1, 0x305: 1, 0x306: 1, 0x307: 1}  # 0x301/0x302 are the floor's (picked up as these)
FLOOR_HERBS = (0x301, 0x302)
# What each weapon's modes load (0x574E90: the weapon and its mode in, the ammo out - one for a mode); the
# knives, the Stun Rod, the fists and the scenes' weapons load none. Add offers a character the ammo its own
# weapons load.
AMMO_OF = {
    0x101: (0x201,), 0x102: (0x201,), 0x103: (0x201, 0x201), 0x104: (0x202,), 0x105: (0x202,), 0x106: (0x20E,),
    0x107: (0x206,), 0x108: (0x207,), 0x109: (0x203,), 0x10A: (0x203,), 0x10B: (0x205, 0x205), 0x10C: (0x201,),
    0x10D: (0x201, 0x201), 0x10E: (0x201, 0x201), 0x10F: (0x204,), 0x110: (0x204, 0x208), 0x111: (0x204,),
    0x112: (0x208, 0x209, 0x20A), 0x117: (0x20C, 0x20D), 0x11B: (0x20B,), 0x128: (0x201, 0x201),
}
# The weapons that keep a count for each mode (0x574B80's packed most), and what each count is called: the
# name of what its mode loads, or - where the modes load the same - the game's own name for the mode (the
# inventory screen's, 0x159AE48: mes_system 1667 "One Gun" for the Wing Shooter's mode 0, its 18 rounds, and
# 1666 "Two Guns" for mode 1, two of them held at once, 36).
MODES = {0x110: ('item', (0x204, 0x208)), 0x112: ('item', (0x208, 0x209, 0x20A)), 0x117: ('item', (0x20C, 0x20D)),
         0x103: ('system', (1667, 1666))}


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


def gmd_messages(data):
    magic, _ver, _lang, _hash, labels, count = struct.unpack_from('<4sIIIII', data, 0)
    if magic != b'GMD\0' or labels != 0:
        raise SystemExit('unexpected GMD header')
    offs = struct.unpack_from(f'<{count}I', data, 0x18)
    base = 0x18 + 4 * count + 4
    out = {}
    for i, o in enumerate(offs):
        if o != 0xFFFFFFFF:
            end = data.index(b'\0', base + o)
            out[i] = data[base + o:end].decode('utf-8', 'replace')
    return out


def message_index(item_id):
    return ((item_id >> 8) - 1) * 100 + (item_id & 0xFF)


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', default=DEFAULT_GAME)
    ap.add_argument('--lang', default='eng')
    ap.add_argument('--out', default='src/items.h')
    a = ap.parse_args()
    arc = os.path.join(a.game, 'nativePC', 'arc', 'DX9', f'Msg_{a.lang}.arc')
    want = f'soft\\message\\mes_item_{a.lang}'
    want_system = f'soft\\message\\mes_system_{a.lang}'
    msgs = system = None
    for name, thash, body in arc_files(arc):
        if thash == GMD_TYPE and name.lower() == want.lower():
            msgs = gmd_messages(body)
        if thash == GMD_TYPE and name.lower() == want_system.lower():
            system = gmd_messages(body)
    if msgs is None or system is None:
        raise SystemExit(f'{want} or {want_system} not in {arc}')

    def name_of(i):
        n = msgs.get(message_index(i), '')
        if not n or n in ('(dummy)', '(delete)', 'sample') or 'dummy' in n.lower():
            raise SystemExit(f'item {i:#x} has no name in {want}')
        return n

    def mode_names(i):
        source, ids = MODES.get(i, (None, ()))
        if source == 'item':
            return [name_of(m) for m in ids]
        out = [system.get(m, '') for m in ids]
        if any(not n for n in out):
            raise SystemExit(f'item {i:#x}: a mode with no name in {want_system}')
        return out

    for w, ammo in AMMO_OF.items():
        if w not in WEAPONS or any(a not in AMMO for a in ammo):
            raise SystemExit(f'item {w:#x}: not a weapon, or loads what is not ammo')
    rows = []  # (id, kind, name, most, offer, modes, ammo)
    for i in WEAPONS:
        ammo = list(dict.fromkeys(AMMO_OF.get(i, ())))  # each kind once, in mode order
        rows.append((i, 'kWeapon', name_of(i), 0, False, mode_names(i), ammo))
    for i, most in GRENADES.items():
        rows.append((i, 'kGrenade', name_of(i), most, True, [], []))
    rows.append((TABLETS, 'kTablets', name_of(TABLETS), 0, False, [], []))
    for i, most in SPRAY.items():
        rows.append((i, 'kRecovery', name_of(i), most, True, [], []))
    for i, most in AMMO.items():
        rows.append((i, 'kAmmo', name_of(i), most, True, [], []))
    for i, most in HERBS.items():
        rows.append((i, 'kRecovery', name_of(i), most, True, [], []))
    for i in FLOOR_HERBS:
        rows.append((i, 'kRecovery', name_of(i), 1, False, [], []))
    rows.sort(key=lambda r: r[0])

    out = []
    out.append('#pragma once')
    out.append('')
    out.append('// The items an inventory holds (cItem\'s mId: category << 8 | number), named as the game names them')
    out.append(f'// (its {want}). Generated by tools/gen_items.py from the game\'s own files - do not edit.')
    out.append('// kind: what the Add list files it under; most: how many one slot holds (a case item\'s);')
    out.append('// offer: Add offers it (no weapon: the Records WEAPONS window gives them); modes: what each count')
    out.append('// of a weapon that keeps a count for each mode is called (what the mode loads, or the game\'s own')
    out.append('// name for the mode); ammo: what a weapon\'s modes load (Add offers a character only these).')
    out.append('')
    out.append('#include <cstdint>')
    out.append('')
    out.append('namespace re6cc::items {')
    out.append('')
    out.append('enum Kind : uint8_t { kWeapon, kGrenade, kAmmo, kRecovery, kTablets };')
    out.append('')
    out.append('struct Info {')
    out.append('  uint16_t id;')
    out.append('  Kind kind;')
    out.append('  const char* name;')
    out.append('  int16_t most;')
    out.append('  bool offer;')
    out.append('  const char* modes[3];')
    out.append('  uint16_t ammo[3];')
    out.append('};')
    out.append('')
    out.append('inline constexpr Info kItems[] = {')
    for i, kind, name, most, offer, modes, ammo in rows:
        m = ', '.join(c_string(x) for x in modes) if modes else ''
        am = ', '.join(f'0x{x:03X}' for x in ammo) if ammo else ''
        out.append(f'    {{0x{i:03X}, {kind}, {c_string(name)}, {most}, {"true" if offer else "false"}, {{{m}}}, '
                   f'{{{am}}}}},')
    out.append('};')
    out.append('inline constexpr int kCount = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));')
    out.append('')
    out.append('inline const Info* find(int id) {')
    out.append('  for (const Info& i : kItems)')
    out.append('    if (i.id == id) return &i;')
    out.append('  return nullptr;')
    out.append('}')
    out.append('')
    out.append('// Weapon `weapon` loads `ammo` in one of its modes.')
    out.append('inline bool loads(int weapon, int ammo) {')
    out.append('  const Info* w = find(weapon);')
    out.append('  if (!w || w->kind != kWeapon) return false;')
    out.append('  for (const uint16_t a : w->ammo)')
    out.append('    if (a && a == ammo) return true;')
    out.append('  return false;')
    out.append('}')
    out.append('')
    out.append('}  // namespace re6cc::items')
    out.append('')
    with open(a.out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print(f'{a.out}: {len(rows)} items ({sum(1 for r in rows if r[4])} offered)')


if __name__ == '__main__':
    sys.exit(main())
