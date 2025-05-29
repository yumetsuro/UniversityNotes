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
make all

# Create CSV file
CSV_FILE="performance_results/test_results_$(date +%Y%m%d_%H%M%S).csv"
mkdir -p performance_results
echo "Implementation,Array_Size,Payload_Size,Threads,MPI_Nodes,Time_ms" > "$CSV_FILE"

# Test configurations
SIZES=(10000000 50000000 100000000)  # 1M, 10M, 50M, 100M
PAYLOADS=(0 10 50)
THREADS=(4 8 16 32)
MPI_NODES=(1 2 3 4 5 6 7 8)

echo "Starting tests..."

# Function to extract time from output
extract_time() {
    echo "$1" | grep -E "(Total time:|Total processing time)" | grep -o '[0-9]\+' | head -1
}

# Test Sequential version
echo "Testing Sequential..."
for size in "${SIZES[@]}"; do
    for payload in "${PAYLOADS[@]}"; do
        echo "Sequential: size=$size, payload=$payload"
        output=$(timeout 300 srun ./mergesort_ff_mpi -s $size -r $payload -q)
        time_ms=$(extract_time "$output")
        if [ -n "$time_ms" ]; then
            echo "Sequential,$size,$payload,1,1,$time_ms" >> "$CSV_FILE"
        else
            echo "Sequential,$size,$payload,1,1,FAILED" >> "$CSV_FILE"
        fi
    done
done

# Test FastFlow version
echo "Testing FastFlow..."
for threads in "${THREADS[@]}"; do
    for size in "${SIZES[@]}"; do
        for payload in "${PAYLOADS[@]}"; do
            echo "FastFlow: threads=$threads, size=$size, payload=$payload"
            output=$(timeout 300 srun ./mergesort_ff_mpi -s $size -r $payload -t $threads 2>&1)
            time_ms=$(extract_time "$output")
            if [ -n "$time_ms" ]; then
                echo "FastFlow,$size,$payload,$threads,1,$time_ms" >> "$CSV_FILE"
            else
                echo "FastFlow,$size,$payload,$threads,1,FAILED" >> "$CSV_FILE"
            fi
        done
    done
done

# Test MPI version
echo "Testing MPI..."
for nodes in "${MPI_NODES[@]}"; do
    # Use 4 threads per node for consistency
    local_threads=4
    for size in "${SIZES[@]}"; do
        for payload in "${PAYLOADS[@]}"; do
            echo "MPI: nodes=$nodes, size=$size, payload=$payload"
            output=$(timeout 300 srun --cpu-bind=none --mpi=pmix -N $nodes -n $local_threads ./mergesort_ff_mpi --size $size --record $payload --threads $local_threads 2>&1)
            time_ms=$(extract_time "$output")
            if [ -n "$time_ms" ]; then
                echo "MPI,$size,$payload,$local_threads,$nodes,$time_ms" >> "$CSV_FILE"
            else
                echo "MPI,$size,$payload,$local_threads,$nodes,FAILED" >> "$CSV_FILE"
            fi
        done
    done
done

echo "Tests completed! Results saved to: $CSV_FILE"
echo "CSV file contents:"
cat "$CSV_FILE"
