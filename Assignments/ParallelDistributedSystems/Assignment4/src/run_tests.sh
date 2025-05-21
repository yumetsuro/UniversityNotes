#!/bin/zsh

# Compile the program
echo "Compiling mergesort_ff..."
make

# Test configurations
echo "Running sequential version with different sizes..."
./mergesort_ff -s 1M -r 8 -q
./mergesort_ff -s 10M -r 8 -q

echo "Running parallel versions with different thread counts..."
for t in 1 2 4 8 16; do
    echo "Running with $t threads..."
    ./mergesort_ff -s 10M -r 8 -t $t
done

echo "Running with different record sizes..."
for r in 8 64 256; do
    echo "Record size $r bytes..."
    ./mergesort_ff -s 10M -r $r -t 8
done

echo "Running with larger array sizes..."
./mergesort_ff -s 50M -r 8 -t 8
./mergesort_ff -s 100M -r 8 -t 16

echo "All tests completed."
