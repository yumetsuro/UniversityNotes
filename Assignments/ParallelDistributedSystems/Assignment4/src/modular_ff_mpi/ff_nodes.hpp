#pragma once
#include "record.hpp"
#include "task_structs.hpp"
#include "sort_utils.hpp"
#include <ff/ff.hpp>
#include <vector>
#include <algorithm>

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
        
        Record* merge_tmp = new Record[n];
        
        // Sort this chunk using the sequential mergesort
        mergesort(st->array, st->left, st->right, merge_tmp);
        
        delete[] merge_tmp;
        
        return task;
    }
};

/**
 * @brief FasfFlow worker for the merging phase
 * This node takes a MergeTask, merges the specified chunks of the array and finish.
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
        delete mt; 
        
        return nullptr; 
    }
};

/**
 * @brief Emitter for the sorting phase
 * This node emits SortTask objects to the SortWorker nodes.
 * It divides the array into chunks based on the number of workers and sends each chunk
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
 * @brief Emitter for merge tasks
 * This node emits MergeTask objects to the MergeWorker nodes.
 * It iterates through the provided tasks vector and returns each task one by one.
 * If all tasks have been emitted, it returns nullptr to signal the end of the stream.
 * This is a simple implementation that does not handle dynamic task generation or EOS signaling.
 */
class MergeEmitter : public ff_node {
private:
    std::vector<MergeTask*>& tasks;
    size_t current_task;
public:
    MergeEmitter(std::vector<MergeTask*>& t) : tasks(t), current_task(0) {}
    
    void* svc(void* task) {
        
        if (current_task < tasks.size()) {
            return tasks[current_task++];
        }

        return nullptr;
    }
};

/**
 * @brief Collector for the merge phase 
 * This node collects the results of the merge tasks.
 * It simply consumes the tasks and returns GO_ON to continue processing.
 */
class MergeCollector : public ff_node {
public:
    void* svc(void* task) {
        return GO_ON;
    }
};
