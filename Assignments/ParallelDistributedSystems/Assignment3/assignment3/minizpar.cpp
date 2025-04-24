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
    int num_threads = omp_get_max_threads();
    if (QUITE_MODE >= 1) {
        printf("Using %d threads for parallel compression/decompression\n", num_threads);
    }

	bool success = true;

	#pragma omp parallel
	{
		#pragma omp single
		{
			while(argv[start]) {
				#pragma omp task firstprivate(start)
				{
					size_t filesize=0;
					bool local_success = true;

					if (isDirectory(argv[start], filesize)) {
						local_success = walkDir_par(argv[start], COMP);
					} else {
						local_success = doWork_par(argv[start], filesize, COMP);
					}

					if (!local_success) {
						#pragma omp critical
						{
							success = false;
						}
					}
				}
				#pragma omp critical
				{
				start++;
				}
				//start++;
			}
			#pragma omp taskwait
		}
	}

	omp_time_end = omp_get_wtime();
	printf("Parallel execution time: %f seconds\n", omp_time_end - omp_time_start);

	if (!success) {
		printf("Exiting with (some) Error(s)\n");
		return -1;
	}

	return 0;
}
	