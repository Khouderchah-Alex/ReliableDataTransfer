link_libraries(RDT)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../include/libRDT"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(simple_client simple_client.cpp ${RDT_SRC} ${RDT_HEADER})
target_compile_definitions(simple_client PUBLIC -DRDT_CLIENT)

add_executable(simple_server simple_server.cpp ${RDT_SRC} ${RDT_HEADER})
target_compile_definitions(simple_server PUBLIC -DRDT_SERVER)
