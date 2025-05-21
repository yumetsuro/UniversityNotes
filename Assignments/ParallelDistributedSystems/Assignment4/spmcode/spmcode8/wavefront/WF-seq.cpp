#include <vector>
#include <thread>
#include <iostream>
#include <cmath>
#include <cassert>

// Compute the product row * col as row * Transpose(column)
// It uses the lower triangular part of the matrix (containing zeros)
// to store transposed columns


double product(const double *matrix, const int n,
					  const int row, const int col) {
    double prod = 0.0;
    int len = col - row + 1;
    const double *row_ptr = matrix + row * n;
    const double *col_ptr = matrix + col * n;
    for (int i = 1; i < len; ++i) {
        prod += row_ptr[col - i] * col_ptr[row + i];
    }
    return prod;
}

void initMatrix(double *matrix, int n) {
	for (int i = 0; i < n; i++)
		matrix[i * n + i] = ((double) (i+1)) / n;
}

void wavefront(double *matrix, int n) {
	for (int k = 1; k < n; ++k) {
		//#pragma omp parallel for schedule(dynamic,1)
		for (int m = 0; m < n - k; ++m) {
			int row = m;
			int col = row + k;
			// cubic root of the dot product result
			double val = std::cbrt(product(matrix, n, row, col));			
			// save the result 
			matrix[row * n + col] = val;
			// save the result also in the transposed cell
			// to speedup next computations using cached row accesses
			matrix[col * n + row] = val;
		}
	}
}

void printMatrix(double *matrix, int n) {
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			std::cout << matrix[i * n + j] << " ";
		}
		std::cout << std::endl;
	}
}


int main(int argc, char **argv) {
    // Take matrix size as input
    if (argc < 2) {
		std::printf("use: %s matsize\n", argv[0]);
        return -1;
    }
	
	int n = std::stoi(argv[1]);
	
	// allocate contiguous matrix for better cache locality
    double *matrix = new double[n*n];
	assert(matrix);
	
	// intialize the major diagonal
	initMatrix(matrix, n);

	// compute the wavefront
	auto start = std::chrono::high_resolution_clock::now();
	wavefront(matrix, n);
	auto end = std::chrono::high_resolution_clock::now();	
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "Matrix size: " << n << " Time taken: " << duration.count() / 1000.0 << "ms Final cell: " << matrix[n-1] << "\n";

    return 0;
}
