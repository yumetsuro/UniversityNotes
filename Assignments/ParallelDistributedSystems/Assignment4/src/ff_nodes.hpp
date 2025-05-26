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
private:
    size_t record_size;
public:
    SortWorker(size_t rs) : record_size(rs) {}
    
    void* svc(void* task) {

        SortTask* st = static_cast<SortTask*>(task);
        size_t n = st->right - st->left + 1;
        
        if (n <= 1) return task;

        Record* tmp = reinterpret_cast<Record*>(new char[n * record_size]);
        Record* merge_tmp = reinterpret_cast<Record*>(new char[n * record_size]);

        memcpy(reinterpret_cast<char*>(tmp), 
               reinterpret_cast<char*>(st->array) + st->left * record_size,
               n * record_size);

        mergesort(tmp, 0, n - 1, record_size, merge_tmp);

        memcpy(reinterpret_cast<char*>(st->array) + st->left * record_size,
               reinterpret_cast<char*>(tmp),
               n * record_size);

        delete[] reinterpret_cast<char*>(tmp);
        delete[] reinterpret_cast<char*>(merge_tmp);

        return task;
    }
};

/**
 * @brief
 */
class MergeWorker : public ff_node {
private:
    size_t record_size;
public:
    MergeWorker(size_t rs) : record_size(rs) {}

    void* svc(void* task) {
    
        MergeTask* mt = static_cast<MergeTask*>(task);
    
        size_t n = mt->right - mt->left + 1;
    
        Record* tmp = reinterpret_cast<Record*>(new char[n * record_size]);
    
        merge(mt->array, mt->left, mt->mid, mt->right, record_size, tmp);
    
        delete[] reinterpret_cast<char*>(tmp);
        delete mt;
    
        return nullptr;
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
    size_t record_size;
public:
    SortEmitter(Record* arr, size_t size, size_t nw, size_t rs)
        : array(arr), n(size), num_workers(nw), record_size(rs) {}
    
        void* svc(void* task) {
    
            size_t chunk_size = n / num_workers;
            if (chunk_size == 0) chunk_size = 1;
        
            for (size_t i = 0; i < n; i += chunk_size) {
        
                size_t right = std::min(i + chunk_size - 1, n - 1);
        
                SortTask* st = new SortTask(array, i, right, record_size);
        
                ff_send_out(st);
            }
        return nullptr;
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
    size_t record_size;
    std::vector<SortTask*> sorted_chunks;
public:
    SortCollector(Record* arr, size_t size, size_t rs)
        : array(arr), n(size), record_size(rs) {}
    
        void* svc(void* task) {
    
            SortTask* st = static_cast<SortTask*>(task);
    
            sorted_chunks.push_back(st);
    
            return GO_ON;
        }
    
        void svc_end() {

            // In FastFlow we cannot predict what chunks will be received first,
            // so we need to sort them by their left index to ensure correct order- 
            std::sort(sorted_chunks.begin(), sorted_chunks.end(),
            [](const SortTask* a, const SortTask* b) { return a->left < b->left; });
            
            for (auto chunk : sorted_chunks) delete chunk;
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
