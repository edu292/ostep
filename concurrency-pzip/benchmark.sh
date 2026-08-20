#!/bin/env zsh

zmodload zsh/datetime
zmodload zsh/stat

WZIP="../initial-utilities/wzip/wzip"
PZIP="./pzip"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
printf "CPU Cores Detected:  %d\n" $CORES

run_benchmark() {
    local title=$1
    local payload_file=$2
    local payload_mb=$3

    print "\n========================================================"
    print "=> Test Case: $title (${payload_mb}MB)"
    print "========================================================"

    local start=$EPOCHREALTIME
    "$WZIP" "$payload_file" > bench.wzip
    local wzip_t=$((EPOCHREALTIME - start))

    start=$EPOCHREALTIME
    "$PZIP" "$payload_file" > bench.pzip
    local pzip_t=$((EPOCHREALTIME - start))

    if ! cmp -s bench.wzip bench.pzip; then
        print -u2 "ERROR: pzip output does not match wzip for $title!"
        rm -f bench.wzip bench.pzip "$payload_file"
        exit 1
    fi

    float orig_size=$(zstat +size "$payload_file")
    float comp_size=$(zstat +size bench.wzip)
    ((comp_size <= 0)) && comp_size=1

    float comp_ratio=$((orig_size / comp_size))
    float space_saved=$(((1.0 - (comp_size / orig_size)) * 100.0))

    float w=$wzip_t
    float p=$pzip_t
    ((p <= 0)) && p=0.001
    ((w <= 0)) && w=0.001

    float speedup=$((w / p))
    float efficiency=$(((speedup / CORES) * 100))
    float p_throughput=$((payload_mb / p))
    float w_throughput=$((payload_mb / w))

    printf "Sequential wzip:     %.3fs (%.1f MB/s)\n" $w $w_throughput
    printf "Parallel pzip:       %.3fs (%.1f MB/s)\n" $p $p_throughput
    printf "Speedup Factor:      %.2fx\n" $speedup
    printf "Parallel Efficiency: %.1f%%\n" $efficiency
    printf "Compression Ratio:   %.2f:1 (%.1f%% space saved)\n" $comp_ratio $space_saved

    rm -f bench.wzip bench.pzip
}

if [[ ! -f bench_mixed.in ]]; then
    yes "aaaaabbbbbcccccdddddeeeee" | head -c 512M > bench_mixed.in
fi
run_benchmark "Standard Mixed Pattern" bench_mixed.in 512

if [[ ! -f bench_max.in ]]; then
    head -c 512M < /dev/zero | tr '\0' 'a' > bench_max.in
fi
run_benchmark "Max Compression" bench_max.in 512

if [[ ! -f bench_worst.in ]]; then
    head -c 64M < /dev/urandom > bench_worst.in
fi
run_benchmark "Worst Case" bench_worst.in 64
