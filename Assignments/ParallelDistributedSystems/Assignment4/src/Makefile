CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I$(FF_ROOT) -pthread
LDFLAGS = -pthread

# Define FastFlow root directory - adjust if needed
# FastFlow is located at Assignment4/src/fastflow
FF_ROOT = fastflow

# Target and source files
TARGETS = mergesort_ff mergesort_ff_clean
SRC1 = mergesort_ff.cpp
SRC2 = mergesort_ff_clean.cpp

all: $(TARGETS)

mergesort_ff: $(SRC1)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

mergesort_ff_clean: $(SRC2)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
