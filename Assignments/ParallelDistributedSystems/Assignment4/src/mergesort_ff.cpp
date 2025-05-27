#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <ff/pipeline.hpp>
#include <ff/utils.hpp>
#include <chrono>
#include <getopt.h>

using namespace ff;
using namespace std;

// Global variable for runtime payload size configuration
size_t g_payload_size = 8;  // Default payload size

// Record structure with maximum payload size
#ifndef MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD_SIZE 256
#endif

// Replace the existing Record structure with this:
struct Record {
    unsigned long key;  // sorting value
    char* rpayload;     // pointer to payload
    
    // Constructor
    Record() : key(0), rpayload(nullptr) {}
    
    // Copy constructor - only copies key and pointer, not payload data
    Record(const Record& other) : key(other.key), rpayload(other.rpayload) {}
    
    // Assignment operator - only copies key and pointer
    Record& operator=(const Record& other) {
        if (this != &other) {
            key = other.key;
            rpayload = other.rpayload;
        }
        return *this;
    }
    
    // Get the record size (just key + pointer)
    static size_t get_record_size() {
        return sizeof(unsigned long) + sizeof(char*);
    }
};

// Modified function to create record array with inline payload allocation
std::pair<Record*, char*> create_record_array_with_payload(size_t n) {
    Record* array = new Record[n];
    
    // Allocate one contiguous block for all payloads
    char* payload_block = new char[n * g_payload_size];
    
    // Initialize records with random keys and payload pointers
    for (size_t i = 0; i < n; i++) {
        array[i].key = rand();
        array[i].rpayload = payload_block + (i * g_payload_size);
        
        // Initialize payload with some pattern (optional)
        for (size_t j = 0; j < g_payload_size; j++) {
            array[i].rpayload[j] = static_cast<char>(j % 256);
        }
    }
    
    return std::make_pair(array, payload_block);
}
// Function to verify the sorted array
bool verify_sorted(Record* array, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (array[i-1].key > array[i].key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << array[i-1].key << " > " << array[i].key << std::endl;
            return false;
        }
    }
    return true;
}

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

void mergesort(Record* array, size_t left, size_t right, Record* tmp) {
    if (left >= right) return;
    
    size_t mid = (left + right) / 2;
    
    mergesort(array, left, mid, tmp);
    mergesort(array, mid + 1, right, tmp);
    
    merge(array, left, mid, right, tmp);
}

// Structure to represent a chunk of the array to be sorted
struct SortTask {
    Record* array;
    size_t left;
    size_t right;
    
    SortTask() : array(nullptr), left(0), right(0) {}
    SortTask(Record* arr, size_t l, size_t r)
        : array(arr), left(l), right(r) {}
};

// Structure to represent two sorted chunks to be merged
struct MergeTask {
    Record* array;
    size_t left;
    size_t mid;
    size_t right;
    
    MergeTask() : array(nullptr), left(0), mid(0), right(0) {}
    MergeTask(Record* arr, size_t l, size_t m, size_t r)
        : array(arr), left(l), mid(m), right(r) {}
};

// Worker for the initial sorting phase
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
// Worker for the merging phase
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

// Emitter for the initial sorting phase
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

// Collector for the sorting phase that also initiates the merge phase
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

// Emitter for the merge phase - creates merge tasks in a tree-like fashion
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

// Collector for the merge phase
class MergeCollector : public ff_node {
public:
    void* svc(void* task) {
        // Just consume the completed merge tasks
        return GO_ON;
    }
};

// Function to parse size with suffixes (K, M, G)
size_t parse_size(const char* str) {
    char* end;
    size_t size = strtoull(str, &end, 10);
    
    if (*end) {
        switch (*end) {
            case 'k': case 'K': size *= 1000; break;
            case 'm': case 'M': size *= 1000000; break;
            case 'g': case 'G': size *= 1000000000; break;
            default: 
                std::cerr << "Invalid size suffix: " << *end << std::endl;
                exit(1);
        }
    }
    
    return size;
}

// Create a simple emitter that sends all tasks
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
                return nullptr; // EOS
            }
};

// Display usage information
void usage(const char* progname) {
    std::cerr << "Usage: " << progname << " [OPTIONS]\n"
              << "Options:\n"
              << "  -s, --size N        Array size (e.g., 10M, 100M)\n"
              << "  -r, --record R      Record payload size in bytes (e.g., 8, 64, 256)\n"
              << "  -t, --threads T     Number of FastFlow threads (e.g., 16, 32)\n"
              << "  -q, --sequential    Run sequential version\n"
              << "  -h, --help          Display this help message\n"
              << "Note: Maximum payload size is " << MAX_PAYLOAD_SIZE << " bytes\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    size_t array_size = 1000000;  // Default: 1M
    size_t num_threads = 4;       // Default: 4 threads
    bool sequential = false;      // Default: parallel execution
   
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
                g_payload_size = atoi(optarg);
                if (g_payload_size > MAX_PAYLOAD_SIZE) {
                    std::cerr << "Error: Payload size " << g_payload_size 
                              << " exceeds maximum " << MAX_PAYLOAD_SIZE << " bytes" << std::endl;
                    return 1;
                }
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
    
    // Record size includes the actual payload size being used
    size_t record_size = Record::get_record_size();
    size_t total_payload_size = g_payload_size * array_size;

    std::cout << "MergeSort Configuration:\n"
              << "  Array size: " << array_size << " elements\n"
              << "  Record payload: " << g_payload_size << " bytes\n"
              << "  Record total size: " << record_size << " bytes\n"
              << "  Total payload size: " << total_payload_size / (1024*1024) << " MB\n"
              << "  Total memory: " << (array_size * record_size) / (1024*1024) << " MB\n"
              << "  Mode: " << (sequential ? "Sequential" : "Parallel with " + std::to_string(num_threads) + " threads")
              << std::endl;
    
    // Create the array of records with random keys
    auto result = create_record_array_with_payload(array_size);
    Record* array = result.first;
    char* payload_block = result.second;
   //Record* array = create_record_array(array_size);
    
    // Temporary array for merging in sequential version
    Record* tmp = nullptr;
    if (sequential) {
        tmp = new Record[array_size];
    }
    
    // Time variables for different phases
    auto start_time = std::chrono::high_resolution_clock::now();
    auto sort_start_time = start_time;
    auto sort_end_time = start_time;
    auto merge_start_time = start_time;
    auto merge_end_time = start_time;
    
    if (sequential) {
        // Run sequential mergesort
        mergesort(array, 0, array_size - 1, tmp);
    } else {
        // Run parallel mergesort using FastFlow with optimized parallel merge
        
        // Phase 1: Sort chunks in parallel using a farm
        sort_start_time = std::chrono::high_resolution_clock::now();
        
        ff_farm sort_farm;
        
        // Create workers for the sorting phase
        std::vector<ff_node*> sort_workers;
        for (size_t i = 0; i < num_threads; i++) {
            sort_workers.push_back(new SortWorker());
        }
        
        // Set emitter and collector for sort farm
        SortEmitter* sort_emitter = new SortEmitter(array, array_size, num_threads);
        SortCollector* sort_collector = new SortCollector(array, array_size);
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
                    
                    merge_tasks.push_back(new MergeTask(array, left, mid, right));
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
                    merge_workers_vec.push_back(new MergeWorker());
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
    bool sorted = verify_sorted(array, array_size);
    
    std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
              << "Total time: " << total_duration.count() << " ms\n";
              
    if (!sequential) {
        std::cout << "  Sort phase: " << sort_duration.count() << " ms (" 
                  << (sort_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                  << "  Merge phase: " << merge_duration.count() << " ms ("
                  << (merge_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                  << "  Overhead: " << overhead_duration.count() << " ms ("
                  << (overhead_duration.count() * 100.0 / total_duration.count()) << "%)\n";
    }
    
    // Clean up
    delete[] array;
    delete[] payload_block; // Free the payload block
    if (sequential) {
        delete[] tmp;
    }
    
    return sorted ? 0 : 1;
}
