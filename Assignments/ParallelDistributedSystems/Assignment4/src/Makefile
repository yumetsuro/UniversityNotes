CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread --ffast-math --march=native
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

# Payload size configurations for testing
PAYLOAD_SIZE ?= 8

all: $(TARGETS)

mergesort_ff: $(SRC1)
	$(CXX) $(CXXFLAGS) -DMAX_PAYLOAD_SIZE=$(PAYLOAD_SIZE) -o $@ $< $(LDFLAGS)

mergesort_ff_modular: $(SRC5)
	$(CXX) $(CXXFLAGS) -DMAX_PAYLOAD_SIZE=$(PAYLOAD_SIZE) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean help
