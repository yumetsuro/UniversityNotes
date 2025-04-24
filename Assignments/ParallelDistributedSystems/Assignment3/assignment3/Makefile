CXX		= g++ -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	+= -Wall 

LDFLAGS 	= -fopenmp
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= minizseq minizpar

.PHONY: all clean cleanall
.SUFFIXES: .cpp 

minizpar: minizpar.cpp cmdline.hpp utility_par.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

minizseq: minizseq.cpp cmdline.hpp utility.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c

all		: $(TARGETS)

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *~



