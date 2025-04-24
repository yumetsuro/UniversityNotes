#!/bin/bash

# Usage: ./generate.sh [output_directory] [num_small] [num_medium] [num_big]
# Example: ./generate.sh ../data 5 2 1

# Default values
OUTPUT_DIR=${1:-.}  # Default to current directory if not specified
NUM_SMALL=${2:-1}
NUM_MEDIUM=${3:-1}
NUM_BIG=${4:-1}

# Check if output directory exists, if not, create it
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "Creating directory: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create directory $OUTPUT_DIR"
        exit 1
    fi
fi

echo "Generating files in $OUTPUT_DIR:"

echo "Generating $NUM_SMALL small files (16 KB each)..."
for i in $(seq 1 $NUM_SMALL); do
    dd if=/dev/urandom of="$OUTPUT_DIR/smallfile_$i.dat" bs=16K count=1 status=none
done

echo "Generating $NUM_MEDIUM medium files (20 MB each)..."
for i in $(seq 1 $NUM_MEDIUM); do
    dd if=/dev/urandom of="$OUTPUT_DIR/mediumfile_$i.dat" bs=1M count=20 status=none
done

echo "Generating $NUM_BIG big files (100 MB each)..."
for i in $(seq 1 $NUM_BIG); do
    dd if=/dev/urandom of="$OUTPUT_DIR/bigfile_$i.dat" bs=2M count=50 status=none
done

echo "Done! Files generated in $OUTPUT_DIR"

