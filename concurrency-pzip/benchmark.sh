#!/bin/env zsh

zmodload zsh/datetime

WZIP="../initial-utilities/wzip/wzip"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
PAYLOAD_MB=512

echo "=> Preparing ${PAYLOAD_MB}MB benchmark payload..."
yes "aaaaabbbbbcccccdddddeeeee" | head -c "${PAYLOAD_MB}M" >bench.in

echo "=> Benchmarking Reference wzip..."
start=$EPOCHREALTIME
"$WZIP" bench.in >bench.wzip
wzip_t=$((EPOCHREALTIME - start))

echo "=> Benchmarking pzip..."
start=$EPOCHREALTIME
./pzip bench.in >bench.pzip
pzip_t=$((EPOCHREALTIME - start))

#if ! cmp -s bench.wzip bench.pzip; then
#    echo "ERROR: pzip output does not match wzip on benchmark payload!"
#    rm -f bench.in bench.wzip bench.pzip
#    exit 1
#fi

# Calculate sizes and floating-point metrics
float orig_size=$(wc -c < bench.in)
float comp_size=$(wc -c < bench.wzip)
((comp_size <= 0)) && comp_size=1

float comp_ratio=$((orig_size / comp_size))
float space_saved=$(((1.0 - (comp_size / orig_size)) * 100.0))

float w=$wzip_t
float p=$pzip_t
((p <= 0)) && p=0.001
((w <= 0)) && w=0.001

float speedup=$((w / p))
float efficiency=$(((speedup / CORES) * 100))
float p_throughput=$((PAYLOAD_MB / p))
float w_throughput=$((PAYLOAD_MB / w))

print "------------------------------------------------"
printf "CPU Cores Detected:  %d\n" $CORES
printf "Sequential wzip:     %.3fs (%.1f MB/s)\n" $w $w_throughput
printf "Parallel pzip:       %.3fs (%.1f MB/s)\n" $p $p_throughput
printf "Speedup Factor:      %.2fx\n" $speedup
printf "Parallel Efficiency: %.1f%%\n" $efficiency
printf "Compression Ratio:   %.2f:1 (%.1f%% space saved)\n" $comp_ratio $space_saved
print "------------------------------------------------"

if ((CORES == 1)); then
    if ((speedup >= 0.85)); then
        print "Grade: A (Optimal single-thread performance)"
    else
        print "Grade: C (High thread/synchronization overhead on 1 core)"
    fi
else
    if ((efficiency >= 65)); then
        print "Grade: A+ (Outstanding scaling - near-linear speedup)"
    elif ((efficiency >= 45)); then
        print "Grade: A  (Good scaling - effective thread utilization)"
    elif ((speedup >= 1.1)); then
        print "Grade: B  (Faster than sequential, but high thread contention/overhead)"
    else
        print "Grade: C  (Slower or equal to sequential - thread synchronization bottleneck)"
    fi
fi

rm -f bench.in bench.wzip bench.pzip
