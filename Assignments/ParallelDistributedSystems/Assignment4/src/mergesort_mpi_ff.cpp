#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <chrono>
#include <getopt.h>
#include <mpi.h>
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <ff/pipeline.hpp>

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

// Function to sort a local array using FastFlow
void sort_local_array(Record* array, size_t array_size, size_t record_size, size_t num_threads) {
    collector_merge_time = std::chrono::milliseconds(0);
    
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
    
    // Run the farm
    if (sort_farm.run_and_wait_end() < 0) {
        std::cerr << "Error running sort farm" << std::endl;
        exit(1);
    }
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
              << "  -h, --help          Display this help message\n"
              << std::endl;
}

// Function to distribute array chunks to MPI processes
void distribute_data(Record* full_array, size_t array_size, size_t record_size, int rank, int size) {
    // Calculate chunk sizes for each process
    std::vector<size_t> chunk_sizes(size);
    size_t base_size = array_size / size;
    for (int i = 0; i < size; i++) {
        chunk_sizes[i] = base_size + (static_cast<size_t>(i) < array_size % static_cast<size_t>(size) ? 1 : 0);
    }
    
    // Send chunks to other processes
    for (int i = 1; i < size; i++) {
        size_t offset = 0;
        for (int j = 0; j < i; j++) {
            offset += chunk_sizes[j];
        }
        
        MPI_Send(reinterpret_cast<char*>(full_array) + offset * record_size, 
                 chunk_sizes[i] * record_size, MPI_BYTE, i, 0, MPI_COMM_WORLD);
    }
}

// Function to perform global merging with computation-communication overlap
Record* global_merge(Record* local_array, size_t local_size, size_t record_size, int rank, int size) {
    // MPI datatype for Record
    MPI_Datatype mpi_record_type;
    MPI_Type_contiguous(record_size, MPI_BYTE, &mpi_record_type);
    MPI_Type_commit(&mpi_record_type);
    
    // Implementation of hierarchical merge with overlap
    // For simplicity, using a gather-to-root approach here
    
    if (rank == 0) {
        // Root process: receive and merge all chunks
        size_t total_size = local_size;
        std::vector<size_t> chunk_sizes(size);
        chunk_sizes[0] = local_size;
        
        // Get chunk sizes from all processes
        for (int i = 1; i < size; i++) {
            MPI_Status status;
            MPI_Recv(&chunk_sizes[i], 1, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD, &status);
            total_size += chunk_sizes[i];
        }
        
        // Allocate memory for the full merged array
        Record* merged_array = reinterpret_cast<Record*>(new char[total_size * record_size]);
        
        // Copy local array to the beginning of merged_array
        memcpy(reinterpret_cast<char*>(merged_array), 
               reinterpret_cast<char*>(local_array), 
               local_size * record_size);
        
        // Temporary buffer for merging
        Record* tmp = reinterpret_cast<Record*>(new char[total_size * record_size]);
        
        // Receive sorted chunks from other processes and merge
        size_t current_size = local_size;
        size_t offset = 0;
        
        for (int i = 1; i < size; i++) {
            offset += chunk_sizes[i-1];
            
            // Receive sorted chunk from process i
            MPI_Status status;
            MPI_Recv(reinterpret_cast<char*>(merged_array) + offset * record_size, 
                     chunk_sizes[i] * record_size, MPI_BYTE, i, 1, MPI_COMM_WORLD, &status);
            
            // Merge the received chunk with the already merged data
            merge(merged_array, 0, current_size - 1, current_size + chunk_sizes[i] - 1, record_size, tmp);
            current_size += chunk_sizes[i];
        }
        
        // Clean up
        delete[] reinterpret_cast<char*>(tmp);
        MPI_Type_free(&mpi_record_type);
        
        return merged_array;
    } else {
        // Non-root process: send sorted chunk to root
        MPI_Send(&local_size, 1, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
        MPI_Send(local_array, local_size, mpi_record_type, 0, 1, MPI_COMM_WORLD);
        
        // Clean up
        MPI_Type_free(&mpi_record_type);
        
        return nullptr; // Non-root processes don't keep the final result
    }
}

int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Default parameters
    size_t array_size = 1000000;  // Default: 1M
    size_t record_payload = 8;    // Default: 8 bytes
    size_t num_threads = 4;       // Default: 4 threads
    
    // Parse command line options
    static struct option long_options[] = {
        {"size",       required_argument, 0, 's'},
        {"record",     required_argument, 0, 'r'},
        {"threads",    required_argument, 0, 't'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "s:r:t:h", long_options, &option_index)) != -1) {
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
            case 'h':
                if (rank == 0) usage(argv[0]);
                MPI_Finalize();
                return 0;
            case '?':
                if (rank == 0) usage(argv[0]);
                MPI_Finalize();
                return 1;
            default:
                abort();
        }
    }
    
    // Initialize random number generator
    srand(time(nullptr) + rank); // Different seed for each process
    
    // Actual record size including the payload
    size_t record_size = sizeof(unsigned long) + record_payload;
    
    if (rank == 0) {
        std::cout << "MPI + FastFlow MergeSort Configuration:\n"
                  << "  Array size: " << array_size << " elements\n"
                  << "  Record payload: " << record_payload << " bytes\n"
                  << "  Record total size: " << record_size << " bytes\n"
                  << "  Total memory: " << (array_size * record_size) / (1024*1024) << " MB\n"
                  << "  MPI processes: " << size << "\n"
                  << "  FastFlow threads per process: " << num_threads << std::endl;
    }
    
    // Calculate local chunk size for this process
    size_t local_size = array_size / size;
    if (static_cast<size_t>(rank) < array_size % static_cast<size_t>(size)) local_size++;
    
    // Allocate local array
    Record* local_array = nullptr;
    Record* full_array = nullptr;
    
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    auto distribution_end_time = start_time;
    auto local_sort_end_time = start_time;
    auto global_merge_end_time = start_time;
    
    // Root process initializes and distributes data
    if (rank == 0) {
        // Generate the full array
        full_array = create_record_array(array_size, record_payload);
        
        // Keep local portion
        local_array = full_array;
        
        // Distribute to other processes
        distribute_data(full_array, array_size, record_size, rank, size);
        
        distribution_end_time = std::chrono::high_resolution_clock::now();
    } else {
        // Allocate and receive local portion
        local_array = reinterpret_cast<Record*>(new char[local_size * record_size]);
        
        // Receive from root
        MPI_Status status;
        MPI_Recv(local_array, local_size * record_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
        
        distribution_end_time = std::chrono::high_resolution_clock::now();
    }
    
    // Local sort using FastFlow
    sort_local_array(local_array, local_size, record_size, num_threads);
    
    local_sort_end_time = std::chrono::high_resolution_clock::now();
    
    // Global merge with computation-communication overlap
    Record* result_array = global_merge(local_array, local_size, record_size, rank, size);
    
    global_merge_end_time = std::chrono::high_resolution_clock::now();
    
    // Verify and print results (only root process)
    if (rank == 0) {
        bool sorted = verify_sorted(result_array, array_size, record_size);
        
        auto distribution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            distribution_end_time - start_time);
        auto local_sort_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            local_sort_end_time - distribution_end_time);
        auto global_merge_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            global_merge_end_time - local_sort_end_time);
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            global_merge_end_time - start_time);
        
        std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
                  << "Total time: " << total_time.count() << " ms\n"
                  << "  Data distribution: " << distribution_time.count() << " ms (" 
                  << (distribution_time.count() * 100.0 / total_time.count()) << "%)\n"
                  << "  Local sort: " << local_sort_time.count() << " ms (" 
                  << (local_sort_time.count() * 100.0 / total_time.count()) << "%)\n"
                  << "    - Sort phase: " << (local_sort_time.count() - collector_merge_time.count()) << " ms\n"
                  << "    - Merge phase: " << collector_merge_time.count() << " ms\n"
                  << "  Global merge: " << global_merge_time.count() << " ms (" 
                  << (global_merge_time.count() * 100.0 / total_time.count()) << "%)\n";
        
        // Clean up
        delete[] reinterpret_cast<char*>(result_array);
    }
    
    // Clean up local array (non-root processes only)
    if (rank != 0 || result_array != local_array) {
        delete[] reinterpret_cast<char*>(local_array);
    }
    
    MPI_Finalize();
    return 0;
}
