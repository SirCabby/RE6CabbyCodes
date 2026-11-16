#!/usr/bin/env python3
"""merc_records.py - the Mercenaries' and No Mercy's records in the game's save file: list them, or fill the stage x
character combinations not yet played with S-rank records whose scores are estimated from the ones the save holds.

    python3 -I tools/merc_records.py [--save PATH]                  # list the records
    python3 -I tools/merc_records.py --fill [--seed N]              # what --fill would write (nothing written)
    python3 -I tools/merc_records.py --fill --write --backup DIR    # write it (the save copied into DIR first)

Pure stdlib. The game must be closed (it writes the save as it saves and quits). Steam Cloud uploads the changed file
at the game's next start.

The save (Steam/userdata/<id>/221040/remote/savedata.bin, 0x99F8 bytes; BH6.exe build 10112070 - `docs/records.md`):
a 0x20-byte header - the magic 0x12071801, the size 0x99F8, a checksum (the 32-bit sum of every dword after the
header: sStorage's 0x53FAB0, checked as the save loads, 0x540E50, with the size, the magic and the Steam ID's two
xor'ed words at +0x10) - then the profile's saved half (cSystemInfo +8 on: a field at +X is at X + 0x18 in the file)
and its cGameInfo's part.

The records (cSystemInfo's: the Mercenaries' at +0x24D0 - 0xE45BA0 -, No Mercy's at +0x4690 - 0xE45BE0): for each
group (0 solo, 1 duo), stage (the stage select's row 0-9) and character (the select id 0-35 - the character table's
+4: 0 Leon, 1 Leon's costumes, 2 Helena ... 13 Ada's costumes, 14 Carla, 15 the Agent, 16-19 No Mercy's Coach, Nick,
Ellis, Rochelle) 12 bytes, as the match end writes them (0x55AFD0): +0 had (1), +1 the rank (0 S .. 5 E), +4 the score
(at most 9,999,999), +8 the kills (a byte), +9 the best combo (a byte), +10 the costume played (its bit), +11 the
partner's (duo). The stage select's side panel shows each character's rank and score.

--fill, for each mode and stage, solo only (the save's records are; duo is left as it is): each character the mode
offers with no record gets an S record - its score drawn from that stage's own records (between 2.5% under their
lowest and their best), every score its own, its last two digits as the stage's records end; the kills and combo the
mode's own records hold (a cleared stage's); the costume the character's record on another stage was played in (its
Default, or Costume 1); no partner. Every score stays below the stage's best record: a match's end uploads the stage's
best record of the profile to the online ranking (0x52E8C0 -> 0x52ADC0 / 0x52AF30), and so the one uploaded stays one
played. Nothing else changes (the stages' bests at +0x2160, the medals, the titles - the game recounts its record
titles from these records).
"""
import argparse
import glob
import os
import random
import shutil
import struct
import subprocess
import sys
import time

MAGIC, SIZE, HEADER = 0x12071801, 0x99F8, 0x20
FILE_OF = 0x18  # a cSystemInfo field at +X lies at X + 0x18 in the file
TABLES = (('The Mercenaries', 0x24D0, 16), ('No Mercy', 0x4690, 20))  # each its first record and its characters
GROUPS, STAGES, IDS, REC = 2, 10, 36, 12
RANKS = 'SABCDE'
STAGE_NAMES = ('Urban Chaos', 'Steel Beast', 'Mining the Depths', 'Rail Yard', 'High Seas Fortress', 'The Catacombs',
               'Requiem for War', 'Liquid Fire', 'Rooftop Mission', 'Creature Workshop')
CHARACTERS = ('Leon', "Leon's costumes", 'Helena', "Helena's costumes", 'Chris', "Chris' costumes", 'Piers',
              "Piers' costumes", 'Jake', "Jake's costumes", 'Sherry', "Sherry's costumes", 'Ada', "Ada's costumes",
              'Carla', 'Agent', 'Coach', 'Nick', 'Ellis', 'Rochelle')
LOW = 0.025  # a filled score at most this much under the stage's lowest record


def default_save():
    saves = glob.glob(os.path.expanduser('~/.local/share/Steam/userdata/*/221040/remote/savedata.bin'))
    if len(saves) != 1:
        raise SystemExit(f'{len(saves)} saves found - name one with --save')
    return saves[0]


def checksum(d):
    return sum(struct.unpack_from(f'<{(SIZE - HEADER) // 4}I', d, HEADER)) & 0xFFFFFFFF


def check_save(d, path):
    magic, size, total = struct.unpack_from('<III', d, 0)
    if len(d) != SIZE or magic != MAGIC or size != SIZE:
        raise SystemExit(f'{path}: not the save this tool knows (magic {magic:#x}, size {size:#x}, {len(d)} bytes)')
    if total != checksum(d):
        raise SystemExit(f'{path}: its checksum does not match its data ({total:#x} != {checksum(d):#x})')


def rec_off(base, group, stage, ident):
    return FILE_OF + base + (ident + (stage + group * STAGES) * IDS) * REC


def read_rec(d, off):
    has, rank = d[off], d[off + 1]
    score = struct.unpack_from('<I', d, off + 4)[0]
    return dict(has=has, rank=rank, score=score, kills=d[off + 8], combo=d[off + 9], costume=d[off + 10],
                partner=d[off + 11])


def name_of(ident):
    return CHARACTERS[ident] if ident < len(CHARACTERS) else f'#{ident}'


def list_records(d):
    for mode, base, nids in TABLES:
        print(f'== {mode}')
        for g in range(GROUPS):
            for s in range(STAGES):
                for i in range(IDS):
                    r = read_rec(d, rec_off(base, g, s, i))
                    if not r['has']:
                        continue
                    print(f"  {'solo' if g == 0 else 'duo '} {STAGE_NAMES[s]:19s} {name_of(i):18s} "
                          f"{RANKS[r['rank']] if r['rank'] < 6 else r['rank']} {r['score']:>10,}  kills {r['kills']:3d}"
                          f"  combo {r['combo']:3d}  costume bit {r['costume']:2d}")


def plan_fill(d, rng):
    """The records --fill writes: (offset, record bytes, mode, stage, character, score)."""
    out = []
    for mode, base, nids in TABLES:
        recs = {(s, i): read_rec(d, rec_off(base, 0, s, i)) for s in range(STAGES) for i in range(IDS)}
        played = [r for r in recs.values() if r['has']]
        if not played:
            continue
        # The mode's clears: the kills and combo its records hold most.
        kills = max(set(r['kills'] for r in played), key=[r['kills'] for r in played].count)
        combo = max(set(r['combo'] for r in played), key=[r['combo'] for r in played].count)
        # Each character's costume: its record's on another stage (the first played).
        costume = {}
        for (s, i), r in sorted(recs.items()):
            if r['has'] and i not in costume:
                costume[i] = r['costume']
        used = set(r['score'] for r in played)
        for s in range(STAGES):
            scores = [recs[(s, i)]['score'] for i in range(nids) if recs[(s, i)]['has']]
            missing = [i for i in range(nids) if not recs[(s, i)]['has'] and i in costume]
            if not scores or not missing:
                continue
            best, lowest = max(scores), min(scores)
            lo, hi = int(lowest * (1 - LOW)), best - 1
            # The stage's records' last two digits: their tens (the units drawn).
            tens = [sc // 10 % 10 for sc in scores]
            for i in missing:
                for _ in range(1000):
                    sc = rng.randint(lo, hi)
                    sc = sc - sc % 100 + rng.choice(tens) * 10 + rng.randint(1, 9)
                    if lo <= sc < best and sc not in used:
                        break
                else:
                    raise SystemExit(f'{mode} {STAGE_NAMES[s]}: no score left in {lo}..{best}')
                used.add(sc)
                rec = bytes([1, 0, 0, 0]) + struct.pack('<I', sc) + bytes([kills, combo, costume[i], 0])
                out.append((rec_off(base, 0, s, i), rec, mode, s, i, sc))
    return out


def game_running():
    for pat in ('steamapps/common/Resident Evil 6/BH6\\.exe',):
        if subprocess.run(['pgrep', '-f', pat], capture_output=True).returncode == 0:
            return True
    return subprocess.run(['pgrep', '-x', 'BH6.exe'], capture_output=True).returncode == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--save', default=None)
    ap.add_argument('--fill', action='store_true')
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--backup', default=None, help='the folder the save is copied into before --write')
    ap.add_argument('--seed', type=int, default=20261115)
    a = ap.parse_args()
    path = a.save or default_save()
    with open(path, 'rb') as f:
        d = bytearray(f.read())
    check_save(d, path)
    if not a.fill:
        list_records(d)
        return 0
    plan = plan_fill(d, random.Random(a.seed))
    for off, rec, mode, s, i, sc in plan:
        print(f'  {mode:15s} {STAGE_NAMES[s]:19s} {name_of(i):18s} S {sc:>10,}  kills {rec[8]:3d}  combo {rec[9]:3d}  '
              f'costume bit {rec[10]:2d}')
    print(f'{len(plan)} record(s) to fill')
    if not a.write or not plan:
        return 0
    if game_running():
        raise SystemExit('the game is running - close it first (it writes the save as it saves and quits)')
    if not a.backup:
        raise SystemExit('--write needs --backup DIR')
    os.makedirs(a.backup, exist_ok=True)
    kept = os.path.join(a.backup, os.path.basename(path))
    if os.path.exists(kept):
        raise SystemExit(f'{kept} exists already - choose another --backup')
    shutil.copy2(path, kept)
    for off, rec, *_ in plan:
        d[off:off + REC] = rec
    struct.pack_into('<I', d, 8, checksum(d))
    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(d)
    os.replace(tmp, path)
    with open(path, 'rb') as f:
        again = f.read()
    check_save(again, path)
    for off, rec, *_ in plan:
        if again[off:off + REC] != rec:
            raise SystemExit(f'{path}: the record at {off:#x} did not take')
    print(f'written: {path} (the save before it: {kept}; checksum {checksum(again):#x})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
