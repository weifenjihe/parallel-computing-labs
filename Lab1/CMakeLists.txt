cmake_minimum_required(VERSION 3.16)
project(Lab1SpMM LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(OpenMP REQUIRED COMPONENTS CXX)

add_library(lab1_core
  src/matrix.cpp
  src/partition.cpp
  src/spmm_serial.cpp
  src/spmm_parallel.cpp
  src/spmm_tiled.cpp
  src/verification.cpp
  src/benchmark.cpp
)
target_include_directories(lab1_core PUBLIC include)
target_link_libraries(lab1_core PUBLIC OpenMP::OpenMP_CXX)
add_executable(lab1_spmm src/main.cpp)
target_link_libraries(lab1_spmm PRIVATE lab1_core)
