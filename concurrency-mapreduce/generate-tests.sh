#!/usr/bin/env bash
set -euo pipefail

mkdir -p tests tests-out tests-data

# ==============================================================================
# tests/pre: Compiles wordcount.c with mapreduce.c
# ==============================================================================
cat <<'EOF' >tests/pre
#!/usr/bin/env bash
gcc -Wall -Werror -Wextra -g -O2 -o wordcount wordcount.c mapreduce.c
gcc -Wall -Werror -Wextra -g -O2 -o inverted-index inverted-index.c mapreduce.c
gcc -Wall -Werror -Wextra -g -O2 -o grep grep.c mapreduce.c
gcc -Wall -Werror -Wextra -g -O2 -o reverse-links reverse-links.c mapreduce.c
EOF
chmod +x tests/pre

# ==============================================================================
# Test 1: Basic Word Count (1 mapper, 1 reducer)
# ==============================================================================
cat <<'EOF' >tests-data/t1.txt
apple banana apple
cherry banana apple
EOF

cat <<'EOF' >tests/1.desc
Word Count: basic correctness (1 mapper, 1 reducer)
EOF
echo "MAPPERS=1 REDUCERS=1 ./wordcount tests-data/t1.txt" >tests/1.run
cat <<'EOF' >tests/1.out
apple 3
banana 2
cherry 1
EOF
: >tests/1.err
echo "0" >tests/1.rc

# ==============================================================================
# Test 2: Edge Cases (Empty file + Single-word file)
# ==============================================================================
: >tests-data/empty.txt
echo "singleton" >tests-data/single.txt

cat <<'EOF' >tests/2.desc
Robustness: empty file and single token
EOF
echo "MAPPERS=2 REDUCERS=1 ./wordcount tests-data/empty.txt tests-data/single.txt" >tests/2.run
cat <<'EOF' >tests/2.out
singleton 1
EOF
: >tests/2.err
echo "0" >tests/2.rc

# ==============================================================================
# Test 3: Sorted Key Order across Multiple Mappers
# ==============================================================================
cat <<'EOF' >tests-data/t3_a.txt
zebra delta alpha
EOF
cat <<'EOF' >tests-data/t3_b.txt
charlie beta delta
EOF

cat <<'EOF' >tests/3.desc
Correctness: strictly ascending keys within partition
EOF
echo "MAPPERS=4 REDUCERS=1 ./wordcount tests-data/t3_a.txt tests-data/t3_b.txt" >tests/3.run
cat <<'EOF' >tests/3.out
alpha 1
beta 1
charlie 1
delta 2
zebra 1
EOF
: >tests/3.err
echo "0" >tests/3.rc

# ==============================================================================
# Test 4: Inverted Index
# ==============================================================================
cat <<'EOF' >tests-data/t4_a.txt
go rust c
c go
EOF
cat <<'EOF' >tests-data/t4_b.txt
c rust
EOF

cat <<'EOF' >tests/4.desc
Inverted Index: file:line emission and custom partitioner
EOF
echo "./inverted-index tests-data/t4_a.txt tests-data/t4_b.txt | LC_ALL=C awk -F': ' '{ n=split(\$2, a, \" \"); asort(a); printf \"%s:\", \$1; for (i=1; i<=n; i++) printf \" %s\", a[i]; print \"\" }' | LC_ALL=C sort" >tests/4.run
cat <<'EOF' >tests/4.out
c: tests-data/t4_a.txt:1 tests-data/t4_a.txt:2 tests-data/t4_b.txt:1
go: tests-data/t4_a.txt:1 tests-data/t4_a.txt:2
rust: tests-data/t4_a.txt:1 tests-data/t4_b.txt:1
EOF
: >tests/4.err
echo "0" >tests/4.rc

# ==============================================================================
# Test 5: Deep Duplicate Key Lists & Valgrind Leak Check
# ==============================================================================
python -c '
with open("tests-data/t5.txt", "w") as f:
    for _ in range(2500):
        f.write("keyA keyB keyC\n")
'

cat <<'EOF' >tests/5.desc
Memory: Valgrind leak and duplicate key management
EOF
echo "valgrind --leak-check=full --error-exitcode=42 --log-file=tests-out/valgrind.log ./wordcount tests-data/t5.txt > /dev/null" >tests/5.run
: >tests/5.out
: >tests/5.err
echo "0" >tests/5.rc

# ==============================================================================
# Test 6: Parallel Scalability (32 files, 8 mappers, 8 reducers)
# ==============================================================================
python -c '
for i in range(32):
    with open(f"tests-data/t6_{i}.txt", "w") as f:
        f.write(f"common {i}\nword_{i % 5} {i}\n")
'

cat <<'EOF' >tests/6.desc
Parallelism: 32 files across 8 mappers and 8 reducers
EOF
echo "./wordcount tests-data/t6_*.txt | LC_ALL=C sort" >tests/6.run

python -c '
from collections import defaultdict
counts = defaultdict(int)
for i in range(32):
    counts["common"] += 1
    counts[str(i)] += 2
    counts[f"word_{i % 5}"] += 1

for k in sorted(counts.keys()):
    print(f"{k} {counts[k]}")
' >tests/6.out
: >tests/6.err
echo "0" >tests/6.rc

# ==============================================================================
# Test 7: Distributed Grep (Selective filter, zero-emission handling)
# ==============================================================================
cat <<'EOF' >tests-data/t7_a.txt
INFO: system startup ok
DEBUG: cache hit
ERROR: connection timed out
WARN: high memory load
EOF

cat <<'EOF' >tests-data/t7_b.txt
INFO: user logged in
DEBUG: query completed
INFO: request served
EOF

cat <<'EOF' >tests-data/t7_c.txt
ERROR: disk full
DEBUG: flushing buffers
ERROR: failed to write block
EOF

cat <<'EOF' >tests/7.desc
Distributed Grep: conditional filtering and zero-emission files
EOF
echo "MAPPERS=3 REDUCERS=1 MATCH_PATTERN=ERROR ./grep tests-data/t7_a.txt tests-data/t7_b.txt tests-data/t7_c.txt | LC_ALL=C sort" >tests/7.run
cat <<'EOF' >tests/7.out
[ERROR] ERROR: connection timed out
[ERROR] ERROR: disk full
[ERROR] ERROR: failed to write block
EOF
: >tests/7.err
echo "0" >tests/7.rc

# ==============================================================================
# Test 8: Reverse Link Graph (In-degree calculation across partitions)
# ==============================================================================
cat <<'EOF' >tests-data/t8_a.txt
pageA pageB
pageC pageB
pageD pageA
EOF

cat <<'EOF' >tests-data/t8_b.txt
pageB pageA
pageE pageB
pageF pageC
EOF

cat <<'EOF' >tests/8.desc
Graph Inversion: reverse web-link in-degree count across partitions
EOF
echo "MAPPERS=2 REDUCERS=4 ./reverse-links tests-data/t8_a.txt tests-data/t8_b.txt | LC_ALL=C sort" >tests/8.run
cat <<'EOF' >tests/8.out
pageA 2
pageB 3
pageC 1
EOF
: >tests/8.err
echo "0" >tests/8.rc

# ==============================================================================
# Test 9: Thread Pool Work-Stealing / Skewed Workload
# 1 huge file + 15 small files with 8 mapper threads
# ==============================================================================
python -c '
with open("tests-data/t9_heavy.txt", "w") as f:
    for _ in range(5000):
        f.write("heavy token stream\n")

for i in range(15):
    with open(f"tests-data/t9_light_{i}.txt", "w") as f:
        f.write(f"light_{i}\n")
'

cat <<'EOF' >tests/9.desc
Robustness: highly skewed workload across 8 mappers and 4 reducers
EOF
echo "MAPPERS=8 REDUCERS=4 ./wordcount tests-data/t9_heavy.txt tests-data/t9_light_*.txt | LC_ALL=C sort" >tests/9.run

python -c '
from collections import defaultdict
counts = defaultdict(int)
counts["heavy"] = 5000
counts["token"] = 5000
counts["stream"] = 5000
for i in range(15):
    counts[f"light_{i}"] = 1

for k in sorted(counts.keys()):
    print(f"{k} {counts[k]}")
' >tests/9.out
: >tests/9.err
echo "0" >tests/9.rc
