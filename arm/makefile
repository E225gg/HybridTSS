# ==== Makefile (clang++ + OpenMP on macOS ARM/Linux) ====

HTPATH = HybridTSS/
OVSPATH = OVS/
TMPATH = TupleMerge/
CTPATH = CutTSS/

# Compiler
CXX ?= clang++

# Base flags
BASE_CXXFLAGS = -g -std=c++17 -pedantic -fpermissive -O3

# Detect compiler
CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
CXX_IS_CLANG := $(findstring clang,$(CXX_VERSION))

# OpenMP flags
OPENMP_CFLAGS :=
OPENMP_LDFLAGS :=

# If not clang (e.g., g++), just use -fopenmp
ifeq ($(CXX_IS_CLANG),)
  OPENMP_CFLAGS  += -fopenmp
  OPENMP_LDFLAGS += -fopenmp
else
  # Clang: prefer Homebrew libomp on macOS; otherwise try generic flags (Linux or custom installs)
  LIBOMP_PREFIX ?= $(shell brew --prefix libomp 2>/dev/null)
  ifneq ($(LIBOMP_PREFIX),)
    OPENMP_CFLAGS  += -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
    OPENMP_LDFLAGS += -L$(LIBOMP_PREFIX)/lib -lomp -Wl,-rpath,$(LIBOMP_PREFIX)/lib
  else
    # Fallback for systems where clang can link libomp directly
    OPENMP_CFLAGS  += -fopenmp
    OPENMP_LDFLAGS += -fopenmp
    $(warning libomp not found via Homebrew; ensure libomp is installed and on library path)
  endif
endif

CXXFLAGS = $(BASE_CXXFLAGS) $(OPENMP_CFLAGS)
LDFLAGS  = $(OPENMP_LDFLAGS)
# remove warning
CXXFLAGS += -Wno-inconsistent-missing-override


# Optional: silence missing 'override' warnings. Comment out if undesired.
# CXXFLAGS += -Wno-inconsistent-missing-override

# ===== Targets =====

main: main.o HybridTSS.o SubHybridTSS.o TupleMergeOnline.o TupleSpaceSearch.o cmap.o SlottedTable.o MapExtensions.o CutTSS.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

# ---- main ----
main.o: main.cpp ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- HybridTSS ----
HybridTSS.o: $(HTPATH)HybridTSS.cpp $(HTPATH)HybridTSS.h ElementaryClasses.h $(HTPATH)SubHybridTSS.h
	$(CXX) $(CXXFLAGS) -c $(HTPATH)HybridTSS.cpp -o $@

SubHybridTSS.o: $(HTPATH)SubHybridTSS.cpp $(HTPATH)SubHybridTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(HTPATH)SubHybridTSS.cpp -o $@

# ---- TupleMerge ----
TupleMergeOnline.o: $(TMPATH)TupleMergeOnline.cpp $(TMPATH)TupleMergeOnline.h $(TMPATH)SlottedTable.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(TMPATH)TupleMergeOnline.cpp -o $@

SlottedTable.o: $(TMPATH)SlottedTable.cpp $(TMPATH)SlottedTable.h $(OVSPATH)TupleSpaceSearch.h
	$(CXX) $(CXXFLAGS) -c $(TMPATH)SlottedTable.cpp -o $@

# ---- TSS ----
cmap.o: $(OVSPATH)cmap.cpp $(OVSPATH)cmap.h $(OVSPATH)hash.h ElementaryClasses.h $(OVSPATH)random.h
	$(CXX) $(CXXFLAGS) -c $(OVSPATH)cmap.cpp -o $@

TupleSpaceSearch.o: $(OVSPATH)TupleSpaceSearch.cpp $(OVSPATH)TupleSpaceSearch.h ElementaryClasses.h $(OVSPATH)cmap.h $(OVSPATH)hash.h
	$(CXX) $(CXXFLAGS) -c $(OVSPATH)TupleSpaceSearch.cpp -o $@

MapExtensions.o: $(OVSPATH)MapExtensions.cpp $(OVSPATH)MapExtensions.h
	$(CXX) $(CXXFLAGS) -c $(OVSPATH)MapExtensions.cpp -o $@

# ---- CutTSS ----
CutTSS.o: $(CTPATH)CutTSS.cpp $(CTPATH)CutTSS.h ElementaryClasses.h
	$(CXX) $(CXXFLAGS) -c $(CTPATH)CutTSS.cpp -o $@

# ---- Utilities ----
clean:
	rm -f *.o main
