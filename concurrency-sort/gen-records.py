#!/usr/bin/env python

import os
import sys

mode, out_path = sys.argv[1], sys.argv[2]
records = []

if mode == "empty":
    pass
elif mode == "single":
    records.append(b"\x01\x02\x03\x04" + b"A" * 96)
elif mode == "sorted":
    records.extend([b"\x01\x00\x00\x00" + b"A" * 96, b"\x02\x00\x00\x00" + b"B" * 96])
elif mode == "reverse":
    records.extend([b"\x02\x00\x00\x00" + b"B" * 96, b"\x01\x00\x00\x00" + b"A" * 96])
elif mode == "identical":
    records = [b"\x05\x00\x00\x00" + b"C" * 96] * 10
else:
    count = int(mode.split("_")[1])
    records = [os.urandom(100) for _ in range(count)]

with open(out_path, "wb") as f:
    f.writelines(records)
