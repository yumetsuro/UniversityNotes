/*
 * miniz source code: https://github.com/richgel999/miniz
 * https://code.google.com/archive/p/miniz/
 * 
 * --------------------
 * example3.c - Demonstrates how to use miniz.c's deflate() and inflate() functions for simple file compression.
 * Public domain, May 15 2011, Rich Geldreich, richgel99@gmail.com. See "unlicense" statement at the end of tinfl.c.
 * For simplicity, this example is limited to files smaller than 4GB, but this is not a limitation of miniz.c.
 * -------------------
 *
 * Here we will implement a parallel version of the minizseq.cpp where we use the library miniz to parallelize: Using OpenMP
 */

#include <config.hpp>
#include <cmdline.hpp>
#include <utility_par.hpp>
#include <chrono>  // For standard timing

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    // parse command line arguments and set some global variables
    long start=parseCommandLine(argc, argv);
	
	double omp_time_start, omp_time_end;

	omp_time_start = omp_get_wtime();	

	// Initialize OpenMP
    int max_threads = omp_get_max_threads();
    int num_threads = (NUM_THREADS > 0) ? NUM_THREADS : max_threads;
    
    // Set the number of threads to use
    omp_set_num_threads(num_threads);
    
    if (QUITE_MODE >= 1) {
        printf("Using %d threads for parallel compression/decompression (max available: %d)\n", 
               num_threads, max_threads);
    }

    // Process files sequentially, but compress/decompress chunks of each file in parallel
    bool success = true;
    long current = start;
    
    while(argv[current]) {
        size_t filesize = 0;
        bool local_success = true;

        if (isDirectory(argv[current], filesize)) {
            local_success = walkDir_par(argv[current], COMP);
        } else {
            local_success = doWork_par(argv[current], filesize, COMP);
        }

        if (!local_success) {
            success = false;
            if (QUITE_MODE >= 1) {
                printf("Error processing %s\n", argv[current]);
            }
        }
        current++;
    }

	omp_time_end = omp_get_wtime();
	printf("Parallel execution time: %f seconds with %d threads\n", omp_time_end - omp_time_start, num_threads);

    // Check if any errors occurred during processing

	if (!success) {
		printf("Exiting with (some) Error(s)\n");
		return -1;
	}

	return 0;
}
