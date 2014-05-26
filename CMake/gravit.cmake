FILE(WRITE "${CMAKE_BINARY_DIR}/CMakeDefines.h" "#define CMAKE_BUILD_DIR \"${CMAKE_BINARY_DIR}\"\n")
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})
SET(GVT_BINARY_DIR ${CMAKE_BINARY_DIR})
SET(GVT_DIR ${PROJECT_BASE_DIR})
MACRO(CONFIGURE_GVT)
	SET(LIBRARY_OUTPUT_PATH ${GVT_BINARY_DIR})
	SET(EXECUTABLE_OUTPUT_PATH ${GVT_BINARY_DIR})
	LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

	IF (GVT_ICC)
		INCLUDE(${PROJECT_BASE_DIR}/CMake/icc.cmake)
	ELSE()
		INCLUDE(${PROJECT_BASE_DIR}/CMake/gcc.cmake)
	ENDIF()

	IF (${GVT_XEON_TARGET} STREQUAL "AVX2")
		ADD_DEFINITIONS(-DGVT_SPMD_WIDTH=8)
		ADD_DEFINITIONS(-DGVT_TARGET_AVX2=1)
		SET(GVT_ISPC_TARGET "avx2")
	ELSEIF (${GVT_XEON_TARGET} STREQUAL "AVX")
		ADD_DEFINITIONS(-DGVT_SPMD_WIDTH=8)
		ADD_DEFINITIONS(-DGVT_TARGET_AVX=1)
		SET(GVT_ISPC_TARGET "avx")
	ELSEIF (${GVT_XEON_TARGET} STREQUAL "SSE")
		ADD_DEFINITIONS(-DGVT_SPMD_WIDTH=4)
		ADD_DEFINITIONS(-DGVT_TARGET_SSE=1)
		SET(GVT_ISPC_TARGET "sse4")
	ELSE()
		MESSAGE("unknown GVT_XEON_TARGET '${GVT_XEON_TARGET}'")
	ENDIF()
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GVT_ARCH_FLAGS__${GVT_XEON_TARGET}}")
	
	IF (GVT_MPI)
		ADD_DEFINITIONS(-DGVT_MPI=1)
	ENDIF()
ENDMACRO()
