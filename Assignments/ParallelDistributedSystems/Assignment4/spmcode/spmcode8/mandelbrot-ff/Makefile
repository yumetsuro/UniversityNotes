# 
# FF_ROOT     points to the FastFlow root directory (i.e.
#             the one containing the ff directory).
ifndef FF_ROOT 
FF_ROOT		= ${HOME}/fastflow
endif

CXX                = g++ -std=c++17
OPTFLAGS	   = -O3 -march=native
CXXFLAGS          += -Wall -DNO_DEFAULT_MAPPING -DBLOCKING_MODE
                      #-DFF_BOUNDED_BUFFER
INCLUDES	   = -I. -I../include -I$(FF_ROOT)
LIBS               = -pthread -lX11
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%:%.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< gfx.c $(LIBS)

all:$(TARGET)

nodisplay: CXXFLAGS += -DNO_DISPLAY
nodisplay: cleanall $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
