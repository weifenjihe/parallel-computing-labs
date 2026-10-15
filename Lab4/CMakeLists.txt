cmake_minimum_required(VERSION 3.16)
project(Lab4SpMM LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(MPI REQUIRED COMPONENTS CXX)

add_library(lab4_core
  src/matrix.cpp
  src/distribution.cpp
  src/communication_plan.cpp
  src/spmm_tiled.cpp
  src/spmm_full.cpp
  src/spmm_halo.cpp
  src/spmm_overlap.cpp
  src/verification.cpp
  src/benchmark.cpp
)
target_include_directories(lab4_core PUBLIC include)
target_link_libraries(lab4_core PUBLIC MPI::MPI_CXX)
add_executable(lab4_spmm src/main.cpp)
target_link_libraries(lab4_spmm PRIVATE lab4_core)
