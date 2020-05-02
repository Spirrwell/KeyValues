cmake_minimum_required( VERSION 3.12.4 FATAL_ERROR )
set( CMAKE_CXX_STANDARD 17 )

FILE( GLOB SRC_FILES src/*.cpp src/*.hpp )

project( KeyValues )
add_executable( keyvalues ${SRC_FILES} )
target_link_libraries( keyvalues )