HTPATH = HybridTSS/
OVSPATH = OVS/
TMPATH = TupleMerge/
CTPATH = CutTSS/

CXX = g++
CXXFLAGS = -std=c++14 -pedantic -fpermissive -fopenmp -O3 \
           -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -fopenmp

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
GXX15 := $(shell which g++-15 2>/dev/null)
ifeq ($(GXX15),)
LIBOMP_PREFIX := $(shell brew --prefix libomp)
CXX = clang++
CXXFLAGS = -std=c++14 -pedantic -fpermissive -O3 \
           -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
           -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
LDFLAGS = -L$(LIBOMP_PREFIX)/lib -lomp
else
CXX = $(GXX15)
CXXFLAGS = -std=c++14 -pedantic -fpermissive -fopenmp -O3 \
           -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -fopenmp
endif
NPROC = $(shell sysctl -n hw.ncpu)
else
NPROC = $(shell nproc)
endif

# Debug build with sanitizers
ifdef DEBUG
CXXFLAGS += -g -O0 -DDEBUG
endif
ifdef ASAN
CXXFLAGS += -g -O0 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address
endif

OBJS = main.o cli.o HybridTSS.o SubHybridTSS.o TupleMergeOnline.o \
       TupleSpaceSearch.o cmap.o SlottedTable.o MapExtensions.o CutTSS.o

# Targets needed to bring the executable up to date
main: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o main $(OBJS)

# -----------------------------------------------------
main.o: main.cpp ElementaryClasses.h cli.h HybridTSS/HybridTSS.h CutTSS/CutTSS.h
	$(CXX) $(CXXFLAGS) -c main.cpp

cli.o: cli.cpp cli.h HybridTSS/HybridTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c cli.cpp

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
	rm -f results.csv

# --- GoogleTest test suite (CMake-based) ---
test:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX)
	cmake --build build -j$(NPROC)
	cd build && ctest --output-on-failure

test-verbose:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX)
	cmake --build build -j$(NPROC)
	cd build && ctest --output-on-failure --verbose

clean-test:
	rm -rf build

.PHONY: clean test test-verbose clean-test smoketest run

# Quick smoke test on generated small dataset (uses defaults)
smoketest: main
	uv run gen_testdata.py --rules 50 --packets 200 --seed 9 --out-dir Data
	./main -r Data/test_50 -p Data/test_50_trace --classifier hybrid --ht-loop 5 --ht-hash-inflation 8 --ht-seed 9 --metrics results.csv
	@grep -q "ht_binth" results.csv && echo "smoketest ok" || (echo "smoketest missing header" && false)

# Convenience run target (pass ARGS="...")
run: main
	./main $(ARGS)
