#!/bin/bash

# Run headless benchmark multiple times and calculate statistics
# Useful for getting stable performance measurements

RUNS=${1:-5}
OUTPUT_FILE="benchmark_results.txt"

echo "========================================="
echo "   RENDERER BENCHMARK STATISTICS"
echo "========================================="
echo "Running benchmark $RUNS times..."
echo ""

# Clear previous results
> $OUTPUT_FILE

# Run benchmarks
for i in $(seq 1 $RUNS); do
    echo "Run $i/$RUNS..."
    ./renderer_benchmark_headless 2>/dev/null | tee -a $OUTPUT_FILE > /dev/null
    sleep 1  # Brief pause between runs
done

echo ""
echo "========================================="
echo "           SUMMARY STATISTICS"
echo "========================================="
echo ""

# Extract and analyze key metrics
echo "Matrix Multiplication (1M ops):"
grep "Iterations: 1000000" $OUTPUT_FILE -A 3 | grep "Operations/sec" | awk -F': ' '{print $2}' | \
    awk '{sum+=$1; sumsq+=$1*$1; count++} END {
        if (count > 0) {
            avg=sum/count; 
            std=sqrt(sumsq/count - avg*avg);
            printf "  Average: %.0f ops/sec\n", avg;
            printf "  Std Dev: %.0f (%.1f%%)\n", std, (std/avg)*100;
            printf "  Samples: %d\n", count
        } else {
            printf "  No data found\n"
        }
    }'

echo ""
echo "Transform Operations (1M points):"
grep "Point transforms: 1000000" $OUTPUT_FILE -A 3 | grep "Transforms/sec" | awk -F': ' '{print $2}' | \
    awk '{sum+=$1; sumsq+=$1*$1; count++} END {
        if (count > 0) {
            avg=sum/count; 
            std=sqrt(sumsq/count - avg*avg);
            printf "  Average: %.0f ops/sec\n", avg;
            printf "  Std Dev: %.0f (%.1f%%)\n", std, (std/avg)*100;
            printf "  Samples: %d\n", count
        } else {
            printf "  No data found\n"
        }
    }'

echo ""
echo "Frustum Culling (100K objects):"
grep "Objects tested: 100000" $OUTPUT_FILE -A 10 | grep "Time:" | head -1 | awk '{print $2}' | \
    awk '{sum+=$1; sumsq+=$1*$1; count++} END {
        if (count > 0) {
            avg=sum/count; 
            std=sqrt(sumsq/count - avg*avg);
            printf "  Average time: %.3f ms\n", avg;
            printf "  Std Dev: %.3f ms (%.1f%%)\n", std, (std/avg)*100;
            printf "  Throughput: %.0f objects/ms\n", 100000/avg
        } else {
            printf "  No data found\n"
        }
    }'

echo ""
echo "Draw Call Batching (50K objects):"
grep "Objects: 50000" $OUTPUT_FILE -A 5 | grep "Batching time:" | awk '{print $3}' | \
    awk '{sum+=$1; sumsq+=$1*$1; count++} END {
        if (count > 0) {
            avg=sum/count; 
            std=sqrt(sumsq/count - avg*avg);
            printf "  Average: %.3f ms\n", avg;
            printf "  Std Dev: %.3f ms (%.1f%%)\n", std, (std/avg)*100;
            printf "  Throughput: %.0f objects/ms\n", 50000/avg
        } else {
            printf "  No data found\n"
        }
    }'

echo ""
echo "Vector Normalization (10M vectors):"
grep "Vector count: 10000000" $OUTPUT_FILE -A 5 | grep "Normalization:" | awk '{print $2}' | \
    awk '{sum+=$1; sumsq+=$1*$1; count++} END {
        if (count > 0) {
            avg=sum/count; 
            std=sqrt(sumsq/count - avg*avg);
            printf "  Average: %.3f ms\n", avg;
            printf "  Std Dev: %.3f ms (%.1f%%)\n", std, (std/avg)*100;
            printf "  Throughput: %.2f Gops/s\n", 10.0/(avg/1000.0)
        } else {
            printf "  No data found\n"
        }
    }'

echo ""
echo "========================================="
echo "Results saved to: $OUTPUT_FILE"
echo ""
echo "Tips for stable measurements:"
echo "  1. Set CPU governor to performance:"
echo "     sudo cpupower frequency-set -g performance"
echo "  2. Pin to specific CPU core:"
echo "     taskset -c 0 ./run_benchmark_stats.sh"
echo "  3. Disable turbo boost for consistency:"
echo "     echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo"
echo "========================================="