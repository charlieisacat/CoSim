cmake_minimum_required(VERSION 3.20)

project(gemmini_interface LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# We need the MLIR CRunnerUtils header to compile the MLIR memref ABI wrapper.
# Prefer finding MLIR via MLIR_DIR; fallback to an explicit include dir.
find_package(MLIR CONFIG QUIET)

if(MLIR_FOUND)
  message(STATUS "Found MLIRConfig.cmake in: ${MLIR_DIR}")
  include_directories(${MLIR_INCLUDE_DIRS})
else()
  if(NOT DEFINED MLIR_INCLUDE_DIR)
    message(FATAL_ERROR "MLIR not found. Set -DMLIR_DIR=<path to MLIRConfig.cmake> or -DMLIR_INCLUDE_DIR=<path to mlir include>.")
  endif()
  include_directories(${MLIR_INCLUDE_DIR})
endif()

add_library(gemmini_interface STATIC
  mlir_gemmini_runtime.cpp
)

target_include_directories(gemmini_interface
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# The wrapper and headers are C++.
target_compile_features(gemmini_interface PUBLIC cxx_std_17)

# # gemmini.h uses math functions like sqrt(). On Linux these live in libm.
# target_link_libraries(gemmini_interface PUBLIC m)
