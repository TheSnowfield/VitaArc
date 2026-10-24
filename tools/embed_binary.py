"""Embed an immutable resource in a generated C header."""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    data = args.source.read_bytes()
    rows = [", ".join(f"0x{value:02x}" for value in data[index:index + 16])
            for index in range(0, len(data), 16)]
    text = "static const unsigned char __logo_png[] = {\n"
    text += "\n".join("    " + row + "," for row in rows)
    text += "\n};\n"

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
