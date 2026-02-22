cmake_minimum_required(VERSION 3.14)
project(HybridTSS_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags to match the main makefile
add_compile_options(-fpermissive -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)

# OpenMP (required by HybridTSS)
find_package(OpenMP REQUIRED)

# --- GoogleTest via FetchContent ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# Prevent GoogleTest from overriding our compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Project source library (everything except main.cpp) ---
add_library(hybridtss_lib STATIC
  OVS/cmap.cpp
  OVS/TupleSpaceSearch.cpp
  OVS/MapExtensions.cpp
  TupleMerge/TupleMergeOnline.cpp
  TupleMerge/SlottedTable.cpp
  CutTSS/CutTSS.cpp
  HybridTSS/HybridTSS.cpp
  HybridTSS/SubHybridTSS.cpp
)
target_include_directories(hybridtss_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(hybridtss_lib PUBLIC OpenMP::OpenMP_CXX)

# --- Tests ---
add_subdirectory(tests)
