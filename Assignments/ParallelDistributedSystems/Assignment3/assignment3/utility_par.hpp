#include <config.hpp>
#include <cmdline.hpp>
#include <utility.hpp>
#include <omp.h>
#include <vector>

// Parallel version of walkDir
// This function recursively processes directories in parallel using OpenMP tasks
// It compresses/decompresses files found in directories based on the 'comp' parameter
static inline bool walkDir_par(const char dname[], const bool comp) {
    double start_time = omp_get_wtime();
    double dir_scan_time = 0, processing_time = 0;
    
    // Change to the target directory
    if (chdir(dname) == -1) {
        if (QUITE_MODE>=1) {
            perror("chdir");
            std::fprintf(stderr, "Error: chdir %s\n", dname);
        }
        return false;
    }
    
    // Open the directory for reading
    DIR *dir;
    if ((dir=opendir(".")) == NULL) {
        if (QUITE_MODE>=1) {
            perror("opendir");
            std::fprintf(stderr, "Error: opendir %s\n", dname);
        }
        return false;
    }
    
    // First, collect all files and directories instead of processing immediately
    // This two-phase approach allows better parallelization
    std::vector<std::string> subdirs;               // Stores subdirectories to process later
    std::vector<std::pair<std::string, size_t>> files;  // Stores files with their sizes
    
    struct dirent *file;
    bool error = false;
    
    double scan_start_time = omp_get_wtime();
    
    // Phase 1: Scan directory and collect items to process
    while((errno=0, file = readdir(dir)) != NULL) {
        struct stat statbuf;
        if (stat(file->d_name, &statbuf) == -1) {
            if (QUITE_MODE>=1) {
                perror("stat");
                std::fprintf(stderr, "Error: stat %s\n", file->d_name);
            }
            closedir(dir);
            return false;
        }
        
        if(S_ISDIR(statbuf.st_mode)) {
            // If it's a directory and not "." or "..", add to subdirs list
            if (!isdot(file->d_name)) {
                subdirs.push_back(std::string(file->d_name));
            }
        } else {
            // If it's a file, check if we should process it based on the operation (compress/decompress)
            if (!discardIt(file->d_name, comp)) {
                files.push_back(std::make_pair(std::string(file->d_name), statbuf.st_size));
            } else if (QUITE_MODE>=2) {
                if (comp) {
                    std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", file->d_name, SUFFIX);
                } else {
                    std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", file->d_name, SUFFIX);
                }
            }
        }
    }
    
    if (errno != 0) {
        if (QUITE_MODE>=1) perror("readdir");
        error = true;
    }
    closedir(dir);
    
    dir_scan_time = omp_get_wtime() - scan_start_time;
    
    // Phase 2: Process files in parallel using tasks
    double processing_start_time = omp_get_wtime();
    
    #pragma omp parallel
    {
        #pragma omp single  // Only one thread creates the tasks
        {
            // Process all collected files in parallel
            for(auto &file_info : files) {
                #pragma omp task firstprivate(file_info)  // Create a task for each file
                {
                    if (!doWork(file_info.first.c_str(), file_info.second, comp)) {
                        #pragma omp critical  // Thread-safe update of error status
                        {
                            error = true;
                        }
                    }
                }
            }
            
            // Process subdirectories recursively in parallel
            for(auto &subdir : subdirs) {
                #pragma omp task firstprivate(subdir)  // Create a task for each subdirectory
                {
                    // Recursive call to process the subdirectory
                    bool subdir_result = walkDir_par(subdir.c_str(), comp);
                    
                    if (subdir_result) {
                        // After processing subdirectory, return to parent directory
                        if (chdir("..") == -1) {
                            perror("chdir");
                            std::fprintf(stderr, "Error: chdir ..\n");
                            #pragma omp critical
                            {
                                error = true;
                            }
                        }
                    } else {
                        #pragma omp critical
                        {
                            error = true;
                        }
                    }
                }
            }
            
            #pragma omp taskwait  // Wait for all tasks to complete before continuing
        }
    }
    
    processing_time = omp_get_wtime() - processing_start_time;
    double total_time = omp_get_wtime() - start_time;
    
    if (QUITE_MODE >= 2) {
        std::printf("[TIMING] walkDir_par(%s): Total: %.3f ms, Directory scan: %.3f ms (%.1f%%), Processing: %.3f ms (%.1f%%)\n",
                 dname, total_time * 1000, 
                 dir_scan_time * 1000, (dir_scan_time / total_time) * 100,
                 processing_time * 1000, (processing_time / total_time) * 100);
    }
    
    return !error;  // Return success status
}

// Parallel version for compressing data in chunks
static inline bool compressData_par(unsigned char *ptr, size_t size, const std::string &fname) {
    double start_time = omp_get_wtime();
    double setup_time = 0, compression_time = 0, file_writing_time = 0;
    
    unsigned char *inPtr = ptr;
    size_t inSize = size;
    
    // For small files, use the sequential version - not worth the parallel overhead
    if (size < BUF_SIZE * 4) {
        if (QUITE_MODE >= 2) {
            printf("Sequential Compressing %s of size %zu\n", fname.c_str(), size);
        }
        return compressData(ptr, size, fname);
    }
    
    if (QUITE_MODE >= 2) {
        printf("Parallel compressing %s of size %zu\n", fname.c_str(), size);
    }
    
    double setup_start = omp_get_wtime();
    // Determine optimal chunk size and number based on available threads and file size
    int num_threads = omp_get_max_threads();
    size_t optimal_chunk_size = std::max(BUF_SIZE * 4, (int)(size / (num_threads * 2)));
    int num_chunks = (size + optimal_chunk_size - 1) / optimal_chunk_size;
    
    // Ensure we don't create too many tiny chunks or too few large chunks
    if (num_chunks < num_threads) {
        num_chunks = num_threads;
        optimal_chunk_size = (size + num_chunks - 1) / num_chunks;
    } else if (num_chunks > num_threads * 4) {
        num_chunks = num_threads * 4;
        optimal_chunk_size = (size + num_chunks - 1) / num_chunks;
    }
    
    // Pre-allocate vectors with the right size to avoid reallocations
    std::vector<std::vector<unsigned char>> compressed_chunks(num_chunks);
    std::vector<size_t> compressed_sizes(num_chunks);
    setup_time = omp_get_wtime() - setup_start;
    
    bool success = true;
    
    double compression_start = omp_get_wtime();
    
    // Here start parallel region
    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic)
        for (int i = 0; i < num_chunks; i++) {
            // Calculate input and output positions for this chunk
            size_t input_offset = i * optimal_chunk_size;
            size_t current_chunk_size = std::min(optimal_chunk_size, inSize - input_offset);
            
            // Allocate space for compressed data
            compressed_chunks[i].resize(compressBound(current_chunk_size));
            size_t output_size = compressed_chunks[i].size();
            
            // Compress this chunk
            if (compress(compressed_chunks[i].data(), &output_size, inPtr + input_offset, current_chunk_size) != Z_OK) {
                #pragma omp critical
                {
                    if (QUITE_MODE >= 1) {
                        std::fprintf(stderr, "Failed to compress chunk %d\n", i);
                    }
                    success = false;
                }
            }
            
            // Store the actual size of the compressed data
            compressed_sizes[i] = output_size;
        }
    }

    compression_time = omp_get_wtime() - compression_start;
    
    if (!success) {
        return false;
    }

    double file_writing_start = omp_get_wtime();
    double total_compressed_size = 0;
    //double file_writing_time = 0;
    
    // Write to output file - use buffered output for better performance
    std::string outfile = fname + SUFFIX;
    std::ofstream outFile(outfile, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        if (QUITE_MODE >= 1) {
            std::fprintf(stderr, "Failed to open output file: %s\n", outfile.c_str());
        }
        return false;
    }
    
    // Set a larger buffer for file output
    std::vector<char> outbuf(BUF_SIZE);
    outFile.rdbuf()->pubsetbuf(outbuf.data(), outbuf.size());
    
    // Write header with original size and number of chunks
    outFile.write(reinterpret_cast<const char*>(&inSize), sizeof(inSize));
    outFile.write(reinterpret_cast<const char*>(&num_chunks), sizeof(num_chunks));
    
    // Write chunk sizes
    for (int i = 0; i < num_chunks; i++) {
        outFile.write(reinterpret_cast<const char*>(&compressed_sizes[i]), sizeof(compressed_sizes[i]));
    }
    
    // Write compressed chunks
    for (int i = 0; i < num_chunks; i++) {
        outFile.write(reinterpret_cast<const char*>(compressed_chunks[i].data()), compressed_sizes[i]);
    }
    
    outFile.close();
    
    if (REMOVE_ORIGIN) {
        unlink(fname.c_str());
    }

    file_writing_time = omp_get_wtime() - file_writing_start;
    
    double total_time = omp_get_wtime() - start_time;
    
    if (QUITE_MODE >= 2) {
        double compression_ratio = (double)total_compressed_size / size * 100.0;
        std::printf("[TIMING] compressData_par(%s): Total: %.3f ms, Setup: %.3f ms (%.1f%%), Compression: %.3f ms (%.1f%%), File Writing: %.3f ms (%.1f%%), Ratio: %.1f%%\n",
                 fname.c_str(), total_time * 1000, 
                 setup_time * 1000, (setup_time / total_time) * 100,
                 compression_time * 1000, (compression_time / total_time) * 100,
                 file_writing_time * 1000, (file_writing_time / total_time) * 100,
                 compression_ratio);
    }
    
    return true;
}

// Parallel version for decompressing data in chunks
static inline bool decompressData_par(unsigned char *ptr, size_t size, const std::string &fname) {
    double start_time = omp_get_wtime();
    double header_parsing_time = 0, file_allocation_time = 0, decompression_time = 0;
    
    double header_start = omp_get_wtime();
    // Read original size
    size_t decompressedSize = reinterpret_cast<size_t*>(ptr)[0];
    ptr += sizeof(size_t);
    
    // For small files, use the sequential version - not worth the parallel overhead
    if (size < BUF_SIZE * 4) {
        if (QUITE_MODE >= 2) {
            printf("Sequential Decompressing %s to size %zu\n", fname.c_str(), decompressedSize);
        }
        // Reset pointer and use sequential version
        ptr -= sizeof(size_t);
        return decompressData(ptr, size, fname);
    }
    
    if (QUITE_MODE >= 2) {
        printf("Parallel decompressing %s to size %zu\n", fname.c_str(), decompressedSize);
    }
    
    // Read number of chunks
    int num_chunks = *reinterpret_cast<int*>(ptr);
    ptr += sizeof(int);
    
    // Read chunk sizes
    std::vector<size_t> compressed_sizes(num_chunks);
    for (int i = 0; i < num_chunks; i++) {
        compressed_sizes[i] = *reinterpret_cast<size_t*>(ptr);
        ptr += sizeof(size_t);
    }
    
    // Calculate offsets for compressed chunks
    std::vector<size_t> chunk_offsets(num_chunks);
    chunk_offsets[0] = 0;
    for (int i = 1; i < num_chunks; i++) {
        chunk_offsets[i] = chunk_offsets[i-1] + compressed_sizes[i-1];
    }
    
    // Calculate optimal chunk size for decompressed data
    size_t chunk_size = (decompressedSize + num_chunks - 1) / num_chunks;
    header_parsing_time = omp_get_wtime() - header_start;
    
    // Allocate space for output file
    double allocation_start = omp_get_wtime();
    unsigned char *decompressed_data = nullptr;
    std::string outfile = fname.substr(0, fname.size() - strlen(SUFFIX));
    
    if (!allocateFile(outfile.c_str(), decompressedSize, decompressed_data)) {
        if (QUITE_MODE >= 1) {
            std::fprintf(stderr, "Failed to allocate output file: %s\n", outfile.c_str());
        }
        return false;
    }
    file_allocation_time = omp_get_wtime() - allocation_start;
    
    // Decompress chunks in parallel
    bool success = true;
    
    double decompression_start = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic)
        for (int i = 0; i < num_chunks; i++) {
            // Calculate output position for this chunk
            size_t output_offset = i * chunk_size;
            size_t current_chunk_size = std::min(chunk_size, decompressedSize - output_offset);
            
            // Get pointer to compressed chunk data
            unsigned char *compressed_chunk = ptr + chunk_offsets[i];
            
            // Buffer for decompressed data
            size_t output_size = current_chunk_size;
            
            // Decompress this chunk
            if (uncompress(decompressed_data + output_offset, &output_size, 
                          compressed_chunk, compressed_sizes[i]) != Z_OK) {
                #pragma omp critical
                {
                    if (QUITE_MODE >= 1) {
                        std::fprintf(stderr, "Failed to decompress chunk %d\n", i);
                    }
                    success = false;
                }
            }
            
            // Verify the expected size
            if (output_size != current_chunk_size) {
                #pragma omp critical
                {
                    if (QUITE_MODE >= 1) {
                        std::fprintf(stderr, "Warning: Chunk %d decompressed to %zu bytes instead of expected %zu bytes\n", 
                                    i, output_size, current_chunk_size);
                    }
                }
            }
        }
    }
    decompression_time = omp_get_wtime() - decompression_start;
    
    // Write to disk by unmapping the file
    unmapFile(decompressed_data, decompressedSize);
    
    if (!success) {
        return false;
    }
    
    if (REMOVE_ORIGIN) {
        unlink(fname.c_str());
    }
    
    double total_time = omp_get_wtime() - start_time;
    
    if (QUITE_MODE >= 2) {
        std::printf("[TIMING] decompressData_par(%s): Total: %.3f ms, Header Parsing: %.3f ms (%.1f%%), File Allocation: %.3f ms (%.1f%%), Decompression: %.3f ms (%.1f%%)\n",
                 fname.c_str(), total_time * 1000, 
                 header_parsing_time * 1000, (header_parsing_time / total_time) * 100,
                 file_allocation_time * 1000, (file_allocation_time / total_time) * 100,
                 decompression_time * 1000, (decompression_time / total_time) * 100);
    }
    
    return true;
}

// Parallel work function
static inline bool doWork_par(const char fname[], size_t size, const bool comp) {
    double start_time = omp_get_wtime();
    double mapping_time = 0, processing_time = 0, unmapping_time = 0;
    
    double mapping_start = omp_get_wtime();
    unsigned char *ptr = nullptr;
    if (!mapFile(fname, size, ptr)) {
        if (QUITE_MODE >= 1) 
            std::fprintf(stderr, "mapFile %s failed\n", fname);
        return false;
    }
    mapping_time = omp_get_wtime() - mapping_start;
    
    double processing_start = omp_get_wtime();
    bool r = (comp) ? 
        compressData_par(ptr, size, fname) :
        decompressData_par(ptr, size, fname);
    processing_time = omp_get_wtime() - processing_start;
    
    double unmapping_start = omp_get_wtime();
    unmapFile(ptr, size);
    unmapping_time = omp_get_wtime() - unmapping_start;
    
    double total_time = omp_get_wtime() - start_time;
    
    if (QUITE_MODE >= 2) {
        std::printf("[TIMING] doWork_par(%s): Total: %.3f ms, File Mapping: %.3f ms (%.1f%%), %s: %.3f ms (%.1f%%), File Unmapping: %.3f ms (%.1f%%)\n",
                 fname, total_time * 1000, 
                 mapping_time * 1000, (mapping_time / total_time) * 100,
                 comp ? "Compression" : "Decompression", processing_time * 1000, (processing_time / total_time) * 100,
                 unmapping_time * 1000, (unmapping_time / total_time) * 100);
    }
    
    return r;
}

