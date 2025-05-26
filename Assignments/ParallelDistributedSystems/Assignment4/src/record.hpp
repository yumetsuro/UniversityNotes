#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Record structure as per assignment
struct Record {
    unsigned long key;  // sorting value
    char rpayload[1];   // flexible array member, actual size will be set at runtime
};

// Function to dynamically create a record array with specific payload size
inline Record* create_record_array(size_t n, size_t payload_size) {
    size_t record_size = sizeof(unsigned long) + payload_size;
    char* memory = new char[n * record_size];
    for (size_t i = 0; i < n; i++) {
        Record* rec = reinterpret_cast<Record*>(memory + i * record_size);
        rec->key = rand();
        for (size_t j = 0; j < payload_size; j++) {
            rec->rpayload[j] = static_cast<char>(j % 256);
        }
    }
    return reinterpret_cast<Record*>(memory);
}

// Function to verify the sorted array
inline bool verify_sorted(Record* array, size_t n, size_t record_size) {
    for (size_t i = 1; i < n; i++) {
        Record* prev = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + (i-1) * record_size);
        Record* curr = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + i * record_size);
        if (prev->key > curr->key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << prev->key << " > " << curr->key << std::endl;
            return false;
        }
    }
    return true;
}
