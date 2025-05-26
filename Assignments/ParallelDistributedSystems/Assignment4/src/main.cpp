#include "record.hpp"
#include "sort_utils.hpp"
#include "task_structs.hpp"
#include "ff_nodes.hpp"
#include "utils.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <getopt.h>
#include <cstdlib>
#include <ctime>
#include <ff/farm.hpp>

using namespace ff;
using namespace std;

int main(int argc, char* argv[]) {
    // Default parameters
    size_t array_size = 1000000;  
    size_t record_payload = 8;    
    size_t num_threads = 4;       
    bool sequential = false;      
   
    // Parse command line options
    static struct option long_options[] = {
        {"size",       required_argument, 0, 's'},
        {"record",     required_argument, 0, 'r'},
        {"threads",    required_argument, 0, 't'},
        {"sequential", no_argument,       0, 'q'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "s:r:t:qh", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                array_size = parse_size(optarg);
                break;
            case 'r':
                record_payload = atoi(optarg);
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'q':
                sequential = true;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case '?':
                usage(argv[0]);
                return 1;
            default:
                abort();
        }
    }
    
    // Initialize random number generator
    srand(time(nullptr));
    
    // Actual record size including the payload
    size_t record_size = sizeof(unsigned long) + record_payload;
    
    std::cout << "MergeSort Configuration:\n"
              << "  Array size: " << array_size << " elements\n"
              << "  Record payload: " << record_payload << " bytes\n"
              << "  Record total size: " << record_size << " bytes\n"
              << "  Total memory: " << (array_size * record_size) / (1024*1024) << " MB\n"
              << "  Mode: " << (sequential ? "Sequential" : "Parallel with " + std::to_string(num_threads) + " threads")
              << std::endl;
    
    // Create the array of records with random keys
    Record* array = create_record_array(array_size, record_payload);
    
    // Temporary array for merging in sequential version
    Record* tmp = nullptr;
    if (sequential) {
        tmp = reinterpret_cast<Record*>(new char[array_size * record_size]);
    }
    
    // Time variables for different phases
    auto start_time = std::chrono::high_resolution_clock::now();
    auto sort_start_time = start_time;
    auto sort_end_time = start_time;
    auto merge_start_time = start_time;
    auto merge_end_time = start_time;
    
    if (sequential) {
        // Run sequential mergesort
        mergesort(array, 0, array_size - 1, record_size, tmp);
    } else {
        // Run parallel mergesort using FastFlow with optimized parallel merge
        
        // Phase 1: Sort chunks in parallel using a farm
        sort_start_time = std::chrono::high_resolution_clock::now();
        
        ff_farm sort_farm;
        
        // Create workers for the sorting phase
        std::vector<ff_node*> sort_workers;
        for (size_t i = 0; i < num_threads; i++) {
            sort_workers.push_back(new SortWorker(record_size));
        }
        
        // Set emitter and collector for sort farm
        SortEmitter* sort_emitter = new SortEmitter(array, array_size, num_threads, record_size);
        SortCollector* sort_collector = new SortCollector(array, array_size, record_size);
        sort_farm.add_emitter(sort_emitter);
        sort_farm.add_workers(sort_workers);
        sort_farm.add_collector(sort_collector);
        
        // Run the sort farm
        if (sort_farm.run_and_wait_end() < 0) {
            std::cerr << "Error running sort farm" << std::endl;
            return 1;
        }
        
        sort_end_time = std::chrono::high_resolution_clock::now();
        
        // Phase 2: Tree-like parallel merge using multiple levels
        merge_start_time = std::chrono::high_resolution_clock::now();
        
        // Initialize ranges for merging
        std::vector<std::pair<size_t, size_t>> current_ranges;
        size_t chunk_size = array_size / num_threads;
        if (chunk_size == 0) chunk_size = 1;
        
        for (size_t i = 0; i < array_size; i += chunk_size) {
            size_t right = std::min(i + chunk_size - 1, array_size - 1);
            current_ranges.push_back({i, right});
        }
        
        // Merge level by level until we have a single sorted array
        while (current_ranges.size() > 1) {
            std::vector<std::pair<size_t, size_t>> next_ranges;
            std::vector<MergeTask*> merge_tasks;
            
            // Create merge tasks for this level
            for (size_t i = 0; i < current_ranges.size(); i += 2) {
                if (i + 1 < current_ranges.size()) {
                    size_t left = current_ranges[i].first;
                    size_t mid = current_ranges[i].second;
                    size_t right = current_ranges[i + 1].second;
                    
                    merge_tasks.push_back(new MergeTask(array, left, mid, right, record_size));
                    next_ranges.push_back({left, right});
                } else {
                    // Odd number of ranges, carry forward the last one
                    next_ranges.push_back(current_ranges[i]);
                }
            }
            
            // Process merge tasks in parallel if we have any
            if (!merge_tasks.empty()) {
                ff_farm merge_farm;
                
                // Create workers for the merging phase (use fewer workers for better load balancing)
                size_t merge_workers = std::min((size_t)merge_tasks.size(), num_threads);
                std::vector<ff_node*> merge_workers_vec;
                for (size_t i = 0; i < merge_workers; i++) {
                    merge_workers_vec.push_back(new MergeWorker(record_size));
                }
                     
                SimpleMergeEmitter* merge_emitter = new SimpleMergeEmitter(merge_tasks);
                MergeCollector* merge_collector = new MergeCollector();
                
                merge_farm.add_emitter(merge_emitter);
                merge_farm.add_workers(merge_workers_vec);
                merge_farm.add_collector(merge_collector);
                
                // Run the merge farm for this level
                if (merge_farm.run_and_wait_end() < 0) {
                    std::cerr << "Error running merge farm" << std::endl;
                    return 1;
                }
            }
            
            current_ranges = next_ranges;
        }
        
        merge_end_time = std::chrono::high_resolution_clock::now();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate durations using FastFlow timing approach
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end_time - sort_start_time);
    auto merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end_time - merge_start_time);
    auto overhead_duration = total_duration - sort_duration - merge_duration;
    
    // Verify that the array is sorted
    bool sorted = verify_sorted(array, array_size, record_size);
    
    std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
              << "Total time: " << total_duration.count() << " ms\n";
              
    if (!sequential) {
        std::cout << "  Sort phase: " << sort_duration.count() << " ms (" 
                  << (sort_duration.count() * 100.0 / total_duration.count()) << "% )\n"
                  << "  Merge phase: " << merge_duration.count() << " ms ("
                  << (merge_duration.count() * 100.0 / total_duration.count()) << "% )\n"
                  << "  Overhead: " << overhead_duration.count() << " ms ("
                  << (overhead_duration.count() * 100.0 / total_duration.count()) << "% )\n";
    }
    
    // Clean up
    delete[] reinterpret_cast<char*>(array);
    if (sequential) {
        delete[] reinterpret_cast<char*>(tmp);
    }
    
    return sorted ? 0 : 1;
}
