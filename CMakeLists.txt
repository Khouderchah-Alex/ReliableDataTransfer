cmake_minimum_required(VERSION 2.8)
project(RDT)

set(CMAKE_CXX_STANDARD 11)

option(RDT_BUILD_EXAMPLES "Build the RDT example programs" ON)
option(RDT_USE_ERROR "Use the RDT-provided ERROR macro, rather than providing your own" ON)
if(RDT_USE_ERROR)
  add_definitions(-DUSE_RDT_ERROR)
endif()

#
# Set source & headers
#
set(RDT_HEADER
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rdt_error.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rdt_structures.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/libRDT/rdt.h")
set(RDT_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rdt.cpp")

#
# Build subdirectories
#
add_subdirectory(src)

if(RDT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
