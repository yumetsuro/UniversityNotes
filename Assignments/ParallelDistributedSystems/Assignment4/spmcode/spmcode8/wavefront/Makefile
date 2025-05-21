# 
# FF_ROOT     points to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT 
FF_ROOT		= ${HOME}/fastflow
endif

CXX                = g++ -std=c++20
OPTFLAGS	   = -O3 -DNDEBUG -march=native -ffast-math
CXXFLAGS          += -Wall #-DNO_DEFAULT_MAPPING
                           #-DBLOCKING_MODE -DFF_BOUNDED_BUFFER

INCLUDES	   = -I. -I $(FF_ROOT)
LIBS               = -pthread
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
