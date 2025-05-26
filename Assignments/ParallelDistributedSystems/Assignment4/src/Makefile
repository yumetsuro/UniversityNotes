CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
LDFLAGS = -pthread

# MPI configuration
MPICXX = mpicxx
MPI_CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
MPI_LDFLAGS = -pthread

# Define FastFlow root directory - adjust if needed
# FastFlow is located at Assignment4/src/fastflow
FF_ROOT = fastflow

# Target and source files
TARGETS = mergesort_ff mergesort_mpi_ff mergesort_ff_naive mergesort_ff_modular
SRC1 = mergesort_ff.cpp
SRC2 = mergesort_mpi_ff.cpp
SRC4 = mergesort_ff_naive.cpp
SRC5 = main.cpp

all: $(TARGETS)

mergesort_ff: $(SRC1)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_mpi_ff: $(SRC2)
	$(MPICXX) $(MPI_CXXFLAGS) -o $@ $< $(MPI_LDFLAGS)

mergesort_ff_naive: $(SRC4)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_ff_modular: $(SRC5)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
