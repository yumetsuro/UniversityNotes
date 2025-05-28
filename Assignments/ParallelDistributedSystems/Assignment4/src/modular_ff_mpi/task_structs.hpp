#pragma once
#include "record.hpp"
#include <cstddef>

/**
 * @brief Structure to represent a sorting task.
 * 
 */
struct SortTask {
    Record* array;
    size_t left;
    size_t right;
    
    SortTask() : array(nullptr), left(0), right(0) {}
    SortTask(Record* arr, size_t l, size_t r)
        : array(arr), left(l), right(r) {}
};

/**
 * @brief Structure to represent a merging task.
 * This structure holds the array and the indices of the two sorted subarrays to be merged.
 */
struct MergeTask {
    Record* array;
    size_t left;
    size_t mid;
    size_t right;
    
    MergeTask() : array(nullptr), left(0), mid(0), right(0) {}
    MergeTask(Record* arr, size_t l, size_t m, size_t r)
        : array(arr), left(l), mid(m), right(r) {}
};
