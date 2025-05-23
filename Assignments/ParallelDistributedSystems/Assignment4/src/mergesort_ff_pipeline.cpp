// Pipeline implementation of parallel mergesort using FastFlow
// This version uses a pipeline of farms for better parallelization and communication modeling
// Designed to help with the MPI implementation strategy

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
#include <ff/parallel_for.hpp>
#include <chrono>
#include <getopt.h>
#include <thread>
#include <mutex>
#include <tuple>

using namespace ff;
using namespace std;

// Global timing variables
std::chrono::milliseconds sort_duration(0);
std::chrono::milliseconds merge_duration(0);

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

// Sequential mergesort implementation (utility function)
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
    size_t chunk_id;  // Added to track the chunk's position in the overall sequence
    
    SortTask() : array(nullptr), left(0), right(0), record_size(0), chunk_id(0) {}
    SortTask(Record* arr, size_t l, size_t r, size_t rs, size_t id = 0)
        : array(arr), left(l), right(r), record_size(rs), chunk_id(id) {}
};

// Structure to represent two sorted chunks to be merged
struct MergeTask {
    Record* array;
    size_t left;
    size_t mid;
    size_t right;
    size_t record_size;
    size_t level;  // Merge level in the hierarchy
    
    MergeTask() : array(nullptr), left(0), mid(0), right(0), record_size(0), level(0) {}
    MergeTask(Record* arr, size_t l, size_t m, size_t r, size_t rs, size_t lv = 0)
        : array(arr), left(l), mid(m), right(r), record_size(rs), level(lv) {}
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

// Node for the emitter of the sort phase
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
            SortTask* st = new SortTask(array, i, right, record_size, i/chunk_size);
            ff_send_out(st);
        }
        
        return nullptr; // EOS
    }
};

// Collector for sort phase and intermediate storage
class SortCollector : public ff_node {
private:
    std::vector<SortTask*> sorted_chunks;
    
public:
    SortCollector() {}
    
    void* svc(void* task) {
        SortTask* st = static_cast<SortTask*>(task);
        sorted_chunks.push_back(st);
        return GO_ON;
    }
    
    const std::vector<SortTask*>& get_sorted_chunks() const {
        return sorted_chunks;
    }
    
    void svc_end() {
        // Sort chunks by left index to ensure correct order
        std::sort(sorted_chunks.begin(), sorted_chunks.end(),
            [](const SortTask* a, const SortTask* b) { return a->left < b->left; });
    }
};

// Class for the merge phase using parallel_for
class MergeStage : public ff_node {
private:
    Record* array;
    size_t array_size;
    size_t record_size;
    size_t num_threads;
    std::vector<SortTask*> chunks;
    
public:
    MergeStage(Record* arr, size_t size, size_t rs, size_t nt)
        : array(arr), array_size(size), record_size(rs), num_threads(nt) {}
    
    void set_chunks(const std::vector<SortTask*>& sorted_chunks) {
        chunks = sorted_chunks;
    }
    
    void* svc(void* task) {
        // Log the start of merge phase
        std::cout << "Starting merge phase with " << chunks.size() << " chunks" << std::endl;
        
        // Use a binary tree merging pattern with multiple merge levels
        int merge_level = 0;
        
        while (chunks.size() > 1) {
            merge_level++;
            std::cout << "Merge level " << merge_level << ": merging " << chunks.size() << " chunks" << std::endl;
            
            // Calculate the number of merge operations at this level
            size_t num_merges = chunks.size() / 2;
            size_t remainder = chunks.size() % 2;
            
            // Create a parallel_for to handle the merge operations
            ParallelFor pf(num_threads);
            
            // Vector to hold the results of this merge level
            std::vector<SortTask*> next_level(num_merges + remainder);
            
            // Define the merge operation as a lambda
            auto merge_op = [&](const long i) {
                size_t left = chunks[i*2]->left;
                size_t mid = chunks[i*2]->right;
                size_t right = chunks[i*2+1]->right;
                
                // Allocate a temporary buffer for merging
                size_t size = right - left + 1;
                Record* tmp = reinterpret_cast<Record*>(new char[size * record_size]);
                
                // Perform the merge
                merge(array, left, mid, right, record_size, tmp);
                
                // Clean up
                delete[] reinterpret_cast<char*>(tmp);
                
                // Create a new task for the merged chunk
                next_level[i] = new SortTask(array, left, right, record_size);
            };
            
            // Execute the parallel merge operations
            pf.parallel_for(0, num_merges, 1, 1, merge_op);
            
            // If there's an odd number of chunks, pass the last one through
            if (remainder) {
                next_level[num_merges] = chunks[chunks.size() - 1];
            }
            
            // Clean up the merged chunks (except for the last one if passed through)
            for (size_t i = 0; i < chunks.size() - remainder; i++) {
                delete chunks[i];
            }
            
            // Update for next iteration
            chunks = next_level;
        }
        
        // Clean up the final chunk - it represents the entire sorted array
        if (!chunks.empty()) {
            delete chunks[0];
            chunks.clear();
        }
        
        return nullptr; // We're done
    }
};

// Pipeline implementation of merge sort
void pipeline_mergesort(Record* array, size_t array_size, size_t record_size, size_t num_threads) {
    // Create timers
    auto sort_start = std::chrono::high_resolution_clock::now();
    
    // Set up the sorting farm
    ff_farm sort_farm;
    
    // Create workers for the sort phase
    std::vector<ff_node*> sort_workers;
    for (size_t i = 0; i < num_threads; i++) {
        sort_workers.push_back(new SortWorker(record_size));
    }
    
    // Create emitter and collector
    SortEmitter* emitter = new SortEmitter(array, array_size, num_threads * 2, record_size);
    SortCollector* collector = new SortCollector();
    
    // Set up the farm
    sort_farm.add_emitter(emitter);
    sort_farm.add_workers(sort_workers);
    sort_farm.add_collector(collector);
    
    // Run the sort farm
    std::cout << "Starting sort phase with " << num_threads << " threads" << std::endl;
    if (sort_farm.run_and_wait_end() < 0) {
        std::cerr << "Error running sort farm" << std::endl;
        exit(1);
    }
    
    auto sort_end = std::chrono::high_resolution_clock::now();
    sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end - sort_start);
    std::cout << "Sort phase completed in " << sort_duration.count() << " ms" << std::endl;
    
    // Get the sorted chunks
    const std::vector<SortTask*>& sorted_chunks = collector->get_sorted_chunks();
    std::cout << "Sorted " << sorted_chunks.size() << " chunks" << std::endl;
    
    // Create the merge stage
    auto merge_start = std::chrono::high_resolution_clock::now();
    
    // Create and run the merge stage
    MergeStage merge_stage(array, array_size, record_size, num_threads);
    merge_stage.set_chunks(sorted_chunks);
    merge_stage.svc(nullptr);
    
    auto merge_end = std::chrono::high_resolution_clock::now();
    merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end - merge_start);
    std::cout << "Merge phase completed in " << merge_duration.count() << " ms" << std::endl;
}

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
    
    // Time variables
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (sequential) {
        // Run sequential mergesort
        mergesort_seq(array, 0, array_size - 1, record_size, tmp);
    } else {
        // Run parallel mergesort using pipeline approach
        pipeline_mergesort(array, array_size, record_size, num_threads);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Verify that the array is sorted
    bool sorted = verify_sorted(array, array_size, record_size);
    
    std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
              << "Total time: " << total_duration.count() << " ms\n";
              
    if (!sequential) {
        // Output timing information for the pipeline implementation
        std::cout << "  Sort phase: " << sort_duration.count() << " ms (" 
                  << (sort_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                  << "  Merge phase: " << merge_duration.count() << " ms ("
                  << (merge_duration.count() * 100.0 / total_duration.count()) << "%)\n";
    }
    
    // Clean up
    delete[] reinterpret_cast<char*>(array);
    if (sequential) {
        delete[] reinterpret_cast<char*>(tmp);
    }
    
    return sorted ? 0 : 1;
}
