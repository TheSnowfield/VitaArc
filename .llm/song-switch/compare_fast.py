from pathlib import Path
import json
import re
import statistics

root = Path(__file__).resolve().parents[2] / 'build/song-switch-fast-20261025'


def records(name):
    rows = []
    row = None
    joins = 0
    for line in (root / name).read_text(errors='replace').splitlines():
        if 'SWITCH join ' in line:
            joins += 1

        match = re.search(r'SWITCH bgm-load channel=0 path=\S*/songs/([^/]+)/base.mp3 us=(\d+)', line)
        if match:
            row = {'song': match[1], 'load_us': int(match[2])}

        match = re.search(r'SWITCH load-frame frame=(\d+) us=(\d+) uploads=(\d+) upload_us=(\d+)', line)
        if match:
            if row is not None:
                row.update(frame=int(match[1]), frame_us=int(match[2]), joins=joins,
                           uploads=int(match[3]), upload_us=int(match[4]))
                rows.append(row)
                row = None

            joins = 0
            continue

        if row is None:
            continue

        match = re.search(r'SWITCH index target=(\d+) us=(\d+) result=(\d+)', line)
        if match and int(match[1]):
            row['index_us'] = int(match[2])

        match = re.search(r'SWITCH mp3-seek target=(\d+) us=(\d+) decodes=(\d+) result=(\d+)', line)
        if match and int(match[1]):
            row.update(target=int(match[1]), seek_us=int(match[2]), decodes=int(match[3]), result=int(match[4]))

        match = re.search(r'SWITCH bgm-play channel=0 before=(\d+) after=(\d+)', line)
        if match:
            row.update(before_ms=int(match[1]), after_ms=int(match[2]))

    assert rows[0]['song'] == 'sayonarahatsukoi'
    return rows[1:]


def summary(rows):
    return {'count': len(rows), 'joins': sum(row['joins'] for row in rows), **{
        key: {'median': statistics.median(row[key] for row in rows),
              'mean': statistics.mean(row[key] for row in rows),
              'max': max(row[key] for row in rows)}
        for key in ('frame_us', 'load_us', 'index_us', 'seek_us', 'decodes', 'upload_us')}}


before = records('baseline.log')
after = records('optimized.log')
assert len(before) == len(after) == 48, (len(before), len(after))
assert [row['song'] for row in before] == [row['song'] for row in after]
assert all(row['result'] == 1 for row in before + after)
assert all(row['before_ms'] == row['after_ms'] == row['target'] // 48 for row in before + after)
assert [row['uploads'] for row in before] == [row['uploads'] for row in after]
assert all(row['joins'] == 1 for row in before)
assert all(row['joins'] == 0 for row in after)
result = {'baseline': {'all': summary(before), 'regular': summary(before[:24]), 'rapid': summary(before[24:])},
          'optimized': {'all': summary(after), 'regular': summary(after[:24]), 'rapid': summary(after[24:])},
          'rows': {'baseline': before, 'optimized': after}}
(root / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({key: value for key, value in result.items() if key != 'rows'}, indent=2))
