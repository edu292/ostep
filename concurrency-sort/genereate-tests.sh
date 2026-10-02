#!/usr/bin/env bash
set -euo pipefail

mkdir -p tests
mkdir -p tests-out

create_error_test() {
    local t=$1
    local desc=$2
    local cmd=$3
    local errmsg=$4

    echo "$desc" >"tests/$t.desc"
    echo "$cmd" >"tests/$t.run"
    echo "1" >"tests/$t.rc"
    : >"tests/$t.out"
    echo "$errmsg" >"tests/$t.err"
}

create_sort_test() {
    local t=$1
    local desc=$2
    local mode=$3
    local extra_arg=$4
    local seed=${5:-42}

    echo "$desc" >"tests/$t.desc"
    echo "0" >"tests/$t.rc"
    : >"tests/$t.out"
    : >"tests/$t.err"

    cat <<EOF >"tests/$t.pre"
./gen-records.py "$mode" "tests-out/in_$t.bin" "$extra_arg" "$seed" --golden "tests-out/expected_$t.bin"
EOF

    cat <<EOF >"tests/$t.run"
./psort tests-out/in_$t.bin tests-out/out_$t.bin && cmp -s tests-out/out_$t.bin tests-out/expected_$t.bin
EOF

    cat <<EOF >"tests/$t.post"
rm -f tests-out/in_$t.bin tests-out/out_$t.bin tests-out/expected_$t.bin
EOF
}

# --- Test Definitions (1 to 16) ---

# 1-3: Argument Handling & Input Validation
create_error_test 1 "No arguments provided" "./psort" "usage: psort input output"
create_error_test 2 "Too few arguments provided" "./psort only_one_arg" "usage: psort input output"
create_error_test 3 "Non-existent input file" "./psort tests-out/non_existent_file.bin tests-out/out_3.bin" "psort: could not open file"

# 4-6: Boundary & Degenerate Cases
create_sort_test 4 "Empty file (0 records)" "empty" ""
create_sort_test 5 "Single record" "fixed" "42"
create_sort_test 6 "Two records already sorted" "fixed" "10,20"
create_sort_test 7 "Two records reversed" "fixed" "99,12"

# 8-10: Small Sequences & Duplicates
create_sort_test 8 "Small sequence with negative numbers" "fixed" "5,10,0,99,1000,42"
create_sort_test 9 "Identical keys (100 records)" "identical" "100"
create_sort_test 10 "Keys with heavy duplicates" "fixed" "3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3,2,3,8,4,6"

# 11-13: Presorted and Adversarial Distributions
create_sort_test 11 "Already sorted (1,000 records)" "sorted" "1000"
create_sort_test 12 "Reverse sorted (1,000 records)" "reverse" "1000"
create_sort_test 13 "Power-of-two size (1,024 records, random keys)" "random" "1024" 101

# 14-16: Concurrency & Stress Tests (Multithread partitioning)
create_sort_test 14 "Prime record count across threads (1,543 records)" "random" "1543" 202
create_sort_test 15 "Moderate dataset (10,000 records)" "random" "10000" 303
create_sort_test 16 "Heavy dataset (50,000 records / 5MB)" "random" "50000" 404

echo "Generated 16 tests in tests/ successfully."
