#!/usr/bin/env python
import random
import struct
import sys

PAYLOAD_SIZE = 96
RECORD_SIZE = 100


def make_record(key: int, fill: bytes = b"A") -> bytes:
    key_bytes = struct.pack("<I", key)
    payload = (fill * (PAYLOAD_SIZE // len(fill) + 1))[:PAYLOAD_SIZE]
    return key_bytes + payload


def main():
    if len(sys.argv) < 3:
        sys.exit(
            "Usage: gen-records.py <mode> <outfile> [count|values] [seed] [--golden <golden_file>]"
        )

    mode = sys.argv[1]
    outfile = sys.argv[2]

    mode = sys.argv[1]
    outfile = sys.argv[2]
    arg3 = sys.argv[3] if len(sys.argv) > 3 else ""
    seed = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4].isdigit() else 42
    match mode:
        case "empty":
            records = []
        case "fixed":
            vals = [int(x) for x in arg3.split(",") if x]
            records = [make_record(v, fill=f"rec_{v}_".encode()) for v in vals]
        case "random":
            count = int(arg3) if arg3 else 0
            rng = random.Random(seed)
            fill = b"rnd_" * 24
            records = [
                struct.pack("<I", rng.getrandbits(32)) + fill for _ in range(count)
            ]
        case "sorted":
            count = int(arg3) if arg3 else 0
            fill = b"asc_" * 24
            records = [struct.pack("<I", i) + fill for i in range(count)]
        case "reverse":
            count = int(arg3) if arg3 else 0
            fill = b"desc" * 24
            records = [struct.pack("<I", count - i) + fill for i in range(count)]
        case "identical":
            count = int(arg3) if arg3 else 0
            rec = struct.pack("<I", 1337) + (b"same" * 24)
            records = [rec] * count
        case _:
            sys.exit(f"Unknown mode: {mode}")

    with open(outfile, "wb") as f:
        f.writelines(records)

    if "--golden" in sys.argv:
        g_idx = sys.argv.index("--golden") + 1
        golden_file = sys.argv[g_idx]
        sorted_records = sorted(records, key=lambda r: struct.unpack("<I", r[:4])[0])
        with open(golden_file, "wb") as f:
            f.writelines(sorted_records)


if __name__ == "__main__":
    main()
