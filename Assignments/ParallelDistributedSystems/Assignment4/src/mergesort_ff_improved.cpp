// Improved implementation of parallel mergesort using FastFlow
// This version uses a pipeline of farms for better parallelization
// of both the sorting and merging phases

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
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <tuple>

using namespace ff;
using namespace std;

// Global timing variables definition
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
        
        merge(mt->array, mt->left, mt->mid, mt->right, record_size, tmp);
        
        delete[] reinterpret_cast<char*>(tmp);
        
        return mt;  // Return the task for potential further processing
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

// Collector for the sorting phase and emitter for merging phase
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
        
        // Binary tree merging approach
        while (sorted_chunks.size() > 1) {
            std::vector<SortTask*> next_level;
            
            // Merge pairs of chunks
            for (size_t i = 0; i < sorted_chunks.size(); i += 2) {
                if (i + 1 < sorted_chunks.size()) {
                    // We have a pair to merge
                    size_t left = sorted_chunks[i]->left;
                    size_t mid = sorted_chunks[i]->right;
                    size_t right = sorted_chunks[i+1]->right;
                    
                    // Create a temporary buffer
                    size_t size = right - left + 1;
                    Record* tmp = reinterpret_cast<Record*>(new char[size * record_size]);
                    
                    // Merge the pair
                    merge(array, left, mid, right, record_size, tmp);
                    
                    // Clean up
                    delete[] reinterpret_cast<char*>(tmp);
                    
                    // Create a new task for the merged chunk
                    SortTask* merged = new SortTask(array, left, right, record_size);
                    next_level.push_back(merged);
                    
                    // Clean up the original tasks
                    delete sorted_chunks[i];
                    delete sorted_chunks[i+1];
                } else {
                    // Odd number of chunks, just pass the last one through
                    next_level.push_back(sorted_chunks[i]);
                }
            }
            
            // Update for next iteration
            sorted_chunks = next_level;
        }
        
        // Clean up the final task
        if (!sorted_chunks.empty()) {
            delete sorted_chunks[0];
            sorted_chunks.clear();
        }
    }
};

// Parallel merge using a farm
void parallel_merge_farm(Record* array, size_t array_size, size_t record_size, size_t num_threads, 
                         const std::vector<SortTask*>& sorted_chunks) {
    
    // Create a farm for merging
    ff_farm merge_farm;
    
    // Create workers for merging
    std::vector<ff_node*> merge_workers;
    for (size_t i = 0; i < num_threads; i++) {
        merge_workers.push_back(new MergeWorker(record_size));
    }
    
    // We need a custom emitter and collector for the merge farm
    class MergeEmitter : public ff_node {
    private:
        Record* array;
        size_t record_size;
        std::vector<SortTask*> chunks;
        std::vector<std::tuple<size_t, size_t, size_t>> merge_ranges; // (left, mid, right)
        size_t current_idx;
        
    public:
        MergeEmitter(Record* arr, size_t rs, const std::vector<SortTask*>& ch)
            : array(arr), record_size(rs), chunks(ch), current_idx(0) {
            
            // Create a binary tree of merge tasks
            size_t n = chunks.size();
            for (size_t step = 1; step < n; step *= 2) {
                for (size_t i = 0; i < n; i += 2 * step) {
                    if (i + step < n) {
                        // We have a pair to merge
                        size_t left = chunks[i]->left;
                        size_t mid = chunks[i + step - 1]->right;
                        size_t right = (i + 2 * step - 1 < n) ? 
                                       chunks[i + 2 * step - 1]->right : 
                                       chunks[n - 1]->right;
                        
                        merge_ranges.push_back(std::make_tuple(left, mid, right));
                    }
                }
            }
        }
        
        void* svc(void* task) {
            if (current_idx >= merge_ranges.size()) {
                return nullptr; // EOS
            }
            
            auto range = merge_ranges[current_idx++];
            
            // Create a merge task
            MergeTask* mt = new MergeTask(
                array, 
                std::get<0>(range), // left
                std::get<1>(range), // mid
                std::get<2>(range), // right
                record_size
            );
            
            return mt;
        }
    };
    
    class MergeCollector : public ff_node {
    public:
        void* svc(void* task) {
            MergeTask* mt = static_cast<MergeTask*>(task);
            delete mt;
            return GO_ON;
        }
    };
    
    // Set up the merge farm
    MergeEmitter* emitter = new MergeEmitter(array, record_size, sorted_chunks);
    MergeCollector* collector = new MergeCollector();
    
    merge_farm.add_emitter(emitter);
    merge_farm.add_workers(merge_workers);
    merge_farm.add_collector(collector);
    
    // Run the merge farm
    if (merge_farm.run_and_wait_end() < 0) {
        std::cerr << "Error running merge farm" << std::endl;
        exit(1);
    }
}

// Two-stage merge algorithm
void two_stage_merge(Record* array, size_t array_size, size_t record_size, size_t num_threads) {
    auto sort_start = std::chrono::high_resolution_clock::now();
    std::cout << "Starting parallel sort phase with " << num_threads << " threads..." << std::endl;
    
    // First, sort chunks in parallel
    ff_farm sort_farm;
    
    // Create workers for the sorting phase
    std::vector<ff_node*> sort_workers;
    for (size_t i = 0; i < num_threads; i++) {
        sort_workers.push_back(new SortWorker(record_size));
    }
    
    // Calculate optimal chunk size - aim for 2x num_threads chunks for better parallelism
    size_t num_chunks = num_threads * 2;
    size_t chunk_size = array_size / num_chunks;
    if (chunk_size == 0) {
        chunk_size = 1;
        num_chunks = array_size;
    }
    
    std::cout << "Created " << num_chunks << " chunks of size ~" << chunk_size << " elements each" << std::endl;
    
    // Collect sorted chunks
    std::vector<SortTask*> sorted_chunks;
    
    // Process and sort each chunk
    for (size_t i = 0; i < array_size; i += chunk_size) {
        size_t right = std::min(i + chunk_size - 1, array_size - 1);
        
        // Create a task
        SortTask task(array, i, right, record_size);
        
        // Sort the chunk directly
        SortWorker worker(record_size);
        SortTask* result = static_cast<SortTask*>(worker.svc(&task));
        
        // Store the sorted chunk
        sorted_chunks.push_back(new SortTask(*result));
    }
    
    auto sort_end = std::chrono::high_resolution_clock::now();
    auto sort_time = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end - sort_start);
    std::cout << "Sort phase completed in " << sort_time.count() << " ms" << std::endl;
    std::cout << "Starting parallel merge phase..." << std::endl;
    
    auto merge_start = std::chrono::high_resolution_clock::now();
    
    // Now, merge all chunks in parallel using a different approach
    // We'll use a binary tree merging pattern
    int merge_level = 0;
    
    while (sorted_chunks.size() > 1) {
        merge_level++;
        std::cout << "Merge level " << merge_level << ": merging " << sorted_chunks.size() << " chunks" << std::endl;
        
        std::vector<SortTask*> next_level;
        std::vector<std::thread> merge_threads;
        
        // Process pairs of chunks in parallel
        for (size_t i = 0; i < sorted_chunks.size(); i += 2) {
            if (i + 1 < sorted_chunks.size()) {
                // We have a pair to merge
                size_t left = sorted_chunks[i]->left;
                size_t mid = sorted_chunks[i]->right;
                size_t right = sorted_chunks[i+1]->right;
                
                // Launch a thread to merge this pair
                merge_threads.push_back(std::thread([=, &next_level, &array, &record_size]() {
                    // Create a temporary buffer
                    size_t size = right - left + 1;
                    Record* tmp = reinterpret_cast<Record*>(new char[size * record_size]);
                    
                    // Merge the pair
                    merge(array, left, mid, right, record_size, tmp);
                    
                    // Clean up
                    delete[] reinterpret_cast<char*>(tmp);
                    
                    // Create a new task for the merged chunk (thread-safe push_back needed)
                    SortTask* merged = new SortTask(array, left, right, record_size);
                    
                    // Use a mutex to protect access to next_level
                    static std::mutex mtx;
                    std::lock_guard<std::mutex> lock(mtx);
                    next_level.push_back(merged);
                }));
            } else {
                // Odd number of chunks, just pass the last one through
                next_level.push_back(sorted_chunks[i]);
            }
        }
        
        // Wait for all merge operations to complete
        for (auto& t : merge_threads) {
            t.join();
        }
        
        // Clean up the original tasks (except for those passed through)
        for (size_t i = 0; i < sorted_chunks.size() - (sorted_chunks.size() % 2); i++) {
            delete sorted_chunks[i];
        }
        
        // Sort the next level by left index
        std::sort(next_level.begin(), next_level.end(),
            [](const SortTask* a, const SortTask* b) { return a->left < b->left; });
        
        // Update for next iteration
        sorted_chunks = next_level;
    }
    
    // Clean up the final task
    if (!sorted_chunks.empty()) {
        delete sorted_chunks[0];
    }
    
    auto merge_end = std::chrono::high_resolution_clock::now();
    auto merge_time = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end - merge_start);
    std::cout << "Merge phase completed in " << merge_time.count() << " ms" << std::endl;
    
    // Update global timing variables
    sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end - sort_start);
    merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end - merge_start);
}

// Global variables for timing
extern std::chrono::milliseconds sort_duration;
extern std::chrono::milliseconds merge_duration;

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
    auto sort_start_time = start_time;
    auto sort_end_time = start_time;
    auto merge_start_time = start_time;
    auto merge_end_time = start_time;
    
    if (sequential) {
        // Run sequential mergesort
        mergesort_seq(array, 0, array_size - 1, record_size, tmp);
    } else {
        // Run parallel mergesort using improved two-stage approach
        two_stage_merge(array, array_size, record_size, num_threads);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Verify that the array is sorted
    bool sorted = verify_sorted(array, array_size, record_size);
    
    std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
              << "Total time: " << total_duration.count() << " ms\n";
              
    if (!sequential) {
        // Output timing information for improved implementation
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
