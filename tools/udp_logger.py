#!/usr/bin/env python3

import argparse
import socket
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="VitaArc UDP log receiver")
    parser.add_argument("--host", default="0.0.0.0", help="listen address")
    parser.add_argument("--port", type=int, default=23333, help="listen port")
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((args.host, args.port))
        print(f"Listening for UDP logs on {args.host}:{args.port}", flush=True)

        try:
            while True:
                data, _ = server.recvfrom(65535)
                sys.stdout.write(data.decode("utf-8", errors="replace"))
                if not data.endswith(b"\n"):
                    sys.stdout.write("\n")
                sys.stdout.flush()
        except KeyboardInterrupt:
            print("\nStopped.", flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
