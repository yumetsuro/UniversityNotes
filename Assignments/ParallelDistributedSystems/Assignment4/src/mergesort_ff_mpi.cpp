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
#include "mpi.h"

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

// Fix the merging of arrays received from other ranks
void merge_arrays(Record* result, Record* array1, size_t size1, Record* array2, size_t size2, char* result_payload) {
    size_t i = 0, j = 0, k = 0;
    
    // Merge the two sorted arrays directly into result
    while (i < size1 && j < size2) {
        if (array1[i].key <= array2[j].key) {
            result[k].key = array1[i].key;
            // Copy payload data to the correct position in result_payload
            memcpy(result_payload + k * g_payload_size, 
                   array1[i].rpayload, 
                   g_payload_size);
            result[k].rpayload = result_payload + k * g_payload_size;
            i++;
        } else {
            result[k].key = array2[j].key;
            // Copy payload data to the correct position in result_payload
            memcpy(result_payload + k * g_payload_size, 
                   array2[j].rpayload, 
                   g_payload_size);
            result[k].rpayload = result_payload + k * g_payload_size;
            j++;
        }
        k++;
    }
    
    // Copy remaining elements from array1
    while (i < size1) {
        result[k].key = array1[i].key;
        memcpy(result_payload + k * g_payload_size, 
               array1[i].rpayload, 
               g_payload_size);
        result[k].rpayload = result_payload + k * g_payload_size;
        i++;
        k++;
    }
    
    // Copy remaining elements from array2
    while (j < size2) {
        result[k].key = array2[j].key;
        memcpy(result_payload + k * g_payload_size, 
               array2[j].rpayload, 
               g_payload_size);
        result[k].rpayload = result_payload + k * g_payload_size;
        j++;
        k++;
    }
}

int main(int argc, char* argv[]) {
    // Default parameters
    size_t array_size = 1000000;  // Default: 1M
    size_t num_threads = 4;       // Default: 4 threads
    bool sequential = false;      // Default: parallel execution
   
    //Initialize MPI
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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
    
    // Initialize random number generator with different seeds per rank
    srand(time(nullptr) + rank);
    
    // Record size includes the actual payload size being used
    size_t record_size = Record::get_record_size();
    size_t total_payload_size = g_payload_size * array_size;

    // Only rank 0 prints configuration
    if (rank == 0) {
        std::cout << "MergeSort Configuration:\n"
                << "  Array size: " << array_size << " elements\n"
                << "  Record payload: " << g_payload_size << " bytes\n"
                << "  Record total size: " << record_size << " bytes\n"
                << "  Total payload size: " << total_payload_size / (1024*1024) << " MB\n"
                << "  Total memory: " << (array_size * record_size) / (1024*1024) << " MB\n"
                << "  Mode: " << (sequential ? "Sequential" : "Parallel with " + std::to_string(num_threads) + " threads")
                << "  MPI: " << size << " processes"
                << std::endl;
    }

    // Prepare variables for all ranks
    Record* array = nullptr;
    char* payload_block = nullptr;
    
    // Calculate per-node array size
    size_t array_size_per_node = array_size / size;
    size_t remainder = array_size % size;
    size_t local_array_size = array_size_per_node + (rank < remainder ? 1 : 0);
    size_t local_offset = rank * array_size_per_node + std::min(rank, (int)remainder);

    // Only rank 0 creates the full array
    if (rank == 0) {
        // Create the array of records with random keys
        auto result = create_record_array_with_payload(array_size);
        array = result.first;
        payload_block = result.second;
    }

    // Local array for each rank
    Record* local_array = new Record[local_array_size];
    char* local_payload_block = new char[local_array_size * g_payload_size];
    
    // Rank 0 distributes data
    if (rank == 0) {
        // Keep my portion
        for (size_t i = 0; i < local_array_size; i++) {
            local_array[i].key = array[i].key;
            // Copy payload data to local payload block
            memcpy(local_payload_block + i * g_payload_size, 
                  array[i].rpayload, 
                  g_payload_size);
            local_array[i].rpayload = local_payload_block + i * g_payload_size;
        }
        
        // Send to other processes
        size_t current_offset = local_array_size;
        for (int i = 1; i < size; i++) {
            size_t target_size = array_size_per_node + (i < remainder ? 1 : 0);
            
            // Create a buffer for keys and send it
            unsigned long* key_buffer = new unsigned long[target_size];
            for (size_t j = 0; j < target_size; j++) {
                key_buffer[j] = array[current_offset + j].key;
            }
            MPI_Send(key_buffer, target_size, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD);
            delete[] key_buffer;
            
            // Create a buffer for payload data and send it
            char* payload_buffer = new char[target_size * g_payload_size];
            for (size_t j = 0; j < target_size; j++) {
                memcpy(payload_buffer + j * g_payload_size,
                      array[current_offset + j].rpayload,
                      g_payload_size);
            }
            MPI_Send(payload_buffer, target_size * g_payload_size, MPI_BYTE, i, 1, MPI_COMM_WORLD);
            delete[] payload_buffer;
            
            current_offset += target_size;
        }
    } else {
        // Receive keys from rank 0
        unsigned long* key_buffer = new unsigned long[local_array_size];
        MPI_Recv(key_buffer, local_array_size, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Receive payload data from rank 0
        MPI_Recv(local_payload_block, local_array_size * g_payload_size, MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Set up local records
        for (size_t i = 0; i < local_array_size; i++) {
            local_array[i].key = key_buffer[i];
            local_array[i].rpayload = local_payload_block + i * g_payload_size;
        }
        delete[] key_buffer;
    }

    // Temporary array for merging in sequential version
    Record* tmp = nullptr;
    if (sequential) {
        tmp = new Record[local_array_size];
    }
    
    // Time variables for different phases
    auto start_time = std::chrono::high_resolution_clock::now();
    auto sort_start_time = start_time;
    auto sort_end_time = start_time;
    auto merge_start_time = start_time;
    auto merge_end_time = start_time;
    auto mpi_merge_start_time = start_time;
    auto mpi_merge_end_time = start_time;
    
    // Memory tracking variables
    size_t total_received_bytes = 0;
    size_t total_received_records = 0;
    
    // Each rank sorts its local data
    if (sequential) {
        // Run sequential mergesort
        mergesort(local_array, 0, local_array_size - 1, tmp);
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
        SortEmitter* sort_emitter = new SortEmitter(local_array, local_array_size, num_threads);
        SortCollector* sort_collector = new SortCollector(local_array, local_array_size);
        sort_farm.add_emitter(sort_emitter);
        sort_farm.add_workers(sort_workers);
        sort_farm.add_collector(sort_collector);
        
        // Run the sort farm
        if (sort_farm.run_and_wait_end() < 0) {
            std::cerr << "Error running sort farm" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
        
        sort_end_time = std::chrono::high_resolution_clock::now();
        
        // Phase 2: Tree-like parallel merge using multiple levels
        merge_start_time = std::chrono::high_resolution_clock::now();
        
        // Initialize ranges for merging
        std::vector<std::pair<size_t, size_t>> current_ranges;
        size_t chunk_size = local_array_size / num_threads;
        if (chunk_size == 0) chunk_size = 1;
        
        for (size_t i = 0; i < local_array_size; i += chunk_size) {
            size_t right = std::min(i + chunk_size - 1, local_array_size - 1);
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
                    
                    merge_tasks.push_back(new MergeTask(local_array, left, mid, right));
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
                    MPI_Abort(MPI_COMM_WORLD, 1);
                    return 1;
                }
            }
            
            current_ranges = next_ranges;
        }
        
        merge_end_time = std::chrono::high_resolution_clock::now();
    }

    // Tree-like merging between MPI ranks - higher ranks send to lower ranks
    mpi_merge_start_time = std::chrono::high_resolution_clock::now();
    int step = 1;
    while (step < size) {
        if (rank % (2 * step) == 0) {
            // Receiver process
            int sender = rank + step;
            if (sender < size) {
                // Receive size first
                size_t received_size;
                MPI_Recv(&received_size, 1, MPI_UNSIGNED_LONG, sender, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // Track received data
                total_received_records += received_size;
                total_received_bytes += received_size * (sizeof(unsigned long) + g_payload_size);
                
                // Receive keys
                unsigned long* received_keys = new unsigned long[received_size];
                MPI_Recv(received_keys, received_size, MPI_UNSIGNED_LONG, sender, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // Receive payload
                char* received_payload = new char[received_size * g_payload_size];
                MPI_Recv(received_payload, received_size * g_payload_size, MPI_BYTE, sender, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // Create received Records
                Record* received_records = new Record[received_size];
                for (size_t i = 0; i < received_size; i++) {
                    received_records[i].key = received_keys[i];
                    received_records[i].rpayload = received_payload + i * g_payload_size;
                }
                delete[] received_keys;
                
                // Create a properly organized merged array
                Record* merged_array = new Record[local_array_size + received_size];
                char* merged_payload = new char[(local_array_size + received_size) * g_payload_size];
                
                // Set up temporary array for merge operation
                Record* merge_tmp = new Record[local_array_size + received_size];
                
                // Create separate arrays for the local and received data
                Record* local_copy = new Record[local_array_size];
                Record* received_copy = new Record[received_size];
                
                // Copy local array data
                for (size_t i = 0; i < local_array_size; i++) {
                    local_copy[i].key = local_array[i].key;
                    memcpy(merged_payload + i * g_payload_size, 
                          local_array[i].rpayload, 
                          g_payload_size);
                    local_copy[i].rpayload = merged_payload + i * g_payload_size;
                }
                
                // Copy received array data
                for (size_t i = 0; i < received_size; i++) {
                    received_copy[i].key = received_records[i].key;
                    memcpy(merged_payload + (local_array_size + i) * g_payload_size, 
                          received_records[i].rpayload, 
                          g_payload_size);
                    received_copy[i].rpayload = merged_payload + (local_array_size + i) * g_payload_size;
                }
                
                // Merge the arrays using our new dedicated merge function
                merge_arrays(merged_array, local_copy, local_array_size, received_copy, received_size, merged_payload);
                
                // Clean up temporary arrays
                delete[] local_copy;
                delete[] received_copy;
                delete[] local_array;
                delete[] local_payload_block;
                delete[] received_records;
                delete[] received_payload;
                delete[] merge_tmp;
                
                // Update local arrays
                local_array = merged_array;
                local_payload_block = merged_payload;
                local_array_size += received_size;
            }
        } else if (rank % step == 0) {
            // Sender process
            int receiver = rank - step;
            if (receiver >= 0) {
                // Send size first
                MPI_Send(&local_array_size, 1, MPI_UNSIGNED_LONG, receiver, 2, MPI_COMM_WORLD);
                
                // Send keys
                unsigned long* key_buffer = new unsigned long[local_array_size];
                for (size_t i = 0; i < local_array_size; i++) {
                    key_buffer[i] = local_array[i].key;
                }
                MPI_Send(key_buffer, local_array_size, MPI_UNSIGNED_LONG, receiver, 3, MPI_COMM_WORLD);
                delete[] key_buffer;
                
                // Send payload
                MPI_Send(local_payload_block, local_array_size * g_payload_size, MPI_BYTE, receiver, 4, MPI_COMM_WORLD);
                
                // Clean up as this process is done
                delete[] local_array;
                delete[] local_payload_block;
                
                break; // This process is done
            }
        }
        step *= 2;
    }
    mpi_merge_end_time = std::chrono::high_resolution_clock::now();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate durations
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end_time - sort_start_time);
    auto merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end_time - merge_start_time);
    auto mpi_merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mpi_merge_end_time - mpi_merge_start_time);
    auto overhead_duration = total_duration - sort_duration - merge_duration - mpi_merge_duration;
    
    // Print per-rank timing and memory information
    for (int i = 0; i < size; i++) {
        if (rank == i) {
            std::cout << "\n";
            std::cout << "Rank " << rank << " Performance:\n"
                      << "  Local array size: " << local_array_size << " elements\n"
                      << "  Local memory: " << (local_array_size * (sizeof(unsigned long) + g_payload_size)) / 1024 << " KB\n"
                      << "  Received records: " << total_received_records << " elements\n"
                      << "  Received data: " << total_received_bytes / 1024 << " KB\n"
                      << "  Total processing time: " << total_duration.count() << " ms\n"
                      << "  Local sort time: " << sort_duration.count() << " ms\n"
                      << "  Local merge time: " << merge_duration.count() << " ms\n"
                      << "  MPI merge time: " << mpi_merge_duration.count() << " ms\n"
                      << "  Other overhead: " << overhead_duration.count() << " ms\n"
                      << std::endl;
            std::cout.flush();
        }
        MPI_Barrier(MPI_COMM_WORLD);  // Ensure ordered output
    }
    
    // Only rank 0 verifies the final sorted array and prints summary
    if (rank == 0) {
        std::cout << "=== Overall Performance Summary ===\n";
        
        // Verify that the array is sorted
        bool sorted = true;
        for (size_t i = 1; i < local_array_size; i++) {
            if (local_array[i-1].key > local_array[i].key) {
                std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                          << ", keys: " << local_array[i-1].key << " > " << local_array[i].key << std::endl;
                sorted = false;
                break;
            }
        }
        
        std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
                  << "Total processing time (rank 0): " << total_duration.count() << " ms\n";
                  
        if (!sequential) {
            std::cout << "Phase breakdown (rank 0):\n"
                      << "  Sort phase: " << sort_duration.count() << " ms (" 
                      << (sort_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                      << "  Local merge phase: " << merge_duration.count() << " ms ("
                      << (merge_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                      << "  MPI merge phase: " << mpi_merge_duration.count() << " ms ("
                      << (mpi_merge_duration.count() * 100.0 / total_duration.count()) << "%)\n"
                      << "  Other overhead: " << overhead_duration.count() << " ms ("
                      << (overhead_duration.count() * 100.0 / total_duration.count()) << "%)\n";
        }
    }
    
    // Clean up - only for rank 0, other ranks cleaned up earlier
    if (rank == 0) {
        if (array != local_array) {
            delete[] array;
            delete[] payload_block;
        }
        delete[] local_array;
        delete[] local_payload_block;
        if (sequential) {
            delete[] tmp;
        }
    }
    
    MPI_Finalize();
    return 0;
}
