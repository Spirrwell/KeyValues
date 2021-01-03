cmake_minimum_required( VERSION 3.12.4 FATAL_ERROR )

project( KeyValues )

set( BUILD_TESTKV TRUE CACHE BOOL "Build testkv executable" )

set( KEYVALUES_SRC_FILES src/keyvalues.cpp )
set( KEYVALUES_INC_FILES include/keyvalues.hpp )

add_library( keyvalues STATIC ${KEYVALUES_SRC_FILES} ${KEYVALUES_INC_FILES} )
target_include_directories( keyvalues PUBLIC include/ )

set_target_properties( keyvalues
	PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
)

if ( ${BUILD_TESTKV} )
	set( TESTKV_SRC_FILES src/main.cpp )

	add_executable( testkv ${TESTKV_SRC_FILES} ${KEYVALUES_INC_FILES} )
	target_link_libraries( testkv keyvalues )
	target_include_directories( testkv PUBLIC include/ )

	set_target_properties( testkv
		PROPERTIES
			CXX_STANDARD 17
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS NO
	)
endif() # ${BUILD_TESTKV}