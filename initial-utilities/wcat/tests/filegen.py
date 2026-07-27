#! /usr/bin/env python

import random
import string

for _ in range(1000000):
    x = ""
    for _ in range(40):
        x += random.choice(string.ascii_letters + " ")
    print(x)
