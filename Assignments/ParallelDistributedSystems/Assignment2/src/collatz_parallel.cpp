// Implement Naive sequential Collatz Conjecture

// The Collatz Conjecture is a mathematical conjecture that states that for any positive integer n,
// the sequence defined by the following rules will eventually reach 1:

// 1. If n is even, divide it by 2.
// 2. If n is odd, multiply it by 3 and add 1.
// 3. Repeat the process indefinitely.

// The conjecture is that no matter what value of n, the sequence will always reach 1.

// The program takes [start] [end] and return for that the maximum count of steps found into that range.
// We also return the time the algorithm took to run.

#include<iostream>
#include<vector>
#include<stdlib.h>
#include<algorithm>
#include<chrono>

// For parallel version we add
#include<thread>
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<cstring>

using ull = unsigned long long;

ull collatz_steps(ull n) {
    ull steps = 0; 

    while (n != 1) {
        if (n % 2 == 0) {
            n /= 2;
        } else {
            n = 3 * n + 1;
        }
        steps++;
    }

    return steps;
}

// Static policy work such that we do the division upfront the calculation
void static_distribution_policy(int num_threads, int chunk_size, ull start, ull end, std::vector<ull>& results) {
    bool cyclic = false; // Set to true for cyclic distribution, false for block-based distribution

    auto worker = [&](int thread_id) {
        if (cyclic) {
            // Cyclic distribution
            for (ull i = start + thread_id; i <= end; i += num_threads) {
                ull steps = collatz_steps(i);
                results[i - start] = steps;
            }
        } else {
            // Block-based distribution
            ull chunk_start = start + thread_id * chunk_size;
            while (chunk_start <= end) {
                ull chunk_end = std::min(chunk_start + chunk_size - 1, end);
                for (ull i = chunk_start; i <= chunk_end; ++i) {
                    ull steps = collatz_steps(i);
                    results[i - start] = steps;
                }
                chunk_start += num_threads * chunk_size;
            }
        }
    };

    std::vector<std::thread> threads(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }
}

// Dynamic policy work such that we divide during the execution, we can do it on demand by using thread pool for example
void dynamic_distribution_policy(int num_threads, int chunk_size, ull start, ull end, std::vector<ull>& results) {
    // How to implement dynamic distribution policy?
    // use a global counter protected by a mutex
    // read global value of a counter, increase atomically by chunk size
    
    // For the dynamic distribution we can just modify a little bit the static distribution
    // such that we can use a global counter to keep track of the current number
    // to be processed. We can use a condition variable to notify the threads when a new number is available.

    // This is a simple implementation of dynamic distribution policy


}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [start1] [end1] [start2] [end2] ... [-n {number_threads}] [-c {batch_dimension}] [-d]" << std::endl;
        return 1;
    }

    std::vector<std::pair<ull, ull>> ranges;
    int num_threads = 16; // default number of threads
    int batch_size = 1;   // default batch size
    bool dynamic = false; // default scheduling is static

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
                num_threads = std::stoi(argv[++i]);
            } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                batch_size = std::stoi(argv[++i]);
            } else if (strcmp(argv[i], "-d") == 0) {
                dynamic = true;
            }
        } else {
            ull start = std::stoull(argv[i++]);
            ull end = std::stoull(argv[i]);
            ranges.emplace_back(start, end);
        }
    }

    // To run a program with multiple ranges write : .\program_name 1 100 2 200 -n 4 -c 10

    if (ranges.empty()) {
        std::cerr << "Error: At least one range must be specified." << std::endl;
        return 1;
    }

    // Start the timer for all ranges
    auto total_start_time = std::chrono::high_resolution_clock::now();

    // Process each range
    for (const auto& range : ranges) {
        ull start = range.first;
        ull end = range.second;

        std::vector<ull> results(end - start + 1);

        auto start_time = std::chrono::high_resolution_clock::now();

        if (dynamic) {
            std::cout << "Dynamic distribution policy is used." << std::endl;
            dynamic_distribution_policy(num_threads, batch_size, start, end, results);            
        } else {
            std::cout << "Static distribution policy is used." << std::endl;
            static_distribution_policy(num_threads, batch_size, start, end, results);
        }

        auto end_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed_time = end_time - start_time;

        std::cout << "Range: [" << start << ", " << end << "] - Time: " << elapsed_time.count() << " seconds" << std::endl;
        std::cout << std::endl;
        // the maximum number of steps
        auto max_steps = *std::max_element(results.begin(), results.end());
        auto max_index = std::distance(results.begin(), std::max_element(results.begin(), results.end()));
        std::cout << "Max Steps: " << max_steps << " at index: " << max_index + start << std::endl;
        std::cout << std::endl;
    }

    // For all ranges
    
    auto total_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_elapsed_time = total_end_time - total_start_time;
    std::cout << "--------------------------" << std::endl;
    std::cout << "Total Time: " << total_elapsed_time.count() << " seconds" << std::endl;

    return 0;
}
