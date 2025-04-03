CC = g++
CFLAGS = -O3 -Wall -std=c++17 -march=native
INCLUDES	   = -I. -I./include
LDFLAGS = -lpthread

# Targets
all: collatz_naive collatz_parallel

collatz_naive: collatz_naive.cpp
	$(CC) $(CFLAGS) -o $@ $<

collatz_parallel: collatz_parallel.cpp
	$(CC) $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f collatz_naive collatz_parallel *.o

.PHONY: all clean
