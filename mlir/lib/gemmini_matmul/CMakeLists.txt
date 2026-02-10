add_mlir_library(GemminiMatmulPass
  GemminiMatmulPass.cpp

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/lib/

  LINK_LIBS PUBLIC
)
