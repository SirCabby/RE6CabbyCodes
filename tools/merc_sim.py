#!/usr/bin/env python3
"""Simulate a Mercenaries stage's spawner (its EmSet script) with every enemy killed as it comes.

    python3 -I tools/merc_sim.py "$GAME_DIR/nativePC/arc/DX9/s1206Soft.arc" [--body 1.5] [--widen] [--nowait]

The script's nodes as the game runs them (docs/modes.md, "How a stage sends its enemies"): a node's EnemyAdd stocks
its groups' unsent set entries (group by group, each entry its own mID) and sends one while fewer than mMinEmNum are
in play and its wait has run; a body stays in play --body seconds; EnemyCheck counts the dead into FreeS32[n] among
its target (mTarget: 0 its listed entries, 2 its group mGroupNo's, 5 every set entry of the stage - the order's
entry 0xD58FC0 and frame 0xD590C0); a Timer sets its FreeFlag after its time; RSC[0] is 0 (AreaHit finding the
player in every area, as the mod has it); a link with no condition is taken. --widen: each sender also stocks the
groups of every sender before it (the mod's fix of 2026-11-14). --nowait: a sender sends whenever fewer than its
mMinEmNum are in play, its mWaitTimer not waited for (the mod's of 2026-11-15). Prints the kills reached, the node it
ends in and when the stage's clear came (150 kills, No Mercy's s1210-s1219 300). Pure stdlib.
"""
import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fsm_dump import arc_files, parse, Obj  # noqa: E402


def main():
    a = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    a.add_argument('arc')
    a.add_argument('--body', type=float, default=1.5, help='how long a body stays in play (s)')
    a.add_argument('--widen', action='store_true')
    a.add_argument('--nowait', action='store_true')
    args = a.parse_args()
    entries, emset = {}, None
    for name, thash, body in arc_files(args.arc):
        m = re.search(r'_em(\d\d)$', name)
        if m and thash == 0x15302EF4:
            infos = parse(body).props.get('mSetInfos') or []
            infos = infos if isinstance(infos, list) else [infos]
            entries[int(m.group(1))] = [i.props.get('mID') for i in infos if isinstance(i, Obj)]
        elif re.search(r'\\EmSet\d+$', name):
            emset = parse(body)
    if emset is None:
        raise SystemExit('no EmSet script in that archive')
    conds = {t.props['mName'].props['mId']: t.props['mpRootNode'] for t in emset.props['mpConditionTree'].props['mpTreeList']}
    nodes = {}
    for nd in emset.props['mpRootCluster'].props['mpNodeList']:
        d = {'add': None, 'check': [], 'links': [], 'timer': None}
        for pr in nd.props.get('mpProcessList') or []:
            kind, p = pr.props.get('mContainerName'), pr.props.get('mpParameter').props
            if kind == 'EnemyAdd':
                d['add'] = (p['mStockGroup'], p['mMinEmNum'], p['mWaitTimer'], p['mRemainEmNum'])
            elif kind == 'Timer':
                d['timer'] = (p['mEndTime'], p['mFlagPrm'].props['mIndex'])
            elif kind == 'EnemyCheck':
                listed = [(e.props['mNo0'], e.props['mNo1']) for e in p['mList'].props['mArray'].props['mpArray']]
                d['check'].append((p['mResultNum'], p.get('mTarget', 0), p.get('mGroupNo', 0), listed))
        for ln in nd.props.get('mpLinkList') or []:
            d['links'].append((ln.props['mDestinationNodeId'], conds.get(ln.props['mConditionId'])))
        nodes[nd.props['mId']] = d
    used, dead, free_s32, flags = set(), set(), collections.defaultdict(int), {}

    def value(c):
        p = c.props
        kids = p.get('mpChildList') or []
        kids = kids if isinstance(kids, list) else [kids]
        if 'mOperator' in p:
            v = [value(k) for k in kids]
            if not v:
                return True
            return {8: lambda: v[0] >= v[1], 3: lambda: v[0] == v[1], 1: lambda: any(v), 16: lambda: all(v)}.get(
                p['mOperator'], lambda: False)()
        if 'mVariable' in p:
            n = p['mVariable'].props['mPropertyName']
            return {'FreeS32': free_s32[p['mIndex']], 'FreeFlag': flags.get(p['mIndex'], 0), 'mCntState': 1e9}.get(n, 0)
        return p.get('mValue', 0)

    stage = re.search(r's(\d{4})', os.path.basename(args.arc))
    clear_at = 300 if stage and 1210 <= int(stage.group(1)) <= 1219 else 150
    cur, t, dt, kills, last, seen, cleared = 0, 0.0, 1 / 30, 0, 0.0, 0, None
    stock, bodies, built, entered, wait = [], [], None, 0.0, 0.0
    while t - last < 30 and t < 1800:
        nd = nodes[cur]
        if built != cur:
            if nd['add']:
                mask = nd['add'][0] | (seen if args.widen else 0)
                seen |= nd['add'][0]
                stock = [(g, i) for g in range(32) if mask >> g & 1 for i in entries.get(g, []) if (g, i) not in used]
                wait = 0.0
            built, entered = cur, t
            if nd['timer']:
                flags[nd['timer'][1]] = 0
        if nd['timer'] and t - entered >= nd['timer'][0]:
            flags[nd['timer'][1]] = 1
        bodies = [b for b in bodies if b > t]
        if nd['add'] and stock and len(bodies) < nd['add'][1]:
            if wait <= 0 or args.nowait:
                e = stock.pop(0)
                used.add(e)
                dead.add(e)
                kills, last = kills + 1, t
                if kills == clear_at:
                    cleared = t
                bodies.append(t + args.body)
                wait = nd['add'][2]
            else:
                wait -= dt
        if nd['add'] and nd['add'][3] < 32:
            free_s32[nd['add'][3]] = len(stock)
        for n, target, group, listed in nd['check']:
            if target not in (0, 2, 5):
                raise SystemExit(f'node {cur}: an EnemyCheck of target {target} (not modelled)')
            free_s32[n] = sum(1 for e in dead if target == 5 or (target == 2 and e[0] == group) or e in listed)
        for dest, c in nd['links']:
            if c is None or value(c):
                cur = dest
                break
        t += dt
    clear = f'the clear ({clear_at}) at {cleared:.0f} s' if cleared is not None else f'no clear ({clear_at})'
    print(f'{os.path.basename(args.arc)}: {kills} kills, ending in node {cur} after {t:.0f} s - {clear} '
          f'({sum(len(v) for v in entries.values())} set entries){" - widened" if args.widen else ""}'
          f'{" - no waits" if args.nowait else ""}')


if __name__ == '__main__':
    sys.exit(main())
