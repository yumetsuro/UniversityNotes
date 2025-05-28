#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>

// Global variable for runtime payload size configuration
size_t g_payload_size = 8;  // Default payload size

// Record structure with maximum payload size
#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE 256
#endif

#ifndef DEBUG
#define DEBUG 1 
#endif

#ifndef SIMULATE_TRANSFER
#define SIMULATE_TRANSFER 1 
#endif

/**
 * @brief Record structure for sorting tasks.
 */
struct Record {
    unsigned long key;  // sorting value
    char* rpayload;     // pointer to payload
    
    // Get the record size (just key + pointer)
    static size_t get_record_size() {
        return sizeof(unsigned long) + sizeof(char*);
    }
};

// Function to create record array with inline payload allocation
inline std::pair<Record*, char*> create_record_array(size_t n) {
    Record* array = new Record[n];
    
    // Allocate one contiguous block for all payloads
    char* payload_block = new char[n * g_payload_size];
    
    // Initialize records with random keys and payload pointers
    for (size_t i = 0; i < n; i++) {
        array[i].key = rand();
        array[i].rpayload = payload_block + (i * g_payload_size); 
    }
    
    return std::make_pair(array, payload_block);
}

// Function to verify the sorted array
inline bool verify_sorted(Record* array, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (array[i-1].key > array[i].key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << array[i-1].key << " > " << array[i].key << std::endl;
            return false;
        }
    }
    return true;
}
