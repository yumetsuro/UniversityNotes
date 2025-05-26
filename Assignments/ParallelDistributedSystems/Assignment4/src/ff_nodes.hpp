#pragma once
#include <ff/ff.hpp>
#include <vector>
#include <algorithm>
#include "task_structs.hpp"
#include "sort_utils.hpp"

using namespace ff;

/**
 * @brief Worker node for sorting tasks using mergesort.
 * This node takes a chunks of the array and sort it using mergesort then
 * returns the SortTask back to the collector.
 */
class SortWorker : public ff_node {
public:
    SortWorker() {}
    
    void* svc(void* task) {
        SortTask* st = static_cast<SortTask*>(task);
        size_t n = st->right - st->left + 1;
        
        if (n <= 1) {
            return task;
        }
        
        // Create a temporary buffer for merging operations
        Record* merge_tmp = new Record[n];
        
        // Sort this chunk using the sequential mergesort
        mergesort(st->array, st->left, st->right, merge_tmp);
        
        delete[] merge_tmp;
        
        return task;
    }
};

/**
 * @brief
 */
class MergeWorker : public ff_node {
public:
    MergeWorker() {}
    
    void* svc(void* task) {
        MergeTask* mt = static_cast<MergeTask*>(task);
        
        size_t n = mt->right - mt->left + 1;
        Record* tmp = new Record[n];
        
        merge(mt->array, mt->left, mt->mid, mt->right, tmp);
        
        delete[] tmp;
        delete mt;  // Free the task
        
        return nullptr; // No output
    }
};

/**
 * @brief
 */
class SortEmitter : public ff_node {
private:
    Record* array;
    size_t n;
    size_t num_workers;
    
public:
    SortEmitter(Record* arr, size_t size, size_t nw)
        : array(arr), n(size), num_workers(nw) {}
    
    void* svc(void* task) {
        size_t chunk_size = n / num_workers;
        if (chunk_size == 0) chunk_size = 1;
        
        for (size_t i = 0; i < n; i += chunk_size) {
            size_t right = std::min(i + chunk_size - 1, n - 1);
            SortTask* st = new SortTask(array, i, right);
            ff_send_out(st);
        }
        
        return nullptr; // EOS
    }
};

/**
 * @brief Collector for the sorting phase
 * This node collects SortTask from SortWorkers, then 
 * sorts them based on their left index in order to merge 
 * and initiates the merge phase.
 */
class SortCollector : public ff_node {
private:
    Record* array;
    size_t n;
    std::vector<SortTask*> sorted_chunks;
    
public:
    SortCollector(Record* arr, size_t size)
        : array(arr), n(size) {}
    
    void* svc(void* task) {
        SortTask* st = static_cast<SortTask*>(task);
        sorted_chunks.push_back(st);
        return GO_ON;
    }
    
    void svc_end() {
        // Sort chunks by left index to ensure correct order
        std::sort(sorted_chunks.begin(), sorted_chunks.end(),
            [](const SortTask* a, const SortTask* b) { return a->left < b->left; });
        
        // Free all chunks as we don't need them anymore
        for (auto chunk : sorted_chunks) {
            delete chunk;
        }
        sorted_chunks.clear();
    }
};

/**
 * @brief Simple emitter for merge tasks
 * This node emits MergeTask objects to the MergeWorker nodes.
 * It iterates through the provided tasks vector and returns each task one by one.
 * If all tasks have been emitted, it returns nullptr to signal the end of the stream.
 * This is a simple implementation that does not handle dynamic task generation or EOS signaling.
 */
class SimpleMergeEmitter : public ff_node {
private:
    std::vector<MergeTask*>& tasks;
    size_t current_task;
public:
    SimpleMergeEmitter(std::vector<MergeTask*>& t) : tasks(t), current_task(0) {}
    
    void* svc(void* task) {
        
        if (current_task < tasks.size()) {
            return tasks[current_task++];
        }

        return nullptr;
    }
};

/**
 * @brief Not yet used
 * 
 */
class MergeEmitter : public ff_node {
private:
    Record* array;
    size_t n;
    size_t num_chunks;
    std::vector<std::pair<size_t, size_t>> current_ranges;
    bool first_iteration;
    
public:
    MergeEmitter(Record* arr, size_t size, size_t chunks)
        : array(arr), n(size), num_chunks(chunks), first_iteration(true) {
        
        // Initialize with the original chunk ranges
        size_t chunk_size = n / num_chunks;
        if (chunk_size == 0) chunk_size = 1;
        
        for (size_t i = 0; i < n; i += chunk_size) {
            size_t right = std::min(i + chunk_size - 1, n - 1);
            current_ranges.push_back({i, right});
        }
    }
    
    void* svc(void* task) {
        if (first_iteration) {
            first_iteration = false;
            
            // Start the merge process
            for (size_t i = 0; i < current_ranges.size(); i += 2) {
                if (i + 1 < current_ranges.size()) {
                    // Create a merge task for adjacent ranges
                    size_t left = current_ranges[i].first;
                    size_t mid = current_ranges[i].second;
                    size_t right = current_ranges[i + 1].second;
                    
                    MergeTask* mt = new MergeTask(array, left, mid, right);
                    ff_send_out(mt);
                }
            }
            
            return GO_ON;
        }
        
        return nullptr; // EOS after first iteration
    }
};

/**
 * @brief Collector for the merge phase 
 * This node collects the results of the merge tasks.
 * It does not perform any specific action on the collected tasks,
 * but it can be extended to handle the results if needed.
 * It simply consumes the tasks and returns GO_ON to continue processing.
 * This is a placeholder implementation that can be extended later.
 * It does not handle EOS signaling or task completion.
 */
class MergeCollector : public ff_node {
public:
    void* svc(void* task) {
        return GO_ON;
    }
};
