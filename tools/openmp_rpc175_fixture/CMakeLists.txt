cmake_minimum_required(VERSION 3.19)

project(openmp_rpc175_fixture LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OMP_SDK_DIR "" CACHE PATH "Path to a local open.mp SDK checkout")
if(NOT OMP_SDK_DIR)
	foreach(candidate
		"/home/chairman/Projects/omp-ipv6/open.mp/SDK"
		"/home/chairman/Projects/LastBedStanding/deps/omp-sdk")
		if(EXISTS "${candidate}/include/sdk.hpp")
			set(OMP_SDK_DIR "${candidate}")
			break()
		endif()
	endforeach()
endif()

if(NOT EXISTS "${OMP_SDK_DIR}/include/sdk.hpp")
	message(FATAL_ERROR
		"OMP_SDK_DIR must point to a local open.mp SDK checkout containing include/sdk.hpp")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR
		"The local open.mp server is i386. Configure this fixture with a 32-bit compiler "
		"(the supplied build.sh passes -m32).")
endif()

add_subdirectory("${OMP_SDK_DIR}" "${CMAKE_BINARY_DIR}/omp-sdk" EXCLUDE_FROM_ALL)

add_library(rpc175_fixture SHARED src/rpc175_fixture.cpp)
target_link_libraries(rpc175_fixture PRIVATE OMP-SDK)
target_include_directories(rpc175_fixture SYSTEM PRIVATE
	"${OMP_SDK_DIR}/include"
	"${OMP_SDK_DIR}/lib/glm/glm/.."
	"${OMP_SDK_DIR}/lib/robin-hood-hashing/src/include"
	"${OMP_SDK_DIR}/lib/span-lite/include"
	"${OMP_SDK_DIR}/lib/string-view-lite/include"
)

target_compile_options(rpc175_fixture PRIVATE
	$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-m32;-Wall;-Wextra;-Wpedantic;-Werror>
)
target_link_options(rpc175_fixture PRIVATE
	$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-m32>
)

set_target_properties(rpc175_fixture PROPERTIES
	PREFIX ""
	OUTPUT_NAME "rpc175_fixture"
	POSITION_INDEPENDENT_CODE ON
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN YES
)
