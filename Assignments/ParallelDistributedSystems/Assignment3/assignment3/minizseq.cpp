/*
 * miniz source code: https://github.com/richgel999/miniz
 * https://code.google.com/archive/p/miniz/
 * 
 * This is a reworked version of the example3.c file distributed with the miniz.c.
 * --------------------
 * example3.c - Demonstrates how to use miniz.c's deflate() and inflate() functions for simple file compression.
 * Public domain, May 15 2011, Rich Geldreich, richgel99@gmail.com. See "unlicense" statement at the end of tinfl.c.
 * For simplicity, this example is limited to files smaller than 4GB, but this is not a limitation of miniz.c.
 * -------------------
 *
 */
/* Author: Massimo Torquati <massimo.torquati@unipi.it>
 * This code is a mix of POSIX C code and some C++ library call.
 */

#include <config.hpp>
#include <cmdline.hpp>
#include <utility.hpp>
#include <chrono>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    // parse command line arguments and set some global variables
    long start=parseCommandLine(argc, argv);
    if (start<0) return -1;
  
	double start_time, end_time;
	// use chrono
	start_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();

	
	
	bool success = true;
	while(argv[start]) {
		size_t filesize=0;
		if (isDirectory(argv[start], filesize)) {
			success &= walkDir(argv[start],COMP);
		} else {
			success &= doWork(argv[start], filesize,COMP);
		}
		start++;
	}
	if (!success) {
		printf("Exiting with (some) Error(s)\n");
		return -1;
	}

	end_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	// print in seconds
	printf("[TIMING] Total: %.6f s\n", (end_time - start_time) / 1000000000.0);
	//printf("[TIMING] Total: %.3f ms\n", (end_time - start_time) / 1000000.0);


	printf("Exiting with Success\n");
	return 0;
}
