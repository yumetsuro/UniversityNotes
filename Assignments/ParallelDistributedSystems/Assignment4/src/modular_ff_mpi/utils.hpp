#pragma once
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <getopt.h>
#include "record.hpp"

// Function to parse size with suffixes (K, M, G)
inline size_t parse_size(const char* str) {
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
inline void usage(const char* progname) {
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
