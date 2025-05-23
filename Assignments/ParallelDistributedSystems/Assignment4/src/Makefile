CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
LDFLAGS = -pthread

# MPI configuration
MPICXX = mpic++
MPI_CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
MPI_LDFLAGS = -pthread

# Define FastFlow root directory - adjust if needed
# FastFlow is located at Assignment4/src/fastflow
FF_ROOT = fastflow

# Target and source files
TARGETS = mergesort_ff mergesort_ff_clean mergesort_mpi_ff mergesort_ff_improved mergesort_ff_pipeline mergesort_mpi_simple
SRC1 = mergesort_ff.cpp
SRC2 = mergesort_ff_clean.cpp
SRC3 = mergesort_mpi_ff.cpp
SRC4 = mergesort_ff_improved.cpp
SRC5 = mergesort_ff_pipeline.cpp
SRC6 = mergesort_mpi_simple.cpp

all: $(TARGETS)

mergesort_ff: $(SRC1)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_ff_clean: $(SRC2)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_mpi_ff: $(SRC3)
	$(MPICXX) $(MPI_CXXFLAGS) -o $@ $< $(MPI_LDFLAGS)

mergesort_ff_improved: $(SRC4)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_ff_pipeline: $(SRC5)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_mpi_simple: $(SRC6)
	$(MPICXX) $(MPI_CXXFLAGS) -o $@ $< $(MPI_LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
