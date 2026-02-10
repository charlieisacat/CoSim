cmake_minimum_required(VERSION 3.20)

project(manual_interface LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(manual_interface STATIC
  interface.c
)

target_include_directories(manual_interface
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
