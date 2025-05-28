CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread -ffast-math -march=native
LDFLAGS = -pthread

# MPI configuration
MPICXX = mpicxx
MPI_CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
MPI_LDFLAGS = -pthread

# Define FastFlow root directory - adjust if needed
# FastFlow is located at Assignment4/src/fastflow
FF_ROOT = ../fastflow

# clean before running the makefile
clean_before_run:
	rm -f mergesort_ff_mpi *.o
# Target and source files
TARGETS = clean_before_run mergesort_ff_mpi 
SRC = main.cpp

# Payload size configurations for testing
PAYLOAD_SIZE ?= 256
DEBUG ?= 0
SIMULATE_TRANSFER ?= 0

all: $(TARGETS)

mergesort_ff_mpi: $(SRC)
	$(MPICXX) $(MPI_CXXFLAGS) -DMAX_PAYLOAD_SIZE=$(PAYLOAD_SIZE) -DDEBUG=$(DEBUG) -DSIMULATE_TRANSFER=$(SIMULATE_TRANSFER) -o $@ $< $(MPI_LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean help
