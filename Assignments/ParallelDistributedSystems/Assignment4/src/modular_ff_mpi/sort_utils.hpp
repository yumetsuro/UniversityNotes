#pragma once
#include "record.hpp"
#include <cstddef>
#include <cstring>

/**
 * @brief Conquer step of the merge sort algorithm. Here we merge two already sorted subarray in a single array.
 * 
 * @param array 
 * @param left 
 * @param mid 
 * @param right 
 * @param tmp 
 */
inline void merge(Record* array, size_t left, size_t mid, size_t right, Record* tmp) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = 0;
    
    while (i <= mid && j <= right) {
        if (array[i].key <= array[j].key) {
            tmp[k].key = array[i].key;
            tmp[k].rpayload = array[i].rpayload;
            if (SIMULATE_TRANSFER)
                memcpy(tmp[k].rpayload, array[i].rpayload, g_payload_size);
            i++;
        } else {
            tmp[k].key = array[j].key;
            tmp[k].rpayload = array[j].rpayload;
            if (SIMULATE_TRANSFER)
                memcpy(tmp[k].rpayload, array[j].rpayload, g_payload_size);
            j++;
        }
        k++;
    }
    
    while (i <= mid) {
        tmp[k].key = array[i].key;
        tmp[k].rpayload = array[i].rpayload;
        if (SIMULATE_TRANSFER)
            memcpy(tmp[k].rpayload, array[i].rpayload, g_payload_size);
        i++; k++;
    }
    
    while (j <= right) {
        tmp[k].key = array[j].key;
        tmp[k].rpayload = array[j].rpayload;
        if (SIMULATE_TRANSFER)
            memcpy(tmp[k].rpayload, array[j].rpayload, g_payload_size);
        j++; k++;
    }
    
    // Copy back to original array
    for (size_t idx = 0; idx < k; idx++) {
        array[left + idx] = tmp[idx];
    }
}

/**
 * @brief Divide step of the merge sort, we take a subarray and we completely order it with mergesort.
 * 
 * @param array 
 * @param left 
 * @param right 
 * @param tmp 
 */
inline void mergesort(Record* array, size_t left, size_t right, Record* tmp) {
    if (left >= right) return;
    
    size_t mid = (left + right) / 2;
    
    mergesort(array, left, mid, tmp);
    mergesort(array, mid + 1, right, tmp);
    
    merge(array, left, mid, right, tmp);
}
