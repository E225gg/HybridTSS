HTPATH = HybridTSS/
OVSPATH = OVS/
TMPATH = TupleMerge/
CTPATH = CutTSS/

CXX = g++
# CXXFLAGS = -g -std=c++14 -pedantic -fpermissive -O3
CXXFLAGS = -g -std=c++14 -pedantic -fpermissive -fopenmp -O3
LDFLAGS = -fopenmp


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