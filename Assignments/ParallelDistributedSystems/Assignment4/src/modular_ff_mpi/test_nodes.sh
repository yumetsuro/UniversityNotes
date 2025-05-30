#!/bin/bash
#SBATCH --partition=normal
#SBATCH -o slurm_output_8.log
#SBATCH -e slurm_error_8.log
#SBATCH --nodes=8
#SBATCH --ntasks=8

echo "Compiling..."
#make clean > /dev/null 2>&1
#g++ -std=c++17 -O3 -Wall -Ifastflow -pthread -DMAX_PAYLOAD_SIZE=1024 -o mergesort_ff mergesort_ff.cpp -pthread
#mpicxx -std=c++17 -O3 -Wall -Ifastflow -pthread -DMAX_PAYLOAD_SIZE=512 -o mergesort_ff_mpi mergesort_ff_mpi.cpp -pthread
#srun --nodes=1 --ntasks=1 make all


# Create CSV file
CSV_FILE="performance_results/test_results_$(date +%Y%m%d_%H%M%S).csv"
ENTIRE_OUTPUT_FILE="performance_results/entire_output_$(date +%Y%m%d_%H%M%S).txt"
#mkdir -p performance_results
echo "Implementation,Array_Size,Payload_Size,Threads,MPI_Nodes,Time_ms" > "$CSV_FILE"

# Test configurations
SIZES=(10000000 50000000 100000000)  # 10M, 50M, 100M
PAYLOADS=(0 32 64)
THREADS=(8 16 32)
MPI_NODES=(2 4 8)

echo "Starting tests..."

# Function to extract time from output
extract_time() {
    echo "$1" | grep -E "(Total time:|Total processing time)" | grep -o '[0-9]\+' | head -1
}

# Test MPI version
echo "Testing MPI..."
echo "Testing MPI ----------------------------------------------" >> "$ENTIRE_OUTPUT_FILE"
for nodes in "${MPI_NODES[@]}"; do
    # Use 32 threads per node for consistency
    for local_threads in "${THREADS[@]}"; do
        for size in "${SIZES[@]}"; do
            for payload in "${PAYLOADS[@]}"; do
                echo "MPI: nodes=$nodes, size=$size, payload=$payload"
                output=$(timeout 300 srun --time=00:10:00 --ntasks-per-node=1 --mpi=pmix -N $nodes -n $nodes ./mergesort_ff_mpi --size $size --record $payload --threads $local_threads)
                echo "$output" >> "$ENTIRE_OUTPUT_FILE"
                time_ms=$(extract_time "$output")
                if [ -n "$time_ms" ]; then
                    echo "MPI,$size,$payload,$local_threads,$nodes,$time_ms" >> "$CSV_FILE"
                else
                    echo "MPI,$size,$payload,$local_threads,$nodes,FAILED" >> "$CSV_FILE"
                fi
                # sleep so that no two processes run at the same time for 20 minls
            done
        done
    done
done

echo "Tests completed! Results saved to: $CSV_FILE"
echo "CSV file contents:"
cat "$CSV_FILE"
