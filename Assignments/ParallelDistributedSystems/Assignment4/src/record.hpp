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
    char* rpayload;     // pointer to payload
    
    // Constructor
    Record() : key(0), rpayload(nullptr) {}
    
    // Copy constructor - only copies key and pointer, not payload data
    Record(const Record& other) : key(other.key), rpayload(other.rpayload) {}
    
    // Assignment operator - only copies key and pointer
    Record& operator=(const Record& other) {
        if (this != &other) {
            key = other.key;
            rpayload = other.rpayload;
        }
        return *this;
    }
    
    // Get the record size (just key + pointer)
    static size_t get_record_size() {
        return sizeof(unsigned long) + sizeof(char*);
    }
};

// Modified function to create record array with inline payload allocation
std::pair<Record*, char*> create_record_array_with_payload(size_t n) {
    Record* array = new Record[n];
    
    // Allocate one contiguous block for all payloads
    char* payload_block = new char[n * g_payload_size];
    
    // Initialize records with random keys and payload pointers
    for (size_t i = 0; i < n; i++) {
        array[i].key = rand();
        array[i].rpayload = payload_block + (i * g_payload_size);
        
        // Initialize payload with some pattern (optional)
        for (size_t j = 0; j < g_payload_size; j++) {
            array[i].rpayload[j] = static_cast<char>(j % 256);
        }
    }
    
    return std::make_pair(array, payload_block);
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