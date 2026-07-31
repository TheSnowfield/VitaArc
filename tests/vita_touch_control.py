"""Control the synthetic Vita-panel fixture and convert its game-frame artifacts.

This communicates only with the standalone test SELF, never the Windows UI.
"""
import argparse
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parent.parent
MAILBOX = ROOT / "build/vita3k/runtime/portable/fs/ux0/vitaarc/touch-probe"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("id", type=int)
    parser.add_argument("command", nargs="*", help="capture/tap/drag/pair/quit and numeric arguments")
    parser.add_argument("--pulse-pairs", type=int, help="queue alternating pairs on all four observed ground lanes")
    args = parser.parse_args()
    if not 0 <= args.id <= 9999:
        parser.error("id must be 0..9999")
    if args.pulse_pairs is not None:
        if args.command or not 1 <= args.pulse_pairs <= 100 or args.id + args.pulse_pairs > 10000:
            parser.error("pulse-pairs must be 1..100, with no other command")
        for offset in range(args.pulse_pairs):
            command = MAILBOX / f"{args.id + offset:04d}.cmd"
            if command.exists():
                parser.error("preserve existing commands; use the next id")
        for offset in range(args.pulse_pairs):
            command = MAILBOX / f"{args.id + offset:04d}.cmd"
            x1, x2 = (180, 580) if offset % 2 == 0 else (380, 780)
            temporary = command.with_suffix(".tmp")
            temporary.write_text(f"pair {x1} 460 {x2} 460 2 1\n")
            temporary.rename(command)
        args.id += args.pulse_pairs - 1
    if args.command:
        if args.command[0] not in ("capture", "tap", "drag", "pair", "quit"):
            parser.error("unknown fixture command")
        command = MAILBOX / f"{args.id:04d}.cmd"
        if command.exists():
            parser.error("preserve existing command; use the next id")
        temporary = command.with_suffix(".tmp")
        temporary.write_text(" ".join(args.command) + "\n")
        temporary.rename(command)
        if args.command[0] == "quit":
            print("quit requested; verify process exit separately")
            return
    ack = MAILBOX / f"{args.id:04d}.ack"
    deadline = time.monotonic() + 30
    while not ack.exists():
        if time.monotonic() > deadline:
            raise TimeoutError(f"No completed native checkpoint {args.id}; inspect live process/log")
        time.sleep(0.1)
    print(ack.read_text().strip())
    from PIL import Image
    rgba = MAILBOX / f"{args.id:04d}.rgba"
    png = ROOT / f"build/vita-touch-probe/{args.id:04d}.png"
    Image.frombytes("RGBA", (960, 544), rgba.read_bytes()).save(png)
    print(png)


if __name__ == "__main__":
    main()
