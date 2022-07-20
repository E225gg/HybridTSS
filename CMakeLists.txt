cmake_minimum_required(VERSION 3.0)
project(HybridTSSv1_2)

set(CMAKE_CXX_STANDARD 14)

add_executable(HybridTSSv1_2 main.cpp ElementaryClasses.h HybridTSS/HybridTSS.cpp HybridTSS/HybridTSS.h HybridTSS/SubHybridTSS.cpp HybridTSS/SubHybridTSS.h OVS/cmap.cpp OVS/cmap.h OVS/hash.h OVS/random.h OVS/TupleSpaceSearch.cpp OVS/TupleSpaceSearch.h OVS/MapExtensions.cpp OVS/MapExtensions.h TupleMerge/SlottedTable.cpp TupleMerge/SlottedTable.h TupleMerge/TupleMergeOnline.cpp TupleMerge/TupleMergeOnline.h CutTSS/CutTSS.cpp CutTSS/CutTSS.h)
