#include <config.hpp>
#include <cmdline.hpp>
#include <utility.hpp>
#include <omp.h>
#include <vector>

// Parallel version of walkDir
static inline bool walkDir_par(const char dname[], const bool comp) {
    if (chdir(dname) == -1) {
        if (QUITE_MODE>=1) {
            perror("chdir");
            std::fprintf(stderr, "Error: chdir %s\n", dname);
        }
        return false;
    }
    
    DIR *dir;
    if ((dir=opendir(".")) == NULL) {
        if (QUITE_MODE>=1) {
            perror("opendir");
            std::fprintf(stderr, "Error: opendir %s\n", dname);
        }
        return false;
    }
    
    // First, collect all files and directories
    std::vector<std::string> subdirs;
    std::vector<std::pair<std::string, size_t>> files;
    
    struct dirent *file;
    bool error = false;
    
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
            if (!isdot(file->d_name)) {
                subdirs.push_back(std::string(file->d_name));
            }
        } else {
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
    
    // Process files in parallel using tasks
    #pragma omp parallel
    {
        #pragma omp single
        {
            for(auto &file_info : files) {
                #pragma omp task firstprivate(file_info)
                {
                    if (!doWork(file_info.first.c_str(), file_info.second, comp)) {
                        #pragma omp critical
                        {
                            error = true;
                        }
                    }
                }
            }
            
            // Process subdirectories
            for(auto &subdir : subdirs) {
                #pragma omp task firstprivate(subdir)
                {
                    bool subdir_result = walkDir_par(subdir.c_str(), comp);
                    
                    if (subdir_result) {
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
            
            #pragma omp taskwait
        }
    }
    
    return !error;
}

// Parallel version for compressing data in chunks
static inline bool compressData_par(unsigned char *ptr, size_t size, const std::string &fname) {
    unsigned char *inPtr = ptr;
    size_t inSize = size;
    
    // If file is small, use the sequential version
    if (size < BUF_SIZE * 4) {
        printf("Sequential Compressing %s of size %zu\n", fname.c_str(), size);
        return compressData(ptr, size, fname);
    }
    
    //compressing in parallel chunks
    printf("Parallel compressing %s of size %zu\n", fname.c_str(), size); 
    
    // For large files, compress in parallel chunks
    // We'll determine the number of chunks based on file size and number of available threads
    int num_threads = omp_get_max_threads();
    int num_chunks = std::min(num_threads * 2, int(size / BUF_SIZE) + 1);
    size_t chunk_size = (size + num_chunks - 1) / num_chunks;
    
    // Allocate space for compressed chunks
    std::vector<unsigned char*> compressed_chunks(num_chunks);
    std::vector<size_t> compressed_sizes(num_chunks);
    
    bool success = true;
    
    #pragma omp parallel for
    for (int i = 0; i < num_chunks; i++) {
        size_t offset = i * chunk_size;
        size_t current_chunk_size = std::min(chunk_size, size - offset);
        
        size_t cmp_bound = compressBound(current_chunk_size);
        compressed_chunks[i] = new unsigned char[cmp_bound];
        compressed_sizes[i] = cmp_bound;
        
        if (compress(compressed_chunks[i], &compressed_sizes[i], inPtr + offset, current_chunk_size) != Z_OK) {
            #pragma omp critical
            {
                if (QUITE_MODE >= 1)
                    std::fprintf(stderr, "Failed to compress chunk %d\n", i);
                success = false;
            }
        }
    }
    
    if (!success) {
        // Clean up allocated memory
        for (int i = 0; i < num_chunks; i++) {
            if (compressed_chunks[i]) {
                delete[] compressed_chunks[i];
            }
        }
        return false;
    }
    
    // Calculate total compressed size
    size_t total_compressed_size = 0;
    for (int i = 0; i < num_chunks; i++) {
        total_compressed_size += compressed_sizes[i];
    }
    
    // Write to output file
    std::string outfile = fname + SUFFIX;
    std::ofstream outFile(outfile, std::ios::binary);
    if (!outFile.is_open()) {
        std::fprintf(stderr, "Failed to open output file: %s\n", outfile.c_str());
        // Clean up
        for (int i = 0; i < num_chunks; i++) {
            delete[] compressed_chunks[i];
        }
        return false;
    }
    
    // Write original size and number of chunks
    outFile.write(reinterpret_cast<const char*>(&inSize), sizeof(inSize));
    outFile.write(reinterpret_cast<const char*>(&num_chunks), sizeof(num_chunks));
    
    // Write chunk sizes
    for (int i = 0; i < num_chunks; i++) {
        outFile.write(reinterpret_cast<const char*>(&compressed_sizes[i]), sizeof(compressed_sizes[i]));
    }
    
    // Write compressed chunks
    for (int i = 0; i < num_chunks; i++) {
        outFile.write(reinterpret_cast<const char*>(compressed_chunks[i]), compressed_sizes[i]);
        delete[] compressed_chunks[i];
    }
    
    outFile.close();
    
    if (REMOVE_ORIGIN) {
        unlink(fname.c_str());
    }
    
    return true;
}

// Parallel version for decompressing data in chunks
static inline bool decompressData_par(unsigned char *ptr, size_t size, const std::string &fname) {
    // Read original size
    size_t decompressedSize = reinterpret_cast<size_t*>(ptr)[0];
    ptr += sizeof(size_t);
    
    // Read number of chunks
    int num_chunks = *reinterpret_cast<int*>(ptr);
    ptr += sizeof(int);
    
    // If only one chunk or small file, use the sequential version
    if (num_chunks <= 1 || decompressedSize < BUF_SIZE * 4) {
        return decompressData(ptr - sizeof(size_t) - sizeof(int), size, fname);
    }
    
    // Read chunk sizes
    std::vector<size_t> compressed_sizes(num_chunks);
    for (int i = 0; i < num_chunks; i++) {
        compressed_sizes[i] = *reinterpret_cast<size_t*>(ptr);
        ptr += sizeof(size_t);
    }
    
    // Allocate space for output file
    unsigned char *decompressed_data = nullptr;
    std::string outfile = fname.substr(0, fname.size() - strlen(SUFFIX));
    
    if (!allocateFile(outfile.c_str(), decompressedSize, decompressed_data)) {
        return false;
    }
    
    // Decompress chunks in parallel
    bool success = true;
    size_t chunk_size = (decompressedSize + num_chunks - 1) / num_chunks;
    
    #pragma omp parallel for
    for (int i = 0; i < num_chunks; i++) {
        size_t output_offset = i * chunk_size;
        size_t current_chunk_size = std::min(chunk_size, decompressedSize - output_offset);
        
        unsigned char *compressed_chunk = ptr;
        size_t current_compressed_size = compressed_sizes[i];
        
        // Calculate offset for compressed data
        for (int j = 0; j < i; j++) {
            compressed_chunk += compressed_sizes[j];
        }
        
        // Decompress this chunk
        size_t output_size = current_chunk_size;
        if (uncompress(decompressed_data + output_offset, &output_size, 
                      compressed_chunk, current_compressed_size) != Z_OK) {
            #pragma omp critical
            {
                if (QUITE_MODE >= 1)
                    std::fprintf(stderr, "Failed to decompress chunk %d\n", i);
                success = false;
            }
        }
    }
    
    // Write to disk
    unmapFile(decompressed_data, decompressedSize);
    
    if (!success) {
        return false;
    }
    
    if (REMOVE_ORIGIN) {
        unlink(fname.c_str());
    }
    
    return true;
}

// Parallel work function
static inline bool doWork_par(const char fname[], size_t size, const bool comp) {
    unsigned char *ptr = nullptr;
    if (!mapFile(fname, size, ptr)) {
        if (QUITE_MODE >= 1) 
            std::fprintf(stderr, "mapFile %s failed\n", fname);
        return false;
    }
    
    bool r = (comp) ? 
        compressData_par(ptr, size, fname) :
        decompressData_par(ptr, size, fname);
    
    unmapFile(ptr, size);
    return r;
}

