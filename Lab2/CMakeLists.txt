cmake_minimum_required(VERSION 3.16)
project(Lab2Locality LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(OpenMP REQUIRED COMPONENTS CXX)

add_library(lab2_core
  src/matrix.cpp
  src/transpose_serial.cpp
  src/transpose_parallel.cpp
  src/transpose_tiled.cpp
  src/verification.cpp
  src/benchmark.cpp
)
target_include_directories(lab2_core PUBLIC include)
target_link_libraries(lab2_core PUBLIC OpenMP::OpenMP_CXX)
add_executable(lab2_locality src/main.cpp)
target_link_libraries(lab2_locality PRIVATE lab2_core)
