#!/usr/bin/env bash

mkdir -p tests

# Base test generator
gen_test() {
    local n=$1
    local desc=$2
    local run_cmd=$3
    local rc=$4
    local err=$5

    printf "%s\n" "$rc" >tests/"$n".rc
    printf "%b" "$err" >tests/"$n".err

    # We output the exact command run-tests.sh will 'eval'
    printf "%s\n" "$run_cmd" >tests/"$n".run
    printf "%s\n" "$desc" >tests/"$n".desc
}

setup_test() {
    local n=$1
    local desc=$2
    local mode=$3

    # 1. Generate the binary input data
    ./gen-records.py "$mode" tests/"$n".in

    # 2. Generate the EXPECTED stdout (run-tests.sh checks tests-out/N.out against this)
    ./reference-sort.py tests/"$n".in tests/"$n".out

    # 3. Create the .run file.
    # It runs psort, then cats the actual output file so it goes to standard output.
    # run-tests.sh will capture this stdout and save it to tests-out/N.out for comparison.
    gen_test "$n" "$desc" "./psort tests/$n.in tests/$n.actual_out && cat tests/$n.actual_out" 0 ""
}

# T1: No args
gen_test 1 "No arguments" "./psort" 1 "usage: psort input output\n"
touch tests/1.out # Expecting empty stdout on error

# T2: Bad input file
# Note: perror("in") usually appends "in: No such file or directory"
gen_test 2 "Bad file" "./psort does_not_exist.in tests/2.actual_out" 1 "psort: could not open file\n"
touch tests/2.out # Expecting empty stdout on error

# T3: Empty file
setup_test 3 "Empty file" "empty"

# T4: Single record
setup_test 4 "Single record" "single"

# T5: Already sorted
setup_test 5 "Two records, already sorted" "sorted"

# T6: Reverse sorted
setup_test 6 "Two records, reverse sorted" "reverse"

# T7: Identical keys
setup_test 7 "Multiple identical records" "identical"

# T8: 100 random records
setup_test 8 "100 random records" "random_100"

# T9: 10,000 random records (~1MB)
setup_test 9 "10,000 random records" "random_10000"

# T10: 100,000 random records (~10MB)
setup_test 10 "100,000 random records" "random_100000"
