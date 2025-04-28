#!/bin/bash

# Usage: ./script.sh {n} {path} {max_threads}
# {n}: Number of times to run the command for each thread count
# {path}: Path to a file or directory to compress
# {max_threads}: Maximum number of threads to test (optional, defaults to number of CPU cores)

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 {n} {path} [max_threads]"
  echo "  {n}: Number of times to run the command for each thread count"
  echo "  {path}: Path to a file or directory to compress"
  echo "  [max_threads]: Maximum number of threads to test (optional, defaults to number of CPU cores)"
  exit 1
fi

n=$1
path=$2
# Use the number of CPU cores as default max_threads if not provided
max_threads=${3:-$(nproc)}

if [ ! -e "$path" ]; then
  echo "Error: Path '$path' does not exist"
  exit 1
fi

# Create a CSV file to store results
result_file="thread_benchmark_results.csv"
echo "Threads,MeanTime(s)" > "$result_file"

# Loop through different thread counts
for threads in $(seq 2 $max_threads); do
  echo "====================================="
  echo "Testing with $threads thread(s)"
  echo "====================================="
  
  total_time=0
  
  for ((i=1; i<=n; i++)); do
    echo "Running trial $i of $n with $threads thread(s)..."
    start_time=$(date +%s.%N)
    
    if [ -d "$path" ]; then
      # If path is a directory, compress all files in it
      ../assignment3/minizpar -q 1 -r 1 -D 0 -t $threads "$path"
    else
      # If path is a file, compress just that file
      ../assignment3/minizpar -q 1 -r 1 -D 0 -t $threads "$path"
    fi
    
    end_time=$(date +%s.%N)
    # Use awk instead of bc for floating point calculations
    elapsed_time=$(awk "BEGIN {printf \"%.5f\", $end_time - $start_time}")
    total_time=$(awk "BEGIN {printf \"%.5f\", $total_time + $elapsed_time}")
    echo "Trial $i completed in $elapsed_time seconds"
  done

  # Calculate mean time in seconds with 5 significant digits using awk
  mean_time=$(awk "BEGIN {printf \"%.5f\", $total_time / $n}")
  echo "Mean time over $n trials with $threads thread(s): $mean_time seconds"
  
  # Add result to CSV
  echo "$threads,$mean_time" >> "$result_file"
done

echo "====================================="
echo "Benchmark completed. Results saved to $result_file"
echo "====================================="