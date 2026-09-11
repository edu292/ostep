#!/bin/env zsh

zmodload zsh/datetime
zmodload zsh/stat

SORT="./sort"
PSORT="./psort"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
printf "CPU Cores Detected:  %d\n" $CORES

if [[ ! -x "$SORT" || ! -x "$PSORT" ]]; then
    print -u2 "ERROR: Executables $SORT or $PSORT not found or not executable."
    exit 1
fi

run_benchmark() {
    local title=$1
    local payload_file=$2
    local payload_mb=$3

    print "\n========================================================"
    print "=> Test Case: $title (${payload_mb}MB)"
    print "========================================================"

    local start=$EPOCHREALTIME
    "$SORT" "$payload_file" bench.sort_out
    local sort_t=$((EPOCHREALTIME - start))

    start=$EPOCHREALTIME
    "$PSORT" "$payload_file" bench.psort_out
    local psort_t=$((EPOCHREALTIME - start))

    if ! cmp -s bench.sort_out bench.psort_out; then
        print -u2 "ERROR: psort output does not match sort for $title!"
        rm -f bench.sort_out bench.psort_out
        exit 1
    fi

    float s=$sort_t
    float p=$psort_t
    ((p <= 0)) && p=0.001
    ((s <= 0)) && s=0.001

    float speedup=$((s / p))
    float efficiency=$(((speedup / CORES) * 100))
    float p_throughput=$((payload_mb / p))
    float s_throughput=$((payload_mb / s))

    printf "Sequential sort:     %.3fs (%.1f MB/s)\n" $s $s_throughput
    printf "Parallel psort:      %.3fs (%.1f MB/s)\n" $p $p_throughput
    printf "Speedup Factor:      %.2fx\n" $speedup
    printf "Parallel Efficiency: %.1f%%\n" $efficiency

    rm -f bench.sort_out bench.psort_out
}

# Generate fixed-size 100-byte record payloads
# 500,000,000 bytes = 5,000,000 records (~476 MiB)

if [[ ! -f bench_random.in ]]; then
    print "Generating 500MB random payload..."
    head -c 500000000 </dev/urandom >bench_random.in
fi
run_benchmark "Random Data" bench_random.in 500

if [[ ! -f bench_sorted.in ]]; then
    print "Generating 500MB pre-sorted payload..."
    "$SORT" bench_random.in bench_sorted.in
fi
run_benchmark "Pre-sorted Data" bench_sorted.in 500

if [[ ! -f bench_identical.in ]]; then
    print "Generating 100MB identical records payload..."
    # 1,000,000 identical records
    python3 -c "import sys; sys.stdout.buffer.write(b'\x01'*100 * 1000000)" >bench_identical.in
fi
run_benchmark "Identical Keys" bench_identical.in 100
