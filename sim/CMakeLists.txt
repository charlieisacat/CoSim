
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-O3)
find_package(OpenMP)

# Find LLVM
find_package(LLVM 22 REQUIRED CONFIG)

if(LLVM_FOUND)
    message(STATUS "LLVM found: ${LLVM_INCLUDE_DIRS}")
    include_directories(${LLVM_INCLUDE_DIRS})
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(AddLLVM)
else()
    message(FATAL_ERROR "LLVM not found")
endif()

link_directories(${CMAKE_SOURCE_DIR}/DRAMSim2)
find_package(yaml-cpp REQUIRED)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# Add the executable
add_executable(my_program ${SOURCES})

add_custom_command(
  TARGET my_program PRE_BUILD
  COMMAND make libdramsim.so
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/DRAMSim2/)

# Link with Boost libraries
target_link_libraries(my_program ${Boost_LIBRARIES})

message(STATUS "Found LLVM version: ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader native passes)
target_link_libraries(my_program ${llvm_libs})
target_link_libraries(my_program yaml-cpp)
target_link_libraries(my_program dramsim OpenMP::OpenMP_CXX)
