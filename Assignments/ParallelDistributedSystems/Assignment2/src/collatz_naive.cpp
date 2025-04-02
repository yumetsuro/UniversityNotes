// Implement Naive sequential Collatz Conjecture

// The Collatz Conjecture is a mathematical conjecture that states that for any positive integer n,
// the sequence defined by the following rules will eventually reach 1:

// 1. If n is even, divide it by 2.
// 2. If n is odd, multiply it by 3 and add 1.
// 3. Repeat the process indefinitely.

// The conjecture is that no matter what value of n, the sequence will always reach 1.

// The program takes [start] [end] and return for that the maximum count of steps found into that range.
// We also return the time the algorithm took to run.

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <algorithm>
#include <chrono>

using ull = unsigned long long;

ull collatz_steps(ull n) {
    ull steps = 0;

    // Skip base cases
    if (n == 0) return 0;
    if (n == 1) return 1;

    // check if n is a power of 2
    // We use bit to check, 
    // in this case the condition is 0 only for powers of 2
    if ((n & (n - 1)) == 0) { 
        // return the count of trailing zeros
        return __builtin_ctzll(n);
    }

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

int main(int argc, char *argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [start1] [end1] [start2] [end2] ... " << std::endl;
        return 1;
    }

   std::vector<std::pair<ull, ull>> ranges;

   // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        ull start = std::stoull(argv[i++]);
        ull end = std::stoull(argv[i]);
        ranges.emplace_back(start, end);
    }


    // Start the timer for all ranges
    auto total_start_time = std::chrono::high_resolution_clock::now();

    // Process each range
    for (const auto& range : ranges) {
        ull start = range.first;
        ull end = range.second;

        std::vector<ull> results(end - start + 1);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Sequentially calculate the number of steps for each number in the range
        for (ull i = start; i <= end; ++i) {
            ull steps = collatz_steps(i);
            results[i - start] = steps;
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
