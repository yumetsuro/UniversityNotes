// Implement Parallel sequential Collatz Conjecture

// The Collatz Conjecture is a mathematical conjecture that states that for any positive integer n,
// the sequence defined by the following rules will eventually reach 1:

// 1. If n is even, divide it by 2.
// 2. If n is odd, multiply it by 3 and add 1.
// 3. Repeat the process indefinitely.

// The conjecture is that no matter what value of n, the sequence will always reach 1.
#include<iostream>
#include<vector>
#include<stdlib.h>
#include<chrono>

#include<thread>
#include<cstring>
#include<future>
#include "threadPool.hpp" 

#define DEBUG 1

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

void static_distribution_policy(int num_threads, int chunk_size, ull start, ull end, std::vector<ull>& results) {

    auto worker = [&](int thread_id) {
        for (ull i = start + thread_id; i <= end; i += num_threads) {
            ull steps = collatz_steps(i);
            results[i - start] = steps;
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


void dynamic_distribution_policy(int num_threads, int chunk_size, ull start, ull end, std::vector<ull>& results) {
    ThreadPool TP(num_threads);
    std::vector<std::future<void>> futures;
    std::atomic<ull> next_index(start);

    // Each thread pulls work in chunks until all work is done
    auto dynamic_work = [&]() {
        while (true) {
            ull i = next_index.fetch_add(chunk_size);
            if (i > end) break;

            ull chunk_start = i;
            ull chunk_end = std::min(i + chunk_size - 1, end);

            for (ull j = chunk_start; j <= chunk_end; ++j) {
                ull steps = collatz_steps(j);
                results[j - start] = steps;
            }
        }
    };

    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(TP.enqueue(dynamic_work));
    }

    for (auto& future : futures) {
        future.get();
    }
}


int main(int argc, char *argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [start1] [end1] [start2] [end2] ... [-n {number_threads}] [-c {batch_dimension}] [-d]" << std::endl;
        return 1;
    }

    std::vector<std::pair<ull, ull>> ranges;
    int num_threads = 16;
    int batch_size = 1;  
    bool dynamic = false; 

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

    if (ranges.empty()) {
        std::cerr << "Error: At least one range must be specified." << std::endl;
        return 1;
    }

    auto total_start_time = std::chrono::high_resolution_clock::now();

    for (const auto& range : ranges) {
        ull start = range.first;
        ull end = range.second;

        std::vector<ull> results(end - start + 1);
        auto start_time = std::chrono::high_resolution_clock::now();

        if (dynamic) {
            dynamic_distribution_policy(num_threads, batch_size, start, end, results);
        } else {
            static_distribution_policy(num_threads, batch_size, start, end, results);
        }

        auto end_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed_time = end_time - start_time;

        std::cout << "Range: [" << start << ", " << end << "] - Time: " << elapsed_time.count() << " seconds" 
                  << " - Algorithm: " << (dynamic ? "Dynamic" : "Static") << " - Threads: " << num_threads
                  << " - Batch Size: " << batch_size;
        std::cout << std::endl;

        
        #if DEBUG
            auto max_steps = *std::max_element(results.begin(), results.end());
            auto max_index = std::distance(results.begin(), std::max_element(results.begin(), results.end()));
            
            std::cout << "Max Steps: " << max_steps << " at index: " << max_index + start << std::endl;
            std::cout << std::endl;
        #endif
    }

    #if DEBUG 
        auto total_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> total_elapsed_time = total_end_time - total_start_time;
        std::cout << "--------------------------" << std::endl;
        std::cout << "Total Time: " << total_elapsed_time.count() << " seconds" << std::endl;
    #endif
    
    return 0;
}
