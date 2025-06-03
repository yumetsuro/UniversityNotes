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
#include <thread>
#include "mpi.h"

int main(int argc, char* argv[]) {
    // Default parameters
    size_t array_size = 1000000;  
    size_t num_threads = 4;       
    bool sequential = false;      
   
    //Initialize MPI
    MPI_Init(&argc, &argv);
    int rank, num_nodes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_nodes);

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
    
    srand(time(nullptr) + rank);
    
    size_t record_size = Record::get_record_size();
    size_t total_payload_size = g_payload_size * array_size;
    size_t total_memory = (array_size * record_size) + total_payload_size;

    // Only rank 0 prints configuration
    if (rank == 0 && DEBUG) {
        std::cout << "MergeSort Configuration:\n"
                << "  Array size: " << array_size << " elements\n"
                << "  Record payload: " << g_payload_size << " bytes\n"
                << "  Record struct size: " << record_size << " bytes (key + pointer)\n"
                << "  Total payload size: " << total_payload_size / (1024*1024) << " MB\n"
                << "  Total memory: " << total_memory / (1024*1024) << " MB (structs + payload)\n"
                << "  Mode: " << (sequential ? "Sequential" : "Parallel with " + std::to_string(num_threads) + " threads")
                << "  MPI: " << num_nodes << " processes"
                << std::endl;
    }

    Record* array = nullptr;
    char* payload_block = nullptr;
    
    // Calculate per-node array size
    size_t array_size_per_node = array_size / num_nodes;
    int remainder = array_size % num_nodes;
    size_t local_array_size = array_size_per_node + (rank < remainder ? 1 : 0);
    
    // ----------- Data Distribution Phase -----------
    // Rank 0 creates the full array and distributes it to other ranks
    
    // Only rank 0 creates the full array
    if (rank == 0) {
        auto result = create_record_array(array_size);
        array = result.first;
        payload_block = result.second;
    }

    // Local array for each rank
    Record* local_array = new Record[local_array_size];
    char* local_payload_block = new char[local_array_size * g_payload_size];
    

    // ----------- Enhanced Data Distribution Phase with Non-blocking MPI -----------
    // Optimized data distribution using non-blocking sends and computation overlap
    
    // Rank 0 distributes data using non-blocking communications
    if (rank == 0) {
        
        // Rank 0 prepares its local array first
        for (size_t i = 0; i < local_array_size; i++) {
            local_array[i].key = array[i].key;
            memcpy(local_payload_block + i * g_payload_size, 
                  array[i].rpayload, 
                  g_payload_size);
            local_array[i].rpayload = local_payload_block + i * g_payload_size;
        }
        
        // Structure to track multiple non-blocking sends
        struct DistributionBuffer {
            unsigned long* key_buffer;
            char* payload_buffer;
            MPI_Request key_request;
            MPI_Request payload_request;
            size_t target_size;
            int target_rank;
        };
        
        std::vector<DistributionBuffer> dist_buffers(num_nodes - 1);
        
        // Start all non-blocking sends simultaneously
        size_t current_offset = local_array_size;
        for (int i = 1; i < num_nodes; i++) {
            size_t target_size = array_size_per_node + (i < remainder ? 1 : 0);
            
            size_t distribution_bytes = target_size * (sizeof(unsigned long) + g_payload_size);
            if (DEBUG) {
                printf("[RANK 0] Starting non-blocking distribution to rank %d: %zu records, %.2f MB\n",
                    i, target_size, distribution_bytes / (1024.0 * 1024.0));
                fflush(stdout);
            }
            
            DistributionBuffer& buffer = dist_buffers[i-1];
            buffer.target_size = target_size;
            buffer.target_rank = i;
            buffer.key_buffer = new unsigned long[target_size];
            buffer.payload_buffer = new char[target_size * g_payload_size];
            
            // Prepare data while starting sends
            for (size_t j = 0; j < target_size; j++) {
                buffer.key_buffer[j] = array[current_offset + j].key;
                memcpy(buffer.payload_buffer + j * g_payload_size,
                      array[current_offset + j].rpayload,
                      g_payload_size);
            }
            
            // Start non-blocking sends immediately after data preparation
            MPI_Isend(buffer.key_buffer, target_size, MPI_UNSIGNED_LONG, i, 0, 
                     MPI_COMM_WORLD, &buffer.key_request);
            MPI_Isend(buffer.payload_buffer, target_size * g_payload_size, MPI_BYTE, i, 1,
                     MPI_COMM_WORLD, &buffer.payload_request);
            
            current_offset += target_size;
        }
        
        // OVERLAP: Can perform other initialization while sends are in progress
        // For example, prepare for local sorting
        if (DEBUG) {
            printf("[RANK 0] All distribution sends started, waiting for completion...\n");
            fflush(stdout);
        }
        
        // Wait for all sends to complete and clean up
        for (auto& buffer : dist_buffers) {
            MPI_Wait(&buffer.key_request, MPI_STATUS_IGNORE);
            MPI_Wait(&buffer.payload_request, MPI_STATUS_IGNORE);
            delete[] buffer.key_buffer;
            delete[] buffer.payload_buffer;
        }
        
    } else {
        // Non-blocking receive for other nodes with computation overlap
        size_t distribution_bytes = local_array_size * (sizeof(unsigned long) + g_payload_size);

        if (DEBUG) {
            printf("[RANK %d] Starting non-blocking receive from rank 0: %zu records, %.2f MB\n",
                   rank, local_array_size, distribution_bytes / (1024.0 * 1024.0));
            fflush(stdout);
        }
        
        // Allocate receive buffer
        unsigned long* key_buffer = new unsigned long[local_array_size];
        MPI_Request key_request, payload_request;
        
        // Start non-blocking receives
        MPI_Irecv(key_buffer, local_array_size, MPI_UNSIGNED_LONG, 0, 0, 
                 MPI_COMM_WORLD, &key_request);
        MPI_Irecv(local_payload_block, local_array_size * g_payload_size, MPI_BYTE, 0, 1, 
                 MPI_COMM_WORLD, &payload_request);
        
        // OVERLAP: Can perform other initialization while receiving
        // For example, initialize temporary arrays for sorting
        if (DEBUG) {
            printf("[RANK %d] Data receive started, preparing for computation...\n", rank);
            fflush(stdout);
        }
        
        // Wait for receives to complete
        MPI_Wait(&key_request, MPI_STATUS_IGNORE);
        MPI_Wait(&payload_request, MPI_STATUS_IGNORE);
        
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
    
    // Time variables 
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
    size_t total_sent_bytes = 0;
    size_t total_sent_records = 0;
    
    // Each rank sorts its local data
    if (sequential) {
        // Run sequential mergesort, in this case only rank 0 will do the sorting
        mergesort(local_array, 0, local_array_size - 1, tmp);
    } else {

        // --------------- Parallel Mergesort Phase -----------
        // Each rank sort it's local data in parallel with fastflow
        // Structure is a
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
                     
                MergeEmitter* merge_emitter = new MergeEmitter(merge_tasks);
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

    // --------------- NON-BLOCKING MPI Merging Phase with Computation Overlap -----------
    // Enhanced MPI merge phase with non-blocking communications and computation overlap
    // This implementation overlaps communication with merge preparation and data copying
    // to significantly reduce wall-clock time

    mpi_merge_start_time = std::chrono::high_resolution_clock::now();

    // Structure to track pending operations for overlap
    struct PendingOperation {
        MPI_Request size_request = MPI_REQUEST_NULL;
        MPI_Request keys_request = MPI_REQUEST_NULL;
        MPI_Request payload_request = MPI_REQUEST_NULL;
        unsigned long* key_buffer = nullptr;
        char* payload_buffer = nullptr;
        size_t buffer_size = 0;
        int partner_rank = -1;
        bool is_sender = false;
        size_t data_size = 0;
    };

    int step = 1;
    while (step < num_nodes) {
        if (DEBUG) {
            printf("[RANK %d] MPI merge step: %d\n", rank, step);
            fflush(stdout);
        }
        
        PendingOperation pending_op;
        bool has_pending = false;
        
        if (rank % (2 * step) == 0) {
            // Receiver process: 0,2,4,...,num_nodes-1
            int sender = rank + step;
            if (sender < num_nodes) {
                has_pending = true;
                pending_op.partner_rank = sender;
                pending_op.is_sender = false;
                
                // Start non-blocking receive for size
                MPI_Irecv(&pending_op.data_size, 1, MPI_UNSIGNED_LONG, sender, 2, 
                         MPI_COMM_WORLD, &pending_op.size_request);
                
                if (DEBUG) {
                    printf("[RANK %d] Started non-blocking receive from rank %d\n", rank, sender);
                    fflush(stdout);
                }
            }
        } else if (rank % step == 0) {
            // Sender process: 1,3,5,...,num_nodes-1
            int receiver = rank - step;
            if (receiver >= 0) {
                has_pending = true;
                pending_op.partner_rank = receiver;
                pending_op.is_sender = true;
                pending_op.data_size = local_array_size;
                
                size_t transfer_bytes = local_array_size * (sizeof(unsigned long) + g_payload_size);
                if (DEBUG) {
                    printf("[RANK %d] Sending to rank %d: %zu records, %.2f MB\n",
                           rank, receiver, local_array_size,
                           transfer_bytes / (1024.0 * 1024.0));
                    fflush(stdout);
                }
                
                // Track sent data
                total_sent_records += local_array_size;
                total_sent_bytes += transfer_bytes;
                
                // Start non-blocking send for size
                MPI_Isend(&local_array_size, 1, MPI_UNSIGNED_LONG, receiver, 2, 
                         MPI_COMM_WORLD, &pending_op.size_request);
            }
        }
        
        // Process pending operations with computation overlap
        if (has_pending) {
            if (pending_op.is_sender) {
                // SENDER: Prepare data while size is being sent
                
                // Wait for size to be sent
                MPI_Wait(&pending_op.size_request, MPI_STATUS_IGNORE);
                
                // Allocate and prepare key buffer while starting key send
                pending_op.key_buffer = new unsigned long[local_array_size];
                
                // Start key send immediately
                MPI_Isend(pending_op.key_buffer, local_array_size, MPI_UNSIGNED_LONG, 
                         pending_op.partner_rank, 3, MPI_COMM_WORLD, &pending_op.keys_request);
                
                // OVERLAP: Copy keys while send is in progress
                for (size_t i = 0; i < local_array_size; i++) {
                    pending_op.key_buffer[i] = local_array[i].key;
                }
                
                // Start payload send
                MPI_Isend(local_payload_block, local_array_size * g_payload_size, MPI_BYTE,
                         pending_op.partner_rank, 4, MPI_COMM_WORLD, &pending_op.payload_request);
                
                // Wait for all sends to complete
                MPI_Wait(&pending_op.keys_request, MPI_STATUS_IGNORE);
                MPI_Wait(&pending_op.payload_request, MPI_STATUS_IGNORE);
                
                // Clean up sender
                delete[] pending_op.key_buffer;
                
                // Mark this rank as finished sender for cleanup
                // Keep local arrays for performance reporting, will be cleaned later
                local_array_size = 0; // Signal that this rank is done
                break; // Sender is done
                
            } else {
                // RECEIVER: Overlap communication with merge preparation
                
                // Wait for size
                MPI_Wait(&pending_op.size_request, MPI_STATUS_IGNORE);
                size_t received_size = pending_op.data_size;
                
                size_t transfer_bytes = received_size * (sizeof(unsigned long) + g_payload_size);
                if (DEBUG) {
                    printf("[RANK %d] Receiving from rank %d: %zu records, %.2f MB\n",
                           rank, pending_op.partner_rank, received_size,
                           transfer_bytes / (1024.0 * 1024.0));
                    fflush(stdout);
                }
                
                // Track received data
                total_received_records += received_size;
                total_received_bytes += transfer_bytes;
                
                // Allocate receive buffers
                pending_op.key_buffer = new unsigned long[received_size];
                pending_op.payload_buffer = new char[received_size * g_payload_size];
                
                // Start non-blocking receives
                MPI_Irecv(pending_op.key_buffer, received_size, MPI_UNSIGNED_LONG,
                         pending_op.partner_rank, 3, MPI_COMM_WORLD, &pending_op.keys_request);
                MPI_Irecv(pending_op.payload_buffer, received_size * g_payload_size, MPI_BYTE,
                         pending_op.partner_rank, 4, MPI_COMM_WORLD, &pending_op.payload_request);
                
                // OVERLAP: Prepare merge structures while receiving data
                Record* merged_array = new Record[local_array_size + received_size];
                char* merged_payload = new char[(local_array_size + received_size) * g_payload_size];
                Record* tmp = new Record[local_array_size + received_size];
                
                // Copy local data to merged array (computation overlap)
                for (size_t i = 0; i < local_array_size; i++) {
                    merged_array[i].key = local_array[i].key;
                    memcpy(merged_payload + i * g_payload_size, 
                           local_array[i].rpayload, g_payload_size);
                    merged_array[i].rpayload = merged_payload + i * g_payload_size;
                }
                
                // Wait for all data to arrive
                MPI_Wait(&pending_op.keys_request, MPI_STATUS_IGNORE);
                MPI_Wait(&pending_op.payload_request, MPI_STATUS_IGNORE);
                
                // Copy received data to merged array
                for (size_t i = 0; i < received_size; i++) {
                    merged_array[local_array_size + i].key = pending_op.key_buffer[i];
                    memcpy(merged_payload + (local_array_size + i) * g_payload_size,
                           pending_op.payload_buffer + i * g_payload_size, g_payload_size);
                    merged_array[local_array_size + i].rpayload = 
                        merged_payload + (local_array_size + i) * g_payload_size;
                }
                
                // Perform final merge
                merge(merged_array, 0, local_array_size - 1, local_array_size + received_size - 1, tmp);
                
                // Clean up old data
                delete[] tmp;
                delete[] local_array;
                delete[] local_payload_block;
                delete[] pending_op.key_buffer;
                delete[] pending_op.payload_buffer;
                
                // Update local arrays
                local_array = merged_array;
                local_payload_block = merged_payload;
                local_array_size += received_size;
            }
        }
        
        // Synchronization point for debugging
 
        
        step *= 2;
    }
    mpi_merge_end_time = std::chrono::high_resolution_clock::now();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto sort_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sort_end_time - sort_start_time);
    auto merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(merge_end_time - merge_start_time);
    auto mpi_merge_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mpi_merge_end_time - mpi_merge_start_time);
    auto overhead_duration = total_duration - sort_duration - merge_duration - mpi_merge_duration;
    

    // ---------- Performance Debugging Phase -----------

    // Print per-rank timing and memory information (debug mode only)
    if (DEBUG) {
        for (int i = 0; i < num_nodes; i++) {
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
            // Synchronize for ordered output
            MPI_Barrier(MPI_COMM_WORLD);  
        }
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
                  << "Total processing time: " << total_duration.count() << " ms\n";
                  
        if (!sequential && DEBUG) {
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
    
    // Clean up - all ranks need to clean up their resources
    if (rank == 0) {
        if (array != local_array) {
            delete[] array;
            delete[] payload_block;
        }
    }
    
    // Clean up local resources for all ranks
    if (local_array_size > 0) { // Only if not a finished sender
        delete[] local_array;
        delete[] local_payload_block;
    }
    
    if (sequential && tmp != nullptr) {
        delete[] tmp;
    }
    
    MPI_Finalize();
    return 0;
}