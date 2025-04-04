#!/bin/bash
# filepath: /home/vincent/UniversityNotes/Assignments/ParallelDistributedSystems/Assignment2/benchmark.sh

# Output file
OUTPUT_FILE="benchmark_results.csv"

# Create CSV header
echo "Range,Algorithm,Threads,BatchSize,Time" > $OUTPUT_FILE

# Compile the program
g++ -o collatz_parallel src/collatz_parallel.cpp -pthread -std=c++17 -O3

# Define the ranges to test
declare -a ranges=(
    "1 1000"
    "50000000 100000000"
    "1000000000 1100000000"
)

# Function to run a single benchmark
run_benchmark() {
    range=$1       # Range as "start end"
    algorithm=$2   # "Static" or "Dynamic"
    threads=$3
    batch_size=$4
    
    # Parse range
    read start end <<< "$range"
    
    # Prepare dynamic flag
    dynamic_flag=""
    if [ "$algorithm" = "Dynamic" ]; then
        dynamic_flag="-d"
    fi
    
    echo "Running $algorithm with $threads threads and batch size $batch_size on range [$start, $end]..."
    
    # Run the program and capture output
    output=$(./collatz_parallel $start $end -n $threads -c $batch_size $dynamic_flag)
    
    # Extract time using grep and awk
    time=$(echo "$output" | grep "Time:" | awk -F "Time: " '{print $2}' | awk '{print $1}')
    
    # Create range string for CSV
    range_str="[$start,$end]"
    
    # Append to CSV
    echo "$range_str,$algorithm,$threads,$batch_size,$time" >> $OUTPUT_FILE
}

# Run all benchmarks
for range in "${ranges[@]}"; do
    for algorithm in "Static" "Dynamic"; do
        for threads in 8 32; do
            for batch_size in 10 64 256 8192; do
                run_benchmark "$range" $algorithm $threads $batch_size
            done
        done
    done
done

echo "Benchmarks completed. Results saved to $OUTPUT_FILE"