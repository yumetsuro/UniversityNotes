#pragma once
#include "record.hpp"
#include <cstddef>
#include <cstring>

/**
 * @brief Merges two sorted subarrays of array[].
 * @param array The array containing the subarrays to merge.
 * @param left The starting index of the first subarray.
 * @param mid The ending index of the first subarray and the starting index of the second subarray.
 * @param right The ending index of the second subarray.
 * @param record_size The size of each record in the array.
 * @param tmp Temporary array to hold merged results.
 */
inline void merge(Record* array, size_t left, size_t mid, size_t right, size_t record_size, Record* tmp) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = 0;

    while (i <= mid && j <= right) {
        Record* rec_i = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + i * record_size);
        Record* rec_j = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + j * record_size);
        if (rec_i->key <= rec_j->key) {
            memcpy(reinterpret_cast<char*>(tmp) + k * record_size, 
                   reinterpret_cast<char*>(array) + i * record_size, 
                   record_size);
            i++;
        } else {
            memcpy(reinterpret_cast<char*>(tmp) + k * record_size, 
                   reinterpret_cast<char*>(array) + j * record_size, 
                   record_size);
            j++;
        }
        k++;
    }
    while (i <= mid) {
        memcpy(reinterpret_cast<char*>(tmp) + k * record_size, 
               reinterpret_cast<char*>(array) + i * record_size, 
               record_size);
        i++; k++;
    }
    while (j <= right) {
        memcpy(reinterpret_cast<char*>(tmp) + k * record_size, 
               reinterpret_cast<char*>(array) + j * record_size, 
               record_size);
        j++; k++;
    }
    memcpy(reinterpret_cast<char*>(array) + left * record_size, 
           reinterpret_cast<char*>(tmp), 
           k * record_size);
}

/**
 * @brief Recursively sorts the array using mergesort algorithm.
 * @param array 
 * @param left 
 * @param right 
 * @param record_size 
 * @param tmp 
 */
void mergesort(Record* array, size_t left, size_t right, size_t record_size, Record* tmp) {
    if (left >= right) return;
    size_t mid = (left + right) / 2;
    mergesort(array, left, mid, record_size, tmp);
    mergesort(array, mid + 1, right, record_size, tmp);
    merge(array, left, mid, right, record_size, tmp);
}
