#pragma once
#include "record.hpp"
#include <cstddef>

// Structure to represent a chunk of the array to be sorted
struct SortTask {
    Record* array;
    size_t left;
    size_t right;
    size_t record_size;
    SortTask() : array(nullptr), left(0), right(0), record_size(0) {}
    SortTask(Record* arr, size_t l, size_t r, size_t rs)
        : array(arr), left(l), right(r), record_size(rs) {}
};

// Structure to represent two sorted chunks to be merged
struct MergeTask {
    Record* array;
    size_t left;
    size_t mid;
    size_t right;
    size_t record_size;
    MergeTask() : array(nullptr), left(0), mid(0), right(0), record_size(0) {}
    MergeTask(Record* arr, size_t l, size_t m, size_t r, size_t rs)
        : array(arr), left(l), mid(m), right(r), record_size(rs) {}
};
