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

    unsigned int hw_threads = std::thread::hardware_concurrency();
    char* slurm_cpus = getenv("SLURM_CPUS_PER_TASK");
    
    std::cout << "Rank " << rank << ": HW threads=" << hw_threads 
              << ", SLURM CPUs=" << (slurm_cpus ? slurm_cpus : "not set") << std::endl;
    

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

    // Only rank 0 prints configuration
    if (rank == 0 && DEBUG) {
        std::cout << "MergeSort Configuration:\n"
                << "  Array size: " << array_size << " elements\n"
                << "  Record payload: " << g_payload_size << " bytes\n"
                << "  Record total size: " << record_size << " bytes\n"
                << "  Total payload size: " << total_payload_size / (1024*1024) << " MB\n"
                << "  Total memory: " << (array_size * record_size) / (1024*1024) << " MB\n"
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
    

    // Rank 0 distributes data
    if (rank == 0) {
        
        // Rank 0 sort it's local array
        for (size_t i = 0; i < local_array_size; i++) {
            local_array[i].key = array[i].key;
            memcpy(local_payload_block + i * g_payload_size, 
                  array[i].rpayload, 
                  g_payload_size);
            local_array[i].rpayload = local_payload_block + i * g_payload_size;
        }
        
        // The rest of array is sent to other nodes
        size_t current_offset = local_array_size;
        for (int i = 1; i < num_nodes; i++) {
            size_t target_size = array_size_per_node + (i < remainder ? 1 : 0);
            
            size_t distribution_bytes = target_size * (sizeof(unsigned long) + g_payload_size);
            if (DEBUG){
                printf("[RANK 0] Initial distribution to rank %d: %zu records, %.2f MB\n",
                    i, target_size, distribution_bytes / (1024.0 * 1024.0));
                fflush(stdout);
            }

            unsigned long* key_buffer = new unsigned long[target_size];
            for (size_t j = 0; j < target_size; j++) {
                key_buffer[j] = array[current_offset + j].key;
            }
            MPI_Send(key_buffer, target_size, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD);
            delete[] key_buffer;
            
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
        // Behaviour of other nodes: they receive their portion of the array and
        size_t distribution_bytes = local_array_size * (sizeof(unsigned long) + g_payload_size);

        if (DEBUG) {
            printf("[RANK %d] Receiving initial data from rank 0: %zu records, %.2f MB\n",
                   rank, local_array_size, distribution_bytes / (1024.0 * 1024.0));
            fflush(stdout);
        }
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

    // --------------- MPI Merging Phase -----------
    // Each rank merges its local sorted data with others using MPI
    // Higher rank send to lower ranks like a tree structure
    // So we need to merge at each step from step 1 to num_nodes/2
    // For unbalanced tree number of nodes, we use step to get last node into the rank 0 node.
    // Eg 5 nodes:
    //           - step 1: 1 -> 0; 3 -> 2; 4 is left alone
    //           - step 2: 2 -> 0; 4 is left alone
    //          - step 4: 4 -> 0; 0 is the final result 
    // This is handled by the rank+step check. 

    mpi_merge_start_time = std::chrono::high_resolution_clock::now();

    int step = 1;
    while (step < num_nodes) {
        // Print the step to understand the merging process
        if DEBUG
            printf("[RANK %d] MPI merge step: %d\n", rank, step);
        
            if (rank % (2 * step) == 0) {
            // Receiver process : 0,2,4,...,num_nodes-1
            int sender = rank + step;
            if (sender < num_nodes) {

                size_t received_size;
                MPI_Recv(&received_size, 1, MPI_UNSIGNED_LONG, sender, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                size_t transfer_bytes = received_size * (sizeof(unsigned long) + g_payload_size);
                
                if (DEBUG) {
                printf("[RANK %d] Receiving from rank %d: %zu records, %.2f MB (keys: %.2f MB + payload: %.2f MB)\n",
                       rank, sender, received_size,
                       transfer_bytes / (1024.0 * 1024.0),
                       (received_size * sizeof(unsigned long)) / (1024.0 * 1024.0),
                       (received_size * g_payload_size) / (1024.0 * 1024.0));
                fflush(stdout);
                }

                // Track received data
                total_received_records += received_size;
                total_received_bytes += transfer_bytes;
                
                unsigned long* received_keys = new unsigned long[received_size];
                MPI_Recv(received_keys, received_size, MPI_UNSIGNED_LONG, sender, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                char* received_payload = new char[received_size * g_payload_size];
                MPI_Recv(received_payload, received_size * g_payload_size, MPI_BYTE, sender, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               
                // Important: after receiving we need to merge the local with received data
                Record* received_records = new Record[received_size];
                for (size_t i = 0; i < received_size; i++) {
                    received_records[i].key = received_keys[i];
                    received_records[i].rpayload = received_payload + i * g_payload_size;
                }
                delete[] received_keys;
                
                Record* merged_array = new Record[local_array_size + received_size];
                char* merged_payload = new char[(local_array_size + received_size) * g_payload_size];
                
                for (size_t i = 0; i < local_array_size; i++) {
                    merged_array[i].key = local_array[i].key;
                    memcpy(merged_payload + i * g_payload_size, 
                        local_array[i].rpayload, g_payload_size);
                    merged_array[i].rpayload = merged_payload + i * g_payload_size;
                }

                for (size_t i = 0; i < received_size; i++) {
                    merged_array[local_array_size + i].key = received_records[i].key;
                    memcpy(merged_payload + (local_array_size + i) * g_payload_size,
                        received_records[i].rpayload, g_payload_size);
                    merged_array[local_array_size + i].rpayload = merged_payload + (local_array_size + i) * g_payload_size;
                }

                Record* tmp = new Record[local_array_size + received_size];

                //We need a final merge from array coming  
                merge(merged_array, 0, local_array_size - 1, local_array_size + received_size - 1, tmp);

                delete[] tmp;
                delete[] local_array;
                delete[] local_payload_block;
                delete[] received_records;
                delete[] received_payload;
                
                local_array = merged_array;
                local_payload_block = merged_payload;
                local_array_size += received_size;
            }
        } else if (rank % step == 0) {
            // Sender process: 1,3,5,...,num_nodes-1
            int receiver = rank - step;
            if (receiver >= 0) {
                size_t transfer_bytes = local_array_size * (sizeof(unsigned long) + g_payload_size);
                
                if (DEBUG) {
                    printf("[RANK %d] Sending to rank %d: %zu records, %.2f MB (keys: %.2f MB + payload: %.2f MB)\n",
                           rank, receiver, local_array_size,
                           transfer_bytes / (1024.0 * 1024.0),
                           (local_array_size * sizeof(unsigned long)) / (1024.0 * 1024.0),
                           (local_array_size * g_payload_size) / (1024.0 * 1024.0));
                }
                // Track sent data
                total_sent_records += local_array_size;
                total_sent_bytes += transfer_bytes;
                
                MPI_Send(&local_array_size, 1, MPI_UNSIGNED_LONG, receiver, 2, MPI_COMM_WORLD);
                
                unsigned long* key_buffer = new unsigned long[local_array_size];
                for (size_t i = 0; i < local_array_size; i++) {
                    key_buffer[i] = local_array[i].key;
                }
                MPI_Send(key_buffer, local_array_size, MPI_UNSIGNED_LONG, receiver, 3, MPI_COMM_WORLD);
                delete[] key_buffer;
                
                MPI_Send(local_payload_block, local_array_size * g_payload_size, MPI_BYTE, receiver, 4, MPI_COMM_WORLD);
                
                // Node finished, proceed to exit
                delete[] local_array;
                delete[] local_payload_block;
                
                break; 
            }
        }
        // Step grows by binary factor for tree merge
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

    // Print per-rank timing and memory information
    for (int i = 0; i < num_nodes; i++) {
        if (rank == i && DEBUG) {
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
        // We need a barrier for orderred performance output
        MPI_Barrier(MPI_COMM_WORLD);  
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
    
    // Clean up, only for rank 0, other nodes finished earlier
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
