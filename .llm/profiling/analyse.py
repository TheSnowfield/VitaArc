from pathlib import Path
import hashlib
import json
import re
import statistics

root = Path(__file__).resolve().parents[2]
out = root / "build/cpu-profile-20261025"


def windows(path):
    result = []
    current = None
    for line in path.read_text(errors="replace").splitlines():
        start = re.search(r"PROFILE frame=(\d+) window_us=(\d+)", line)
        if start:
            current = {"frame": int(start[1]), "window_us": int(start[2]), "stats": {}}
            result.append(current)
            continue

        if current is None:
            continue

        entry = re.search(r"PROFILE ([\w-]+) calls=(\d+) us=(\d+) peak=(\d+)(?: samples=(\d+))?", line)
        if entry:
            current["stats"][entry[1]] = {
                "calls": int(entry[2]), "us": int(entry[3]), "peak": int(entry[4]),
                "samples": int(entry[5]) if entry[5] else 0,
            }

        buffers = re.search(r"PROFILE buffers=(\d+) deleted=(\d+) collect128_us=(\d+)", line)
        if buffers:
            current.update(buffers=int(buffers[1]), deleted=int(buffers[2]), collect128_us=int(buffers[3]))

    return result


def selection(path):
    rows = []
    for row in windows(path):
        stats = row["stats"]
        expected = {"render": 300, "draw": 38700, "bind": 104400, "attrib": 79800}
        if not all(stats.get(name, {}).get("calls") == count for name, count in expected.items()):
            continue
        if row.get("buffers") != 933 or row.get("deleted") != 0:
            continue
        if not 4900000 <= row["window_us"] <= 5100000:
            continue
        rows.append(row)

    if not rows:
        raise ValueError(f"No matching steady selection windows in {path}")

    mean = statistics.median(row["stats"]["render"]["us"] / 300 for row in rows)
    return {"windows": len(rows), "frames": 300 * len(rows), "render_median_us": mean,
        "collect128_median_us": statistics.median(row["collect128_us"] for row in rows), "raw": rows}


before = selection(out / "baseline-collection.log")
after = selection(out / "optimized.log")
result = {"scene": "Arcaea 1.6.1c, Vicious Labyrinth / Axium Crisis song selection",
    "buffers": 933, "calls_per_frame": {"draw": 129, "bind": 348, "attrib": 266, "buffer-data": 42},
    "baseline": before, "optimized": after,
    "render_reduction_percent": (1 - after["render_median_us"] / before["render_median_us"]) * 100,
    "self_sha256": hashlib.sha256((out / "eboot.bin").read_bytes()).hexdigest(),
    "hardware_verified": False,
    "scope": "Instrumented Vita3K wall times; GL timings sample one call in 64. Not device CPU utilization."}
(out / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({key: value for key, value in result.items() if key not in ("baseline", "optimized")}, indent=2))
for name, value in (("baseline", before), ("optimized", after)):
    print(name, {key: item for key, item in value.items() if key != "raw"})
