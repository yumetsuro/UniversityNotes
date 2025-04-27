#!/bin/bash

# Usage: ./testseq.sh {n} {path}
# {n}: Number of times to run the command
# {path}: Path to a file or directory to compress

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 {n} {path}"
  echo "  {n}: Number of times to run the command"
  echo "  {path}: Path to a file or directory to compress"
  exit 1
fi

n=$1
path=$2

if [ ! -e "$path" ]; then
  echo "Error: Path '$path' does not exist"
  exit 1
fi

# Create a CSV file to store results
result_file="seq_benchmark_results.csv"
echo "Trial,Time(s)" > "$result_file"

echo "====================================="
echo "Running $n trials for sequential compression"
echo "====================================="

total_time=0

for ((i=1; i<=n; i++)); do
  echo "Running trial $i of $n..."
  start_time=$(date +%s.%N)
  
  if [ -d "$path" ]; then
    # If path is a directory, compress all files in it
    ../assignment3/minizseq -q 1 -r 1 -C 0 "$path"
  else
    # If path is a file, compress just that file
    ../assignment3/minizseq -q 1 -r 1 -C 0 "$path"
  fi
  
  end_time=$(date +%s.%N)
  elapsed_time=$(echo "$end_time - $start_time" | bc)
  total_time=$(echo "$total_time + $elapsed_time" | bc)
  echo "Trial $i completed in $elapsed_time seconds"
  
  # Add individual trial result to CSV
  echo "$i,$elapsed_time" >> "$result_file"
done

# Calculate mean time in seconds with 5 significant digits
mean_time=$(echo "scale=5; $total_time / $n" | bc)
echo "Mean time over $n trials: $mean_time seconds"

# Add mean time to CSV
echo "Mean,$mean_time" >> "$result_file"

echo "====================================="
echo "Benchmark completed. Results saved to $result_file"
echo "====================================="