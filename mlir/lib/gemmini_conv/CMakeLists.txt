add_mlir_library(GemminiConvPass
  GemminiConvPass.cpp

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/lib/

  LINK_LIBS PUBLIC
)
