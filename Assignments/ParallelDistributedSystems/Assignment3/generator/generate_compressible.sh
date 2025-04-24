#!/bin/bash

# generate_compressible.sh
# This script generates fake data that has patterns and is more compressible 
# than random data from /dev/urandom.

# Usage: ./generate_compressible.sh [output_directory] [num_small] [num_medium] [num_big] [random]
# Example: ./generate_compressible.sh ../compression 5 2 1
# Example with random data: ./generate_compressible.sh ../compression 5 2 1 random

# Default values
OUTPUT_DIR=${1:-.}  # Default to current directory if not specified
NUM_SMALL=${2:-1}
NUM_MEDIUM=${3:-1}
NUM_BIG=${4:-1}
RANDOM_FLAG=${5:-""}  # Optional random flag
NORANDOM_FLAG=${6:-""}  # Optional norandom flag

# Check if output directory exists, if not, create it
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "Creating directory: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create directory $OUTPUT_DIR"
        exit 1
    fi
fi

if [ "$RANDOM_FLAG" = "random" ]; then
    echo "Generating random files (using /dev/urandom) in $OUTPUT_DIR:"
    # call the file adding _random suffix
    echo "Generating $NUM_SMALL small random files (16 KB each)..."
    for i in $(seq 1 $NUM_SMALL); do
        dd if=/dev/urandom of="$OUTPUT_DIR/smallfile_random_$i.txt" bs=16K count=1 status=none
        echo "Created $OUTPUT_DIR/smallfile_$i.txt: $(stat -c%s "$OUTPUT_DIR/smallfile_$i.txt") bytes"
    done
    
    echo "Generating $NUM_MEDIUM medium random files (20 MB each)..."
    for i in $(seq 1 $NUM_MEDIUM); do
        dd if=/dev/urandom of="$OUTPUT_DIR/mediumfile_random_$i.csv" bs=1M count=20 status=none
        echo "Created $OUTPUT_DIR/mediumfile_$i.csv: $(stat -c%s "$OUTPUT_DIR/mediumfile_$i.csv") bytes"
    done
    
    echo "Generating $NUM_BIG big random files (100 MB each)..."
    for i in $(seq 1 $NUM_BIG); do
        dd if=/dev/urandom of="$OUTPUT_DIR/bigfile_random_$i.dat" bs=2M count=50 status=none
        echo "Created $OUTPUT_DIR/bigfile_$i.dat: $(stat -c%s "$OUTPUT_DIR/bigfile_$i.dat") bytes"
    done
    echo "Done! Random files generated in $OUTPUT_DIR"
    else
    echo "No random files generated. Use 'random' flag to generate random files."
fi

echo "Generating compressible files in $OUTPUT_DIR:"

# Function to generate compressible text data
generate_text_data() {
    local file_path=$1
    local size_kb=$2
    
    # Create a set of words and phrases that will be repeated
    words=(
        "the quick brown fox jumps over the lazy dog"
        "hello world this is a test of compressible data"
        "lorem ipsum dolor sit amet consectetur adipiscing elit"
        "parallel distributed systems assignment compression test"
        "data compression algorithms use patterns like this to achieve better ratios"
        "this sentence will repeat many times in the file creating patterns"
        "OpenMP is a parallel programming model for shared memory multiprocessing"
    )
    
    # Number of words in our array
    word_count=${#words[@]}
    
    # Create file with repeated patterns
    rm -f "$file_path" # Remove if exists
    
    # Calculate roughly how many lines we need based on requested size
    # Average line length ~60 chars
    local target_size_bytes=$((size_kb * 1024))
    local lines=$((target_size_bytes / 60))
    
    for ((i=0; i<lines; i++)); do
        # Pick random words from our array and append to file
        index=$((i % word_count))
        echo "${words[$index]}" >> "$file_path"
        
        # Every few lines, add some structured data like JSON or CSV
        if (( i % 20 == 0 )); then
            echo "{\"id\": $i, \"name\": \"item$i\", \"value\": $(($i * 10))}" >> "$file_path"
        fi
        
        # Add some repeated patterns
        if (( i % 50 == 0 )); then
            for ((j=0; j<5; j++)); do
                echo "REPEATED BLOCK $i - LINE $j" >> "$file_path"
            done
        fi
    done
    
    # Ensure we hit the target size (approximately)
    current_size=$(stat -c%s "$file_path")
    if (( current_size < target_size_bytes )); then
        # Pad with a repeating pattern if needed
        padding=$((target_size_bytes - current_size))
        dd if=/dev/zero bs=$padding count=1 2>/dev/null | tr '\0' 'X' >> "$file_path"
    fi
    
    echo "Created $file_path: $(stat -c%s "$file_path") bytes"
}

# Function to generate semi-structured data (like CSV)
generate_csv_data() {
    local file_path=$1
    local num_rows=$2
    
    # CSV header
    echo "id,name,category,value,description,date,status" > "$file_path"
    
    # Categories and statuses that will repeat
    categories=("Electronics" "Clothing" "Food" "Books" "Tools")
    statuses=("Active" "Pending" "Completed" "Cancelled" "Processing")
    
    for ((i=1; i<=num_rows; i++)); do
        cat_idx=$((i % ${#categories[@]}))
        status_idx=$((i % ${#statuses[@]}))
        
        # Generate a row with some repetitive patterns
        echo "$i,Item$i,${categories[$cat_idx]},$((i * 100)),This is a sample description for item $i,2025-04-$((i % 30 + 1)),${statuses[$status_idx]}" >> "$file_path"
    done
    
    echo "Created CSV $file_path: $(stat -c%s "$file_path") bytes"
}

# Function to generate binary data with patterns
generate_binary_data() {
    local file_path=$1
    local size_mb=$2
    
    # Create a 1KB pattern block that will repeat
    pattern_file="/tmp/pattern_$$"
    
    # Generate pattern with some structure
    dd if=/dev/urandom bs=256 count=1 2>/dev/null > "$pattern_file"
    # Duplicate this block to create repetition
    cat "$pattern_file" "$pattern_file" "$pattern_file" "$pattern_file" > "${pattern_file}_1k"
    
    # Repeat the 1KB block to create the file
    blocks=$((size_mb * 1024)) # Number of 1KB blocks for the target size
    for ((i=0; i<blocks; i++)); do
        cat "${pattern_file}_1k" >> "$file_path"
        
        # Every so often, insert a solid block of zeros to create patterns
        if (( i % 100 == 0 )); then
            dd if=/dev/zero bs=1K count=1 2>/dev/null >> "$file_path"
        fi
    done
    
    # Clean up temp files
    rm -f "$pattern_file" "${pattern_file}_1k"
    
    echo "Created binary $file_path: $(stat -c%s "$file_path") bytes"
}

# if NORANDOM_FLAG is on then generate files with patterns otherwise do not generate nothing
if [ "$NORANDOM_FLAG" = "true" ]; then
    # generate 16 kb files (these are small) use text data from words
    echo "Generating $NUM_SMALL small text files with patterns (16 KB each)..."
    for i in $(seq 1 $NUM_SMALL); do
        generate_text_data "$OUTPUT_DIR/smallfile_$i.txt" 16
    done

    echo "Generating $NUM_MEDIUM medium CSV files (20 MB each)..."
    for i in $(seq 1 $NUM_MEDIUM); do
        generate_csv_data "$OUTPUT_DIR/mediumfile_$i.csv" 20000
    done

    echo "Generating $NUM_BIG big binary files with patterns (100 MB each)..."
    for i in $(seq 1 $NUM_BIG); do
        generate_binary_data "$OUTPUT_DIR/bigfile_$i.dat" 100
    done
else
    echo "No compressible files generated. Use 'norandom' flag to generate files with patterns."
    exit 1
fi


echo "Done! Compressible files generated in $OUTPUT_DIR"