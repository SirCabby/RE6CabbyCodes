#!/usr/bin/env python3
"""gen_records.py - src/records_text.h: the texts of the records and skills windows, from the game's own files and
Steam's.

    python3 -I tools/gen_records.py [--game DIR] [--steam DIR] [--lang eng] [--out src/records_text.h]

Pure stdlib; reads only. The game's texts: nativePC/arc/DX9/Msg_<lang>.arc (GMDs inside an MT Framework 2
ARC, zlib-compressed):
  soft\\message\\mes_system_<lang>  10-14 the difficulties (the Records grid's columns), 15 "Chapter",
                                   16-19 the campaigns, 21-28 the characters (the records' rows), 463-512
                                   the 50 achievements' names (one an emblem), the medal categories' labels;
  soft\\message\\mes_file_<lang>    n a skill's name, 100 + n what it does, 200 + n what the game asks before it
                                   sells it (n: the skill's id, its record's fifth byte in the game's skill
                                   table, which the mod reads in the game - the skill screen draws the three
                                   so: BH6.exe 0x9F33E0, 0x9F4E80); 600 + n a title's name, 1800 + n what
                                   earns it (n: the title's message number, the first word of its record in
                                   the game's title table, which the mod reads in the game), 3000-3599 the
                                   medals (each category from a base the mod reads out of the medal page's
                                   name function, BH6.exe 0x5656B0), 3600 + n the Records WEAPONS page's row n
                                   (the base the mod reads out of the page's draw, BH6.exe 0x9E483C), 3700 + n
                                   its ENEMIES page's row n (out of that page's draw, BH6.exe 0x9E4FC0);
  soft\\message\\mes_system_<lang>  2250/2251 the WEAPONS page's "Times Used" and "Enemies Defeated" (the ENEMIES
                                   page's first count too), 2256 the ENEMIES page's "Times Defeated", 992 the
                                   pages' "? ? ?"; the Mercenaries' unlocks window: 807-816 the stages (Rail
                                   Yard ... Liquid Fire: s1200 ... s1209 and No Mercy's s1210 ... s1219 - the
                                   order the match end's unlock notices give them, 2499-2508, by the stage
                                   table's stage of each bit, BH6.exe 0x1797230), 1139-1147 the character
                                   select's characters and 29-32 No Mercy's four (its table's order, BH6.exe
                                   0x1767208), 1149-1152 a costume's own number (Default, Costume 1-3),
                                   1154-1183 each costume by its bit (the character table's order, BH6.exe
                                   0x1797398: 4 a character, then Carla, the Agent);
Steam's: the game's achievement schema Steam keeps in appcache/stats/UserGameStatsSchema_221040.bin (binary
KeyValues): each achievement's API name (ACH01..ACH70, the game's SetAchievement names) and its English name
and description - what the emblem window says an emblem is for. The game's 50 names must equal Steam's first
50 (the emblems' achievements), or the generator stops.

Transcribed from BH6.exe (build 10112070): the medal page's category labels (0x175DE34: 2302, 2276, 2277,
2282, 2303, 3298, 2279, 2280, 2281 - the sixth, 3298, has no text in mes_system; Survivors' team battle label
2304 is used for it).
"""
import argparse
import os
import struct
import sys
import zlib

DEFAULT_GAME = os.path.expanduser('~/.local/share/Steam/steamapps/common/Resident Evil 6')
GMD_TYPE = 0x242BB29A  # rGUIMessage's type hash in an ARC
APP = '221040'

DIFFICULTIES = range(10, 15)
CHAPTER = 15
STORIES = range(16, 20)
CHARACTERS = range(21, 29)
ACHIEVEMENT_NAMES = 463  # + 0..49
MEDAL_LABELS = (2302, 2276, 2277, 2282, 2303, 2304, 2279, 2280, 2281)
SKILL_NAME, SKILL_DESC, SKILL_NEED, SKILL_MSGS = 0, 100, 200, 97  # a skill's state index is below 0x61
TITLE_NAME, TITLE_NEED, TITLE_MSGS = 600, 1800, 1200
MEDAL_FIRST, MEDAL_MSGS = 3000, 600
WEAPON_FIRST, WEAPON_MSGS = 3600, 50  # the page's rows (the profile keeps 50 rows' counts; the game names 43)
WEAPON_USES, WEAPON_KILLS, UNKNOWN = 2250, 2251, 992
ENEMY_FIRST, ENEMY_MSGS = 3700, 90  # the ENEMIES page's rows (the profile keeps 90 entries' counts; the game names 86)
ENEMY_DEFEATED = 2256  # its second count (the first, "Enemies Defeated", is WEAPON_KILLS: the page draws 0x8CB too)
MERC_STAGES = range(807, 817)  # the Mercenaries' stage names, by the stage's number less 1200 (or 1210: No Mercy's)
MERC_CHARACTERS = list(range(1139, 1148)) + list(range(29, 33))  # the character select's characters (No Mercy's four)
MERC_COSTUMES = range(1149, 1153)  # Default, Costume 1-3 (a costume's own number)
MERC_COSTUME_NAMES = range(1154, 1184)  # each costume by its bit ("Leon", "Leon - Costume 1" ... "Carla", "Agent")


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


def kv_parse(d, i=0):
    """Steam's binary KeyValues: (type, key\\0, value) until 8."""
    out = {}
    while True:
        t = d[i]
        i += 1
        if t == 8:
            return out, i
        e = d.index(b'\0', i)
        k = d[i:e].decode('utf-8', 'replace')
        i = e + 1
        if t == 0:
            v, i = kv_parse(d, i)
        elif t == 1:
            e = d.index(b'\0', i)
            v = d[i:e].decode('utf-8', 'replace')
            i = e + 1
        elif t in (2, 3, 4):
            v = struct.unpack_from('<i' if t == 2 else '<f' if t == 3 else '<I', d, i)[0]
            i += 4
        elif t == 7:
            v = struct.unpack_from('<Q', d, i)[0]
            i += 8
        else:
            raise SystemExit(f'Steam schema: unknown KeyValues type {t} at {i}')
        out[k] = v


def clean(s):
    """A message as the panel shows it: one line, no star glyphs (the panel's font has none: the level is
    shown on its own), no dummy or deleted entries."""
    if not s or s in ('(delete)', 'sample', 'Invalid Message') or 'ダミー' in s or 'dummy' in s.lower():
        return ''
    s = s.replace('\r\n', ' ').replace('\n', ' ').replace('★', '').replace('☆', '')
    return ' '.join(s.split())


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', default=DEFAULT_GAME)
    ap.add_argument('--steam', default=None, help='the Steam folder (default: three folders above the game)')
    ap.add_argument('--lang', default='eng')
    ap.add_argument('--out', default='src/records_text.h')
    a = ap.parse_args()
    steam = a.steam or os.path.normpath(os.path.join(a.game, '..', '..', '..'))

    arc = os.path.join(a.game, 'nativePC', 'arc', 'DX9', f'Msg_{a.lang}.arc')
    files = {}
    for name, thash, body in arc_files(arc):
        if thash == GMD_TYPE:
            files[name.lower()] = gmd_messages(body)
    sysm = files.get(f'soft\\message\\mes_system_{a.lang}')
    filem = files.get(f'soft\\message\\mes_file_{a.lang}')
    if sysm is None or filem is None:
        raise SystemExit(f'mes_system / mes_file not in {arc}')

    def need(m, i, what):
        s = clean(m.get(i, ''))
        if not s:
            raise SystemExit(f'{what} (message {i}) has no text')
        return s

    schema_path = os.path.join(steam, 'appcache', 'stats', f'UserGameStatsSchema_{APP}.bin')
    with open(schema_path, 'rb') as f:
        schema, _ = kv_parse(f.read())
    achievements = {}
    for stat in schema[APP]['stats'].values():
        for bit in stat.get('bits', {}).values():
            api = bit['name']
            if not api.startswith('ACH'):
                continue
            n = int(api[3:])
            d = bit['display']
            achievements[n] = (api, clean(d['name']['english']), clean(d['desc']['english']))
    if sorted(achievements) != list(range(1, len(achievements) + 1)) or len(achievements) < 50:
        raise SystemExit(f'{schema_path}: the achievements are not ACH01..ACHnn')
    for i in range(50):
        game = need(sysm, ACHIEVEMENT_NAMES + i, f'achievement {i + 1}')
        if game != achievements[i + 1][1]:
            raise SystemExit(f'achievement {i + 1}: the game calls it "{game}", Steam "{achievements[i + 1][1]}"')

    out = []
    w = out.append
    w('#pragma once')
    w('')
    w('// The texts of the records, skills and Mercenaries windows: the game\'s own (its soft\\message\\mes_system_' + a.lang + ' and')
    w('// mes_file_' + a.lang + ') and its achievements as Steam describes them (UserGameStatsSchema_' + APP + '.bin).')
    w('// Generated by tools/gen_records.py - do not edit.')
    w('')
    w('namespace re6cc::records_text {')
    w('')
    w('// The Records grid: its columns (difficulty 0-4), "Chapter", the campaigns (0-3: the stories) and the')
    w('// characters (0-7; the Agent shares Ada\'s records).')
    w('inline constexpr const char* kDifficulty[5] = {' +
      ', '.join(c_string(need(sysm, i, 'difficulty')) for i in DIFFICULTIES) + '};')
    w(f'inline constexpr const char* kChapter = {c_string(need(sysm, CHAPTER, "chapter"))};')
    w('inline constexpr const char* kStory[4] = {' + ', '.join(c_string(need(sysm, i, 'story')) for i in STORIES) + '};')
    w('inline constexpr const char* kCharacter[8] = {' +
      ', '.join(c_string(need(sysm, i, 'character')) for i in CHARACTERS) + '};')
    w('')
    w('// The achievements, in the game\'s order (index i: Steam\'s ACH<i+1>; the first 50 are the emblems).')
    w('struct Achievement {')
    w('  const char* api;   // the name the game hands Steam\'s SetAchievement')
    w('  const char* name;')
    w('  const char* desc;  // what earns it')
    w('};')
    w(f'inline constexpr int kAchievements = {len(achievements)};')
    w('inline constexpr Achievement kAchievement[kAchievements] = {')
    for n in sorted(achievements):
        api, name, desc = achievements[n]
        w(f'    {{{c_string(api)}, {c_string(name)}, {c_string(desc)}}},')
    w('};')
    w('')
    w(f'// The titles: by a title\'s message number n (its record\'s first word), its name (mes_file {TITLE_NAME} + n,')
    w(f'// stars left out) and what earns it ({TITLE_NEED} + n). "" where the game has none.')
    w(f'inline constexpr int kTitleMsgs = {TITLE_MSGS};')
    w('inline constexpr const char* kTitleName[kTitleMsgs] = {')
    for i in range(TITLE_MSGS):
        w(f'    {c_string(clean(filem.get(TITLE_NAME + i, "")))},')
    w('};')
    w('inline constexpr const char* kTitleNeed[kTitleMsgs] = {')
    for i in range(TITLE_MSGS):
        w(f'    {c_string(clean(filem.get(TITLE_NEED + i, "")))},')
    w('};')
    w('')
    w(f'// The skills (the skill screen\'s list), by a skill\'s id n (its state\'s place in the profile): its name')
    w(f'// (mes_file n), what it does ({SKILL_DESC} + n) and what the game asks before it sells it ({SKILL_NEED} + n).')
    w('// "" where the game has none.')
    w(f'inline constexpr int kSkillMsgs = {SKILL_MSGS};')
    for arr, base in (('kSkillName', SKILL_NAME), ('kSkillDesc', SKILL_DESC), ('kSkillNeed', SKILL_NEED)):
        w(f'inline constexpr const char* {arr}[kSkillMsgs] = {{')
        for i in range(SKILL_MSGS):
            w(f'    {c_string(clean(filem.get(base + i, "")))},')
        w('};')
    w('')
    w(f'// The medals: message {MEDAL_FIRST} + i (each category from its own base, read in the game).')
    w(f'inline constexpr int kMedalFirst = {MEDAL_FIRST}, kMedalMsgs = {MEDAL_MSGS};')
    w('inline constexpr const char* kMedal[kMedalMsgs] = {')
    for i in range(MEDAL_MSGS):
        w(f'    {c_string(clean(filem.get(MEDAL_FIRST + i, "")))},')
    w('};')
    w(f'// The Records WEAPONS page: row n\'s name (mes_file {WEAPON_FIRST} + n; "" where the game has none), the')
    w('// labels of its counts and what it shows for a row it does not name.')
    w(f'inline constexpr int kWeaponFirst = {WEAPON_FIRST}, kWeaponMsgs = {WEAPON_MSGS};')
    w('inline constexpr const char* kWeaponName[kWeaponMsgs] = {')
    for i in range(WEAPON_MSGS):
        w(f'    {c_string(clean(filem.get(WEAPON_FIRST + i, "")))},')
    w('};')
    w(f'inline constexpr const char* kWeaponUses = {c_string(need(sysm, WEAPON_USES, "Times Used"))};')
    w(f'inline constexpr const char* kWeaponKills = {c_string(need(sysm, WEAPON_KILLS, "Enemies Defeated"))};')
    w(f'inline constexpr const char* kUnknown = {c_string(need(sysm, UNKNOWN, "? ? ?"))};')
    w(f'// The Records ENEMIES page: row n\'s name (mes_file {ENEMY_FIRST} + n; "" where the game has none) and the')
    w('// label of its second count (its first is kWeaponKills, "Enemies Defeated").')
    w(f'inline constexpr int kEnemyFirst = {ENEMY_FIRST}, kEnemyMsgs = {ENEMY_MSGS};')
    w('inline constexpr const char* kEnemyName[kEnemyMsgs] = {')
    for i in range(ENEMY_MSGS):
        w(f'    {c_string(clean(filem.get(ENEMY_FIRST + i, "")))},')
    w('};')
    w(f'inline constexpr const char* kEnemyDefeated = {c_string(need(sysm, ENEMY_DEFEATED, "Times Defeated"))};')
    w('')
    w('// The Mercenaries\' unlocks window: a stage by its number less 1200 (No Mercy\'s: less 1210), the character')
    w('// select\'s characters (0-8 the profile\'s, 9-12 No Mercy\'s own four), a costume\'s own number (0 Default, 1-3')
    w('// Costume n) and each costume by its bit (the profile\'s 0-29).')
    w('inline constexpr const char* kMercStage[10] = {' +
      ', '.join(c_string(need(sysm, i, 'stage')) for i in MERC_STAGES) + '};')
    w(f'inline constexpr int kMercCharacters = {len(MERC_CHARACTERS)};')
    w('inline constexpr const char* kMercCharacter[kMercCharacters] = {' +
      ', '.join(c_string(need(sysm, i, 'character')) for i in MERC_CHARACTERS) + '};')
    w('inline constexpr const char* kMercCostume[4] = {' +
      ', '.join(c_string(need(sysm, i, 'costume')) for i in MERC_COSTUMES) + '};')
    w(f'inline constexpr int kMercCostumeNames = {len(MERC_COSTUME_NAMES)};')
    w('inline constexpr const char* kMercCostumeName[kMercCostumeNames] = {')
    for i in MERC_COSTUME_NAMES:
        w(f'    {c_string(need(sysm, i, "costume name"))},')
    w('};')
    w('')
    w('// The medal page\'s categories (0: the campaigns, 1: Agent Hunt, 2/3: the Mercenaries and No Mercy,')
    w('// 4/5: Survivors, 6: Onslaught, 7: Predator, 8: Siege).')
    w('inline constexpr const char* kMedalCategory[9] = {' +
      ', '.join(c_string(need(sysm, i, 'medal category')) for i in MEDAL_LABELS) + '};')
    w('')
    w('}  // namespace re6cc::records_text')
    w('')
    with open(a.out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    titles = sum(1 for i in range(TITLE_MSGS) if clean(filem.get(TITLE_NAME + i, '')))
    medals = sum(1 for i in range(MEDAL_MSGS) if clean(filem.get(MEDAL_FIRST + i, '')))
    skills = sum(1 for i in range(SKILL_MSGS) if clean(filem.get(SKILL_NAME + i, '')))
    weapons = sum(1 for i in range(WEAPON_MSGS) if clean(filem.get(WEAPON_FIRST + i, '')))
    enemies = sum(1 for i in range(ENEMY_MSGS) if clean(filem.get(ENEMY_FIRST + i, '')))
    print(f'{a.out}: {len(achievements)} achievements, {skills} skill names, {titles} title names, {medals} medal texts, '
          f'{weapons} weapon rows, {enemies} enemy rows')


if __name__ == '__main__':
    sys.exit(main())
