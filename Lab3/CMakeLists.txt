cmake_minimum_required(VERSION 3.16)
project(Lab3Histogram LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(OpenMP REQUIRED COMPONENTS CXX)

add_library(lab3_core
  src/input.cpp
  src/workspace.cpp
  src/histogram_serial.cpp
  src/histogram_critical.cpp
  src/histogram_atomic.cpp
  src/histogram_locks.cpp
  src/histogram_private.cpp
  src/verification.cpp
  src/benchmark.cpp
)
target_include_directories(lab3_core PUBLIC include)
target_link_libraries(lab3_core PUBLIC OpenMP::OpenMP_CXX)
add_executable(lab3_histogram src/main.cpp)
target_link_libraries(lab3_histogram PRIVATE lab3_core)
