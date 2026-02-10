cmake_minimum_required(VERSION 3.20.0)

project(mlir-customization LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_DEPS ON)

find_package(MLIR REQUIRED CONFIG)

message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})
set(LLVM_PROJECT_DIR "/staff/haoxiaoyu/mlir_project/llvm-project")
set(MLIR_LINK_DIR "/staff/haoxiaoyu/mlir_project/llvm-project/build/externals/llvm-project/install/lib")

message(STATUS "link dir ${MLIR_LINK_DIR}")

include(AddLLVM)
include(TableGen)

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
include(AddMLIR)
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${LLVM_PROJECT_DIR})
include_directories(${PROJECT_BINARY_DIR})

add_subdirectory(lib)
add_subdirectory(tools)