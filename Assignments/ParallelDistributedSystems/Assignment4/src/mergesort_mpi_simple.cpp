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

using namespace std;

// Global variables for MPI communication
int mpi_rank, mpi_size;

// Record structure as per assignment
struct Record {
    unsigned long key;  // sorting value
    char* payload;      // pointer to payload data
    size_t payload_size; // size of payload
    
    Record() : key(0), payload(nullptr), payload_size(0) {}
    Record(unsigned long k, size_t ps) : key(k), payload_size(ps) {
        if (ps > 0) {
            payload = new char[ps];
            // Initialize payload with some pattern
            for (size_t i = 0; i < ps; i++) {
                payload[i] = static_cast<char>(i % 256);
            }
        } else {
            payload = nullptr;
        }
    }
    
    ~Record() {
        delete[] payload;
    }
    
    // Copy constructor
    Record(const Record& other) : key(other.key), payload_size(other.payload_size) {
        if (payload_size > 0) {
            payload = new char[payload_size];
            memcpy(payload, other.payload, payload_size);
        } else {
            payload = nullptr;
        }
    }
    
    // Assignment operator
    Record& operator=(const Record& other) {
        if (this != &other) {
            delete[] payload;
            key = other.key;
            payload_size = other.payload_size;
            if (payload_size > 0) {
                payload = new char[payload_size];
                memcpy(payload, other.payload, payload_size);
            } else {
                payload = nullptr;
            }
        }
        return *this;
    }
    
    bool operator<(const Record& other) const {
        return key < other.key;
    }
};

// Function to create record array with specific payload size
vector<Record> create_record_array(size_t n, size_t payload_size) {
    vector<Record> array;
    array.reserve(n);
    
    for (size_t i = 0; i < n; i++) {
        array.emplace_back(rand(), payload_size);
    }
    
    return array;
}

// Function to verify the sorted array
bool verify_sorted(const vector<Record>& array) {
    for (size_t i = 1; i < array.size(); i++) {
        if (array[i-1].key > array[i].key) {
            std::cerr << "Sorting failure at position " << (i-1) << " and " << i 
                      << ", keys: " << array[i-1].key << " > " << array[i].key << std::endl;
            return false;
        }
    }
    return true;
}

// Serialize records for MPI communication
vector<char> serialize_records(const vector<Record>& records) {
    if (records.empty()) return vector<char>();
    
    size_t payload_size = records[0].payload_size;
    size_t record_serialized_size = sizeof(unsigned long) + payload_size;
    vector<char> buffer(records.size() * record_serialized_size);
    
    for (size_t i = 0; i < records.size(); i++) {
        size_t offset = i * record_serialized_size;
        memcpy(buffer.data() + offset, &records[i].key, sizeof(unsigned long));
        if (payload_size > 0) {
            memcpy(buffer.data() + offset + sizeof(unsigned long), 
                   records[i].payload, payload_size);
        }
    }
    
    return buffer;
}

// Deserialize records from MPI communication
vector<Record> deserialize_records(const vector<char>& buffer, size_t payload_size) {
    if (buffer.empty()) return vector<Record>();
    
    size_t record_serialized_size = sizeof(unsigned long) + payload_size;
    size_t num_records = buffer.size() / record_serialized_size;
    vector<Record> records;
    records.reserve(num_records);
    
    for (size_t i = 0; i < num_records; i++) {
        size_t offset = i * record_serialized_size;
        unsigned long key;
        memcpy(&key, buffer.data() + offset, sizeof(unsigned long));
        
        Record rec(key, payload_size);
        if (payload_size > 0) {
            memcpy(rec.payload, buffer.data() + offset + sizeof(unsigned long), payload_size);
        }
        records.push_back(std::move(rec));
    }
    
    return records;
}

// Binary merge of two sorted arrays
vector<Record> merge_arrays(const vector<Record>& left, const vector<Record>& right) {
    vector<Record> result;
    result.reserve(left.size() + right.size());
    
    size_t i = 0, j = 0;
    
    while (i < left.size() && j < right.size()) {
        if (left[i].key <= right[j].key) {
            result.push_back(left[i]);
            i++;
        } else {
            result.push_back(right[j]);
            j++;
        }
    }
    
    // Copy remaining elements
    while (i < left.size()) {
        result.push_back(left[i]);
        i++;
    }
    
    while (j < right.size()) {
        result.push_back(right[j]);
        j++;
    }
    
    return result;
}

// MPI-based distributed merge sort
vector<Record> mpi_merge_sort(vector<Record> data, size_t payload_size, 
                             double& sort_time, double& merge_time, double& comm_time) {
    
    auto start_total = std::chrono::high_resolution_clock::now();
    auto start_sort = std::chrono::high_resolution_clock::now();
    
    // Calculate local data size for each process
    size_t total_elements = data.size();
    size_t elements_per_process = total_elements / mpi_size;
    size_t remaining_elements = total_elements % mpi_size;
    
    // Adjust for the last process
    size_t local_elements = elements_per_process;
    if (mpi_rank == mpi_size - 1) {
        local_elements += remaining_elements;
    }
    
    vector<Record> local_data;
    
    auto start_comm = std::chrono::high_resolution_clock::now();
    
    if (mpi_rank == 0) {
        // Root process: keep local portion and distribute the rest
        local_data.assign(data.begin(), data.begin() + local_elements);
        
        // Send data to other processes
        for (int i = 1; i < mpi_size; i++) {
            size_t start_idx = i * elements_per_process;
            size_t end_idx = start_idx + elements_per_process;
            if (i == mpi_size - 1) {
                end_idx += remaining_elements;
            }
            
            vector<Record> chunk(data.begin() + start_idx, data.begin() + end_idx);
            vector<char> serialized = serialize_records(chunk);
            
            int size = serialized.size();
            MPI_Send(&size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(serialized.data(), size, MPI_BYTE, i, 1, MPI_COMM_WORLD);
        }
    } else {
        // Non-root processes: receive data
        int size;
        MPI_Recv(&size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        vector<char> buffer(size);
        MPI_Recv(buffer.data(), size, MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data = deserialize_records(buffer, payload_size);
    }
    
    auto end_comm1 = std::chrono::high_resolution_clock::now();
    comm_time += std::chrono::duration<double>(end_comm1 - start_comm).count();
    
    // Local sort
    std::sort(local_data.begin(), local_data.end());
    
    auto end_sort = std::chrono::high_resolution_clock::now();
    sort_time = std::chrono::duration<double>(end_sort - start_sort).count();
    
    auto start_merge = std::chrono::high_resolution_clock::now();
    
    // Binary tree merge phase
    int step = 1;
    vector<Record> current_data = std::move(local_data);
    
    while (step < mpi_size) {
        start_comm = std::chrono::high_resolution_clock::now();
        
        if (mpi_rank % (2 * step) == 0) {
            // Receiver process
            int sender = mpi_rank + step;
            if (sender < mpi_size) {
                // Receive data
                int size;
                MPI_Recv(&size, 1, MPI_INT, sender, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                vector<char> buffer(size);
                MPI_Recv(buffer.data(), size, MPI_BYTE, sender, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                auto end_comm2 = std::chrono::high_resolution_clock::now();
                comm_time += std::chrono::duration<double>(end_comm2 - start_comm).count();
                
                // Deserialize and merge
                vector<Record> received_data = deserialize_records(buffer, payload_size);
                current_data = merge_arrays(current_data, received_data);
            }
        } else if (mpi_rank % (2 * step) == step) {
            // Sender process
            int receiver = mpi_rank - step;
            
            // Serialize and send data
            vector<char> serialized = serialize_records(current_data);
            int size = serialized.size();
            
            MPI_Send(&size, 1, MPI_INT, receiver, 0, MPI_COMM_WORLD);
            MPI_Send(serialized.data(), size, MPI_BYTE, receiver, 1, MPI_COMM_WORLD);
            
            auto end_comm3 = std::chrono::high_resolution_clock::now();
            comm_time += std::chrono::duration<double>(end_comm3 - start_comm).count();
            
            break; // This process is done
        }
        
        step *= 2;
    }
    
    auto end_merge = std::chrono::high_resolution_clock::now();
    merge_time = std::chrono::duration<double>(end_merge - start_merge).count();
    
    return current_data;
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
              << "  -h, --help          Display this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    
    // Default parameters
    size_t array_size = 100000;  // Default: 100K (smaller for testing)
    size_t record_payload = 8;    // Default: 8 bytes
    
    // Parse command line options
    static struct option long_options[] = {
        {"size",       required_argument, 0, 's'},
        {"record",     required_argument, 0, 'r'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "s:r:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                array_size = parse_size(optarg);
                break;
            case 'r':
                record_payload = atoi(optarg);
                break;
            case 'h':
                if (mpi_rank == 0) usage(argv[0]);
                MPI_Finalize();
                return 0;
            case '?':
                if (mpi_rank == 0) usage(argv[0]);
                MPI_Finalize();
                return 1;
            default:
                abort();
        }
    }
    
    // Initialize random number generator
    srand(time(nullptr) + mpi_rank); // Different seed for each process
    
    if (mpi_rank == 0) {
        std::cout << "MPI MergeSort Payload Scaling Analysis:\n"
                  << "  Array size: " << array_size << " elements\n"
                  << "  Record payload: " << record_payload << " bytes\n"
                  << "  Record total size: " << (sizeof(unsigned long) + record_payload) << " bytes\n"
                  << "  Total memory: " << (array_size * (sizeof(unsigned long) + record_payload)) / (1024*1024) << " MB\n"
                  << "  MPI processes: " << mpi_size << std::endl;
    }
    
    // Create data (only on rank 0)
    vector<Record> data;
    if (mpi_rank == 0) {
        data = create_record_array(array_size, record_payload);
    }
    
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Timing variables
    double sort_time = 0.0, merge_time = 0.0, comm_time = 0.0;
    
    // Perform distributed merge sort
    vector<Record> result = mpi_merge_sort(std::move(data), record_payload, 
                                          sort_time, merge_time, comm_time);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration<double>(end_time - start_time).count();
    
    // Collect timing statistics
    double max_sort_time, max_merge_time, max_comm_time, max_total_time;
    MPI_Reduce(&sort_time, &max_sort_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&merge_time, &max_merge_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&comm_time, &max_comm_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_time, &max_total_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    // Verify and print results (only root process)
    if (mpi_rank == 0) {
        bool sorted = verify_sorted(result);
        
        std::cout << "Sorting " << (sorted ? "successful" : "FAILED") << "\n"
                  << "Performance Results:\n"
                  << "  Total time: " << (max_total_time * 1000) << " ms\n"
                  << "  Sort time: " << (max_sort_time * 1000) << " ms (" 
                  << (max_sort_time * 100.0 / max_total_time) << "%)\n"
                  << "  Merge time: " << (max_merge_time * 1000) << " ms (" 
                  << (max_merge_time * 100.0 / max_total_time) << "%)\n"
                  << "  Communication time: " << (max_comm_time * 1000) << " ms (" 
                  << (max_comm_time * 100.0 / max_total_time) << "%)\n"
                  << "  Communication overhead: " << (max_comm_time * 100.0 / max_total_time) << "%\n";
        
        // Calculate data transfer metrics
        size_t total_data_size = array_size * (sizeof(unsigned long) + record_payload);
        double data_transferred_mb = (total_data_size * (mpi_size - 1)) / (1024.0 * 1024.0);
        
        std::cout << "  Data transfer: " << data_transferred_mb << " MB transferred\n"
                  << "  Effective bandwidth: " 
                  << (data_transferred_mb / max_comm_time) << " MB/s\n";
        
        // Show payload size impact
        double comm_per_byte = max_comm_time / total_data_size;
        std::cout << "\nPayload Size Impact Analysis:\n"
                  << "  Communication time per byte: " << (comm_per_byte * 1000000) << " microseconds/byte\n"
                  << "  Expected: Larger payloads increase communication time linearly\n" << std::endl;
    }
    
    MPI_Finalize();
    return 0;
}
