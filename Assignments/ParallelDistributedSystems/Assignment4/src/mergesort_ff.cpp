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
#include <chrono>
#include <getopt.h>

using namespace ff;
using namespace std;

// Record structure as per assignment
struct Record {
    unsigned long key;  // sorting value
    char rpayload[1];   // flexible array member, actual size will be set at runtime
};

// Function to dynamically create a record array with specific payload size
Record* create_record_array(size_t n, size_t payload_size) {
    // Allocate memory for the array with the correct record size
    size_t record_size = sizeof(unsigned long) + payload_size;
    char* memory = new char[n * record_size];
    
    // Initialize records with random keys
    for (size_t i = 0; i < n; i++) {
        Record* rec = reinterpret_cast<Record*>(memory + i * record_size);
        rec->key = rand();
        
        // Initialize payload with some pattern (optional)
        for (size_t j = 0; j < payload_size; j++) {
            rec->rpayload[j] = static_cast<char>(j % 256);
        }
    }
    
    return reinterpret_cast<Record*>(memory);
}

// Function to verify the sorted array
bool verify_sorted(Record* array, size_t n, size_t record_size) {
    for (size_t i = 1; i < n; i++) {
        Record* prev = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + (i-1) * record_size);
        Record* curr = reinterpret_cast<Record*>(reinterpret_cast<char*>(array) + i * record_size);
        if (prev->key > curr->key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << prev->key << " > " << curr->key << std::endl;
            return false;
        }
    }
    return true;
}

// Sequential mergesort implementation
void merge(Record* array, size_t left, size_t mid, size_t right, size_t record_size, Record* tmp) {
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
    
    // Copy back to original array
    memcpy(reinterpret_cast<char*>(array) + left * record_size, 
           reinterpret_cast<char*>(tmp), 
           k * record_size);
}

void mergesort_seq(Record* array, size_t left, size_t right, size_t record_size, Record* tmp) {
    if (left >= right) return;
    
    size_t mid = (left + right) / 2;
    
    mergesort_seq(array, left, mid, record_size, tmp);
    mergesort_seq(array, mid + 1, right, record_size, tmp);
    
    merge(array, left, mid, right, record_size, tmp);
}

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

// Worker for the initial sorting phase
class SortWorker : public ff_node {
private:
    size_t record_size;
    
public:
    SortWorker(size_t rs) : record_size(rs) {}
    
    void* svc(void* task) {
        SortTask* st = static_cast<SortTask*>(task);
        size_t n = st->right - st->left + 1;
        
        if (n <= 1) {
            return task;
        }
        
        // Create a temporary buffer for the sorted chunk
        Record* tmp = reinterpret_cast<Record*>(new char[n * record_size]);
        
        // Sort just this chunk using the sequential mergesort
        // First create a temporary buffer for merging operations
        Record* merge_tmp = reinterpret_cast<Record*>(new char[n * record_size]);
        
        // Copy the chunk to the temporary buffer
        memcpy(reinterpret_cast<char*>(tmp), 
               reinterpret_cast<char*>(st->array) + st->left * record_size,
               n * record_size);
               
        // Sort the temporary buffer
        mergesort_seq(tmp, 0, n - 1, record_size, merge_tmp);
        
        // Copy back to original array
        memcpy(reinterpret_cast<char*>(st->array) + st->left * record_size,
               reinterpret_cast<char*>(tmp),
               n * record_size);
        
        delete[] reinterpret_cast<char*>(tmp);
        delete[] reinterpret_cast<char*>(merge_tmp);
        
        return task;
    }
};

// Worker for the merging phase
class MergeWorker : public ff_node {
private:
    size_t record_size;
    
public:
    MergeWorker(size_t rs) : record_size(rs) {}
    
    void* svc(void* task) {
        MergeTask* mt = static_cast<MergeTask*>(task);
        
        size_t n = mt->right - mt->left + 1;
        Record* tmp = reinterpret_cast<Record*>(new char[n * record_size]);
        
        // Debug info
        //std::cout << "Merging: left=" << mt->left << " mid=" << mt->mid << " right=" << mt->right << std::endl;
        
        merge(mt->array, mt->left, mt->mid, mt->right, record_size, tmp);
        
        // Verify the merged segment is sorted
        /*
        bool segment_sorted = true;
        for (size_t i = mt->left + 1; i <= mt->right; i++) {
            Record* prev = reinterpret_cast<Record*>(reinterpret_cast<char*>(mt->array) + (i-1) * record_size);
            Record* curr = reinterpret_cast<Record*>(reinterpret_cast<char*>(mt->array) + i * record_size);
            if (prev->key > curr->key) {
                std::cerr << "Merge failed at positions " << (i-1) << " and " << i 
                          << " (task range " << mt->left << "-" << mt->right << ")"
                          << ", keys: " << prev->key << " > " << curr->key << std::endl;
                segment_sorted = false;
                break;
            }
        }
        if (!segment_sorted) {
            std::cerr << "Segment not sorted after merge!" << std::endl;
        }
        */
        
        delete[] reinterpret_cast<char*>(tmp);
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
        
        return nullptr; // EOS
    }
};

// Global variables for timing
std::chrono::milliseconds collector_merge_time(0);

// Collector for the sorting phase, emitter for merge phase
class SortCollectorMergeEmitter : public ff_node {
private:
    Record* array;
    size_t n;
    size_t record_size;
    std::vector<SortTask*> sorted_chunks;
    
public:
    SortCollectorMergeEmitter(Record* arr, size_t size, size_t rs)
        : array(arr), n(size), record_size(rs) {}
    
    void* svc(void* task) {
        SortTask* st = static_cast<SortTask*>(task);
        sorted_chunks.push_back(st);
        return GO_ON;
    }
    
    void svc_end() {
        // Sort chunks by left index to ensure correct order
        std::sort(sorted_chunks.begin(), sorted_chunks.end(),
            [](const SortTask* a, const SortTask* b) { return a->left < b->left; });
        
        auto merge_start = std::chrono::high_resolution_clock::now();
        
        // Sequential merge of all chunks
        if (sorted_chunks.size() > 1) {
            // Create a temporary buffer for the entire array
            Record* tmp = reinterpret_cast<Record*>(new char[n * record_size]);
            
            // Start with the first two chunks
            size_t left = sorted_chunks[0]->left;
            size_t mid = sorted_chunks[0]->right;
            size_t right = sorted_chunks[1]->right;
            
            // Merge the first two chunks
            merge(array, left, mid, right, record_size, tmp);
            
            // Now merge each subsequent chunk with the accumulated result
            for (size_t i = 2; i < sorted_chunks.size(); i++) {
                left = sorted_chunks[0]->left;  // Always start from the beginning
                mid = right;                   // Previous right becomes the new mid
                right = sorted_chunks[i]->right; // New right
                
                merge(array, left, mid, right, record_size, tmp);
            }
            
            delete[] reinterpret_cast<char*>(tmp);
        }
        
        auto merge_end = std::chrono::high_resolution_clock::now();
        collector_merge_time = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end - merge_start);
        
        // Free all chunks
        for (auto chunk : sorted_chunks) {
            delete chunk;
        }
        sorted_chunks.clear();
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

// Display usage information
void usage(const char* progname) {
    std::cerr << "Usage: " << progname << " [OPTIONS]\n"
              << "Options:\n"
              << "  -s, --size N        Array size (e.g., 10M, 100M)\n"
              << "  -r, --record R      Record payload size in bytes (e.g., 8, 64, 256)\n"
              << "  -t, --threads T     Number of FastFlow threads (e.g., 16, 32)\n"
              << "  -q, --sequential    Run sequential version\n"
              << "  -h, --help          Display this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    size_t array_size = 1000000;  // Default: 1M
    size_t record_payload = 8;    // Default: 8 bytes
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
        mergesort_seq(array, 0, array_size - 1, record_size, tmp);
    } else {
        // Run parallel mergesort using FastFlow
        
        // Start timing for sort phase
        
        // Phase 1: Sort chunks in parallel
        ff_farm sort_farm;
        
        // Create workers for the sorting phase
        std::vector<ff_node*> sort_workers;
        for (size_t i = 0; i < num_threads; i++) {
            sort_workers.push_back(new SortWorker(record_size));
        }
        
        // Set emitter and workers
        SortEmitter* emitter = new SortEmitter(array, array_size, num_threads, record_size);
        SortCollectorMergeEmitter* collector = new SortCollectorMergeEmitter(array, array_size, record_size);
        sort_farm.add_emitter(emitter);
        sort_farm.add_workers(sort_workers);
        sort_farm.add_collector(collector);
        
        sort_start_time = std::chrono::high_resolution_clock::now();
        
        // Run the farm
        if (sort_farm.run_and_wait_end() < 0) {
            std::cerr << "Error running sort farm" << std::endl;
            return 1;
        }
        
        sort_end_time = std::chrono::high_resolution_clock::now();
        merge_start_time = sort_end_time;
        
        // Note: In our current implementation, merging happens inside the collector
        merge_end_time = std::chrono::high_resolution_clock::now();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate durations
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end_time - sort_start_time);
    
    // Verify that the array is sorted
    bool sorted = verify_sorted(array, array_size, record_size);
    
    std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
              << "Total time: " << total_duration.count() << " ms\n";
              
    if (!sequential) {
        // Compute the actual sort time (excluding merging)
        auto actual_sort_time = sort_duration - collector_merge_time;
        
        std::cout << "  Sort phase: " << actual_sort_time.count() << " ms (" 
                  << (actual_sort_time.count() * 100.0 / total_duration.count()) << "%)\n"
                  << "  Merge phase: " << collector_merge_time.count() << " ms ("
                  << (collector_merge_time.count() * 100.0 / total_duration.count()) << "%)\n";
    }
    
    // Clean up
    delete[] reinterpret_cast<char*>(array);
    if (sequential) {
        delete[] reinterpret_cast<char*>(tmp);
    }
    
    return sorted ? 0 : 1;
}
