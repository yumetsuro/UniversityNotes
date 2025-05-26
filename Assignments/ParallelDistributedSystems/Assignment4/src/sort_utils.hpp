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
// Sequential mergesort implementation
void merge(Record* array, size_t left, size_t mid, size_t right, Record* tmp) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = 0;
    
    while (i <= mid && j <= right) {
        if (array[i].key <= array[j].key) {
            tmp[k] = array[i];
            i++;
        } else {
            tmp[k] = array[j];
            j++;
        }
        k++;
    }
    
    while (i <= mid) {
        tmp[k] = array[i];
        i++; k++;
    }
    
    while (j <= right) {
        tmp[k] = array[j];
        j++; k++;
    }
    
    // Copy back to original array
    for (size_t idx = 0; idx < k; idx++) {
        array[left + idx] = tmp[idx];
    }
}

/**
 * @brief Recursively sorts the array using mergesort algorithm.
 * @param array 
 * @param left 
 * @param right 
 * @param record_size 
 * @param tmp 
 */
void mergesort(Record* array, size_t left, size_t right, Record* tmp) {
    if (left >= right) return;
    
    size_t mid = (left + right) / 2;
    
    mergesort(array, left, mid, tmp);
    mergesort(array, mid + 1, right, tmp);
    
    merge(array, left, mid, right, tmp);
}