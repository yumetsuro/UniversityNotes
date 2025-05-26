#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Global variable for runtime payload size configuration
size_t g_payload_size = 8;  // Default payload size

// Record structure with maximum payload size
#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE 256
#endif

struct Record {
    unsigned long key;  // sorting value
    char rpayload[MAX_PAYLOAD_SIZE];   // maximum size payload
    
    // Get the actual record size based on current payload setting
    static size_t get_record_size() {
        return sizeof(unsigned long) + g_payload_size;
    }
};

// Function to create a record array with runtime payload size
Record* create_record_array(size_t n) {
    Record* array = new Record[n];
    
    // Initialize records with random keys
    for (size_t i = 0; i < n; i++) {
        array[i].key = rand();
        
        // Initialize payload with some pattern (optional)
        for (size_t j = 0; j < g_payload_size; j++) {
            array[i].rpayload[j] = static_cast<char>(j % 256);
        }
    }
    
    return array;
}

// Function to verify the sorted array
bool verify_sorted(Record* array, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (array[i-1].key > array[i].key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << array[i-1].key << " > " << array[i].key << std::endl;
            return false;
        }
    }
    return true;
}