get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
get_property(translation_libs GLOBAL PROPERTY MLIR_ALL_LIBS)

# message(STATUS "Dialect libs: ${dialect_libs}")
# message(STATUS "Conversion libs: ${conversion_libs}")
# message(STATUS "All libs: ${translation_libs}")

set (LIBS
    ${dialect_libs}
    ${conversion_libs}
    ${translation_libs}
    GemminiMatmulPass
    GemminiConvPass
)

add_llvm_executable(customize customize.cpp)

llvm_update_compile_flags(customize)
target_link_libraries(customize PRIVATE ${LIBS})

mlir_check_all_link_libraries(customize)