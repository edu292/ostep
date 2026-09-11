#!/usr/bin/env python

import struct
import sys


def main():
    if len(sys.argv) != 3:
        print("Usage: standard_sort.py <input> <output>")
        sys.exit(1)

    in_file, out_file = sys.argv[1], sys.argv[2]

    with open(in_file, "rb") as f:
        data = f.read()

    record_size = 100
    records = [data[i : i + record_size] for i in range(0, len(data), record_size)]

    records.sort(key=lambda r: struct.unpack("<I", r[:4])[0])

    with open(out_file, "wb") as f:
        f.writelines(records)


if __name__ == "__main__":
    main()
