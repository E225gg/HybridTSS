HTPATH = HybridTSS/
OVSPATH = OVS/
TMPATH = TupleMerge/
CTPATH = CutTSS/

CXX = g++
CXXFLAGS = -std=c++14 -pedantic -fpermissive -fopenmp -O3 \
           -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -fopenmp

# Debug build with sanitizers
ifdef DEBUG
CXXFLAGS += -g -O0 -DDEBUG
endif
ifdef ASAN
CXXFLAGS += -g -O0 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address
endif

OBJS = main.o HybridTSS.o SubHybridTSS.o TupleMergeOnline.o \
       TupleSpaceSearch.o cmap.o SlottedTable.o MapExtensions.o CutTSS.o

# Targets needed to bring the executable up to date
main: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o main $(OBJS)

# -----------------------------------------------------
main.o: main.cpp ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c main.cpp

# ** HybridTSS **
HybridTSS.o: $(HTPATH)HybridTSS.h $(HTPATH)HybridTSS.cpp $(HTPATH)SubHybridTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(HTPATH)HybridTSS.cpp

SubHybridTSS.o: $(HTPATH)SubHybridTSS.cpp $(HTPATH)SubHybridTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(HTPATH)SubHybridTSS.cpp

# ** TupleMerge **
TupleMergeOnline.o: $(TMPATH)TupleMergeOnline.cpp $(TMPATH)TupleMergeOnline.h $(TMPATH)SlottedTable.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(TMPATH)TupleMergeOnline.cpp

SlottedTable.o: $(TMPATH)SlottedTable.cpp $(TMPATH)SlottedTable.h $(OVSPATH)TupleSpaceSearch.h
	$(CXX) $(CXXFLAGS) -c $(TMPATH)SlottedTable.cpp

# ** TSS **
cmap.o: $(OVSPATH)cmap.cpp $(OVSPATH)cmap.h $(OVSPATH)hash.h ElementaryClasses.h $(OVSPATH)random.h
	$(CXX) $(CXXFLAGS) -c  $(OVSPATH)cmap.cpp

TupleSpaceSearch.o: $(OVSPATH)TupleSpaceSearch.cpp $(OVSPATH)TupleSpaceSearch.h ElementaryClasses.h $(OVSPATH)cmap.h $(OVSPATH)hash.h
	$(CXX) $(CXXFLAGS) -c $(OVSPATH)TupleSpaceSearch.cpp

MapExtensions.o: $(OVSPATH)MapExtensions.cpp $(OVSPATH)MapExtensions.h
	$(CXX) $(CXXFLAGS) -c $(OVSPATH)MapExtensions.cpp

# ** CutTSS **
CutTSS.o: $(CTPATH)CutTSS.h $(CTPATH)CutTSS.cpp ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(CTPATH)CutTSS.cpp

clean:
	rm -f *.o
	rm -f main

.PHONY: clean