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
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [start] [end]" << std::endl;
        return 1;
    }


    int start = std::atoi(argv[1]);
    int end = std::atoi(argv[2]);

    if (start <= 0 || end <= 0 || start > end) {
        std::cerr << "Invalid range. Please provide positive integers with start <= end." << std::endl;
        return 1;
    }

    int max_steps = 0;
    int number_with_max_steps = start;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = start; i <= end; ++i) {
        int steps = collatz_steps(i);
        if (steps > max_steps) {
            max_steps = steps;
            number_with_max_steps = i;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed_time = end_time - start_time;

    std::cout << "Time taken: " << elapsed_time.count() << " seconds." << std::endl;
    std::cout << "Number with maximum steps: " << number_with_max_steps << " with " << max_steps << " steps." << std::endl;

    return 0;
}