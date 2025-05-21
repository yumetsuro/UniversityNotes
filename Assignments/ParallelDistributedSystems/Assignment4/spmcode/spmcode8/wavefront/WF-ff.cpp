#include <vector>
#include <thread>
#include <iostream>
#include <cmath>
#include <cassert>

#include <ff/ff.hpp>


// Compute the product row * col as row * Transpose(column)
// It uses the lower triangular part of the matrix (containing zeros)
// to store transposed columns


using namespace ff;
struct task_t {
	int start;
	int stop;
	int iter;
};

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

void compute_iter(double *matrix, const int n,
				  const int start, const int stop, const int iter) {
	for(int m=start;m<stop;++m) {
		const int row=m;
		const int col=row+iter;
		// cubic root of the dot product result
		double val = std::cbrt(product(matrix, n, row, col));
		// save the result 
		matrix[row * n + col] = val;
		// save the result also in the transposed cell
		// to speedup next computations using cached row accesses
		matrix[col * n + row] = val;
	}
}

struct Master:ff_monode_t<task_t> {
	Master(double *matrix, int n, int pardegree):
		matrix(matrix), n(n),pardegree(pardegree),taskv(pardegree) {	
	}

	// sends tasks to workers 
	void send_tasks(int &start, int &stop) {
		const size_t chunksize = (n-iter) / pardegree;
		ssize_t more           = (n-iter) - chunksize*pardegree;

		// send one chunk to all Workers
		for(int i=0; i<(pardegree-1); ++i) {
			start = stop;
			stop  = start + chunksize + (more>0 ? 1:0);
			--more;
			
			taskv[i] = {start, stop, iter};

			ff_send_out_to(&taskv[i], i);
		}
		nsent=(pardegree-1);
		// if there are more Worker than task, terminate one Worker (the last one)
		if ((n-iter)==pardegree)  {
			ff_send_out_to(EOS, pardegree-1);
			--pardegree;
		}
		// chunk reserved for the Emitter
		start = stop;
		stop  = start + chunksize + (more>0 ? 1:0);
	}
		
	task_t* svc(task_t* in) {
		int start=0;
		int stop=0;
		if (!in) {  // first call
			send_tasks(start,stop);
			compute_iter(matrix, n, start,stop,iter);			
			return pardegree?GO_ON:EOS;
		}

		if (!--nsent) {
			++iter; // starting next iteration

			if (pardegree==0) {
				assert(iter == (n-1));
				compute_iter(matrix, n, 0, n-iter, iter);				
				return EOS;
			}
				
			send_tasks(start,stop);
			compute_iter(matrix, n, start,stop,iter);
		}
		return GO_ON;
	}
	
	double *matrix;              // the matrix
	int n;                       // number of columns/rows
	int pardegree;               // initial number of workers
	int iter=1;                  // current iterations (from 1 to n)

	int nsent=0;                 // n. of active tasks sent to Workers
	std::vector<task_t> taskv;	 // memory needed for the tasks
};

struct Worker:ff_minode_t<task_t> {
	Worker(double *matrix, int n):matrix(matrix),n(n) {}
	task_t* svc(task_t* in) {
		compute_iter(matrix, n, in->start,in->stop,in->iter);
		return in; // notify Emitter, we've completed the task
	}
	double *matrix;  // the matrix
	int     n;       // n. of columns/rows
};

void wavefront(double *matrix, int n, int pardegree, bool blocking, bool nomapping) {
	if (pardegree==1) {  // sequential computation
		for (int k = 1; k < n; ++k) {
			for (int m = 0; m < n - k; ++m) {
				int row = m;
				int col = row + k;
				
				double val = std::cbrt(product(matrix, n, row, col));
				matrix[row * n + col] = val;
				matrix[col * n + row] = val;
			}
		}
		return;
	}
	// create a farm 
	ff_farm farm;
	Master m(matrix, n, pardegree);
	farm.add_emitter(&m);
    std::vector<ff_node*> W;
	for(int i=0;i<pardegree;++i) W.push_back(new Worker(matrix,n));
	farm.add_workers(W);
	farm.wrap_around();
	farm.cleanup_workers();
	farm.blocking_mode(blocking);      // set/reset blocking mode
	if (nomapping) farm.no_mapping();  // reset thread affinity
	if (farm.run_and_wait_end()<0){
		error("running the farm\n");
	}
}

int main(int argc, char **argv) {
    // Take matrix size as input
    if (argc < 3) {
		std::printf("use: %s size pardegree [blocking] [nomapping]\n", argv[0]);
		std::printf("example: %s 1024 10 1 1    # means BLOCKING_MODE and NO_DEFAULT_MAPPING\n", argv[0]); 
		return -1;
    }
	bool blocking  = false;
	bool nomapping = false;
	long n         = std::stol(argv[1]);
	long pardegree = std::stol(argv[2]);

	if (argc == 4) {
		blocking = std::stoi(argv[3])?true:false;
	}
	if (argc == 5) {
		nomapping = std::stoi(argv[4])?true:false;
	}	
	if (pardegree <=0) {
		std::cout << "pardegree should be greater than zero\n";
		return -1;
	}
	// allocate contiguous matrix for better cache locality
    double *matrix = new double[n*n];
	assert(matrix);
	
	// intialize the major diagonal	
	initMatrix(matrix, n);

	// compute the wavefront
	auto start = std::chrono::high_resolution_clock::now();
	wavefront(matrix, n, std::min(pardegree,n), blocking, nomapping);
	auto end = std::chrono::high_resolution_clock::now();	
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "Matrix size: " << n << " Time taken: " << duration.count() / 1000.0 << "ms Final cell: " << matrix[n-1] << "\n";
	
    return 0;
}
