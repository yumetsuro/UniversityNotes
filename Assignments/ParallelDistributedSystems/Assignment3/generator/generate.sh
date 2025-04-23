#!/bin/bash

# Usage: ./generate_files.sh <num_small> <num_medium> <num_big>

NUM_SMALL=${1:-1}
NUM_MEDIUM=${2:-1}
NUM_BIG=${3:-1}

echo "Generating $NUM_SMALL small files (16 KB each)..."
for i in $(seq 1 $NUM_SMALL); do
    dd if=/dev/urandom of=smallfile_$i.dat bs=16K count=1 status=none
done

echo "Generating $NUM_MEDIUM medium files (20 MB each)..."
for i in $(seq 1 $NUM_MEDIUM); do
    dd if=/dev/urandom of=mediumfile_$i.dat bs=1M count=20 status=none
done

echo "Generating $NUM_BIG big files (100 MB each)..."
for i in $(seq 1 $NUM_BIG); do
    dd if=/dev/urandom of=bigfile_$i.dat bs=2M count=50 status=none
done

echo "Done!"

