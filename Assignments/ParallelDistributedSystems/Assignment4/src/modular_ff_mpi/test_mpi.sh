#!/bin/bash

# Simple Performance Test Script for MergeSort Implementations
# Tests: Sequential, FastFlow (4,8,16 threads), MPI (2,4 nodes)
# Array sizes: 1M, 10M, 50M
# Payload sizes: 0, 10, 50 bytes

#cd /home/vincent/UniversityNotes/Assignments/ParallelDistributedSystems/Assignment4/src

# Compile programs
echo "Compiling..."
#make clean > /dev/null 2>&1
#g++ -std=c++17 -O3 -Wall -Ifastflow -pthread -DMAX_PAYLOAD_SIZE=1024 -o mergesort_ff mergesort_ff.cpp -pthread
#mpicxx -std=c++17 -O3 -Wall -Ifastflow -pthread -DMAX_PAYLOAD_SIZE=512 -o mergesort_ff_mpi mergesort_ff_mpi.cpp -pthread
srun --nodes=1 --ntask=1 make all

# Create CSV file
CSV_FILE="performance_results/test_results_$(date +%Y%m%d_%H%M%S).csv"
ENTIRE_OUTPUT_FILE="performance_results/entire_output_$(date +%Y%m%d_%H%M%S).txt"
mkdir -p performance_results
echo "Implementation,Array_Size,Payload_Size,Threads,MPI_Nodes,Time_ms" > "$CSV_FILE"

# Test configurations - Start smaller to debug memory issues
SIZES=(5000000 10000000 100000000)  # 1M, 5M, 10M (further reduced to avoid memory issues)
PAYLOADS=(0 10 30)  # Reduced max payload from 50 to 30 bytes
THREADS=(8 16 32)  # Removed 32 threads for now
MPI_NODES=(2 4 8)   # Reduced to 2 and 4 nodes only

echo "Starting tests..."

# Function to extract time from output
extract_time() {
    echo "$1" | grep -E "(Total time:|Total processing time:|processing time)" | grep -o '[0-9]\+' | head -1
}

# Test Sequential version
echo "Testing Sequential..."
for size in "${SIZES[@]}"; do
    for payload in "${PAYLOADS[@]}"; do
        echo "Sequential: size=$size, payload=$payload"
        output=$(timeout 300 srun ./mergesort_ff_mpi -s $size -r $payload -q 2>&1)
        echo "$output" >> "$ENTIRE_OUTPUT_FILE"
        time_ms=$(extract_time "$output")
        if [ -n "$time_ms" ] && [ "$time_ms" -gt 0 ] 2>/dev/null; then
            echo "Sequential,$size,$payload,1,1,$time_ms" >> "$CSV_FILE"
        else
            echo "Sequential,$size,$payload,1,1,FAILED" >> "$CSV_FILE"
            echo "Sequential FAILED: $output" >> "$ENTIRE_OUTPUT_FILE"
        fi
    done
done

# Test FastFlow version
echo "Testing FastFlow..."
echo "Testing FastFlow ----------------------------------------------" >> "$ENTIRE_OUTPUT_FILE"
for threads in "${THREADS[@]}"; do
    for size in "${SIZES[@]}"; do
        for payload in "${PAYLOADS[@]}"; do
            echo "FastFlow: threads=$threads, size=$size, payload=$payload"
            output=$(timeout 300 srun ./mergesort_ff_mpi -s $size -r $payload -t $threads -q 2>&1)
            # extract all the output and send it to a new file
            echo "$output" >> "$ENTIRE_OUTPUT_FILE"
            time_ms=$(extract_time "$output")
            if [ -n "$time_ms" ] && [ "$time_ms" -gt 0 ] 2>/dev/null; then
                echo "FastFlow,$size,$payload,$threads,1,$time_ms" >> "$CSV_FILE"
            else
                echo "FastFlow,$size,$payload,$threads,1,FAILED" >> "$CSV_FILE"
                echo "FastFlow FAILED: $output" >> "$ENTIRE_OUTPUT_FILE"
            fi
        done
    done
done

# Test MPI version
echo "Testing MPI..."
echo "Testing MPI ----------------------------------------------" >> "$ENTIRE_OUTPUT_FILE"
for nodes in "${MPI_NODES[@]}"; do
    # Use 16 threads per node for consistency (reduced from 32)
    local_threads=16
    for size in "${SIZES[@]}"; do
        for payload in "${PAYLOADS[@]}"; do
            echo "MPI: nodes=$nodes, size=$size, payload=$payload"
            output=$(timeout 300 srun --cpu-bind=none --mpi=pmix -N $nodes -n $nodes ./mergesort_ff_mpi -s $size -r $payload -t $local_threads -q 2>&1)
            # divide the testing of different version
            echo "$output" >> "$ENTIRE_OUTPUT_FILE"
            time_ms=$(extract_time "$output")
            if [ -n "$time_ms" ] && [ "$time_ms" -gt 0 ] 2>/dev/null; then
                echo "MPI,$size,$payload,$local_threads,$nodes,$time_ms" >> "$CSV_FILE"
            else
                echo "MPI,$size,$payload,$local_threads,$nodes,FAILED" >> "$CSV_FILE"
                echo "MPI FAILED: $output" >> "$ENTIRE_OUTPUT_FILE"
            fi
        done
    done
done

echo "Tests completed! Results saved to: $CSV_FILE"
echo "CSV file contents:"
cat "$CSV_FILE"
