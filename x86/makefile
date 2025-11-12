HTPATH = HybridTSS/
OVSPATH = OVS/
TMPATH = TupleMerge/
CTPATH = CutTSS/

CXX ?= g++

BASE_CXXFLAGS = -g -std=c++17 -pedantic -fpermissive -O3

CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
CXX_IS_CLANG := $(findstring clang,$(CXX_VERSION))
OPENMP_CFLAGS :=
OPENMP_LDFLAGS :=

ifeq ($(CXX_IS_CLANG),)
OPENMP_CFLAGS += -fopenmp
OPENMP_LDFLAGS += -fopenmp
else
LIBOMP_PREFIX ?= $(shell brew --prefix libomp 2>/dev/null)
ifneq ($(LIBOMP_PREFIX),)
OPENMP_CFLAGS += -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
OPENMP_LDFLAGS += -L$(LIBOMP_PREFIX)/lib -lomp
else
$(warning Unable to find libomp; install with `brew install libomp` or set LIBOMP_PREFIX=/path/to/libomp)
endif
endif

CXXFLAGS = $(BASE_CXXFLAGS) $(OPENMP_CFLAGS)
LDFLAGS = $(OPENMP_LDFLAGS)


# Targets needed to bring the executable up to date
main: main.o HybridTSS.o TupleMergeOnline.o TupleSpaceSearch.o cmap.o SlottedTable.o MapExtensions.o SubHybridTSS.o CutTSS.o
	$(CXX) $(CXXFLAGS) -o main *.o

# -----------------------------------------------------
main.o: main.cpp ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c main.cpp

# ** HybridTSS **
HybridTSS.o: $(HTPATH)HybridTSS.h $(HTPATH)HybridTSS.cpp $(HTPATH)SubHybridTSS.cpp $(HTPATH)SubHybridTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(HTPATH)HybridTSS.cpp $(HTPATH)SubHybridTSS.cpp

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
	rm main
