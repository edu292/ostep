#!/bin/env zsh

zmodload zsh/datetime
zmodload zsh/stat

SORT="./sort"
PSORT="./psort"
GEN="./gen-records.py"

CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
printf "CPU Cores Detected: %d\n" $CORES

run_benchmark() {
    local title=$1
    local payload_file=$2
    local rec_count=$3
    local size_bytes=$((rec_count * 100))
    float size_mb=$((size_bytes / (1024.0 * 1024.0)))

    print "\n========================================================"
    printf "=> Test Case: %s (%.1f MB, %d records)\n" "$title" $size_mb $rec_count
    print "========================================================"

    local start=$EPOCHREALTIME
    "$SORT" "$payload_file" bench.sort
    local sort_t=$((EPOCHREALTIME - start))

    start=$EPOCHREALTIME
    "$PSORT" "$payload_file" bench.psort
    local psort_t=$((EPOCHREALTIME - start))

    if ! cmp -s bench.sort bench.psort; then
        print -u2 "ERROR: psort output does not match sort for $title!"
        rm -f bench.sort bench.psort
        exit 1
    fi

    float s=$sort_t
    float p=$psort_t
    ((p <= 0)) && p=0.0001
    ((s <= 0)) && s=0.0001

    float speedup=$((s / p))
    float efficiency=$(((speedup / CORES) * 100.0))
    float p_throughput=$((size_mb / p))
    float s_throughput=$((size_mb / s))

    printf "Sequential sort:     %.3fs (%.1f MB/s)\n" $s $s_throughput
    printf "Parallel psort:      %.3fs (%.1f MB/s)\n" $p $p_throughput
    printf "Speedup Factor:      %.2fx\n" $speedup
    printf "Parallel Efficiency: %.1f%%\n" $efficiency

    rm -f bench.sort bench.psort
}

# 1. Uniform Random (500 MB ~ 5,000,000 records)
if [[ ! -f bench_random.in ]]; then
    print "Generating random dataset (500 MB)..."
    python "$GEN" random bench_random.in 5000000
fi
run_benchmark "Large Random Keys" bench_random.in 5000000

# 2. Already Sorted (100 MB ~ 1,000,000 records)
if [[ ! -f bench_sorted.in ]]; then
    print "Generating pre-sorted dataset (100 MB)..."
    python "$GEN" sorted bench_sorted.in 1000000
fi
run_benchmark "Pre-Sorted Data" bench_sorted.in 1000000

# 3. Reverse Sorted (100 MB ~ 1,000,000 records)
if [[ ! -f bench_reverse.in ]]; then
    print "Generating reverse-sorted dataset (100 MB)..."
    python "$GEN" reverse bench_reverse.in 1000000
fi
run_benchmark "Reverse Sorted Data" bench_reverse.in 1000000

# 4. Duplicate Keys (100 MB ~ 1,000,000 records)
if [[ ! -f bench_identical.in ]]; then
    print "Generating identical keys dataset (100 MB)..."
    python "$GEN" identical bench_identical.in 1000000
fi
run_benchmark "Identical Keys" bench_identical.in 1000000
