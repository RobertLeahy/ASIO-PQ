cmake_minimum_required(VERSION 2.6)
project(ASIO-PQ LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(add_linker_options)
	foreach(arg ${ARGV})
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${arg}")
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES}" PARENT_SCOPE)
	endforeach(arg)
endfunction()

#	Where our project's headers live
include_directories(include)

#	We need libpq
set(PostgreSQL_ADDITIONAL_VERSIONS "9.5")
find_package(PostgreSQL REQUIRED)
include_directories(${PostgreSQL_INCLUDE_DIRS})
link_directories(${PostgreSQL_LIBRARY_DIRS})

#	General command line arguments to the compiler that should be present
#	on all platforms
add_compile_options(-std=c++1z)
if(DEFINED CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_options(-O0 -g -fno-inline -fno-omit-frame-pointer -Wall -Wpedantic -Wextra -Werror)
	#	Address Sanitizer (non-Windows only)
	if(NOT WIN32)
		if(NOT DEFINED USE_ADDRESS_SANITIZER OR USE_ADDRESS_SANITIZER)
			add_compile_options(-fsanitize=address)
			add_linker_options(-fsanitize=address)
		endif()
	endif()
	#	Enable all warnings except a choice few on Clang
	if (DEFINED CMAKE_CXX_COMPILER_ID AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		add_compile_options(-Weverything)
		#	We don't care about C++98 compatibility: This is a C++14
		#	library
		add_compile_options(-Wno-c++98-compat)
		#	These warnings are just noise
		add_compile_options(-Wno-weak-vtables -Wno-padded -Wno-switch-enum)
	endif()
else()
	add_compile_options(-O2)
endif()

if(NOT DEFINED USE_BOOST_FUTURE)
	set(USE_BOOST_FUTURE 1)
endif()

if(NOT DEFINED USE_BOOST_ASIO)
	set(USE_BOOST_ASIO 1)
endif()

configure_file(src/configure.hpp.in include/asiopq/configure.hpp)

add_library(asiopq SHARED
	src/connect.cpp
	src/connection.cpp
	src/exception.cpp
	src/operation.cpp
	src/query.cpp
	src/reset.cpp
)
target_link_libraries(asiopq ${PostgreSQL_LIBRARIES})
if(USE_BOOST_FUTURE)
	target_link_libraries(asiopq boost_thread)
endif()
if(USE_BOOST_ASIO OR USE_BOOST_FUTURE)
	target_link_libraries(asiopq boost_system)
endif()
if(WIN32)
	target_link_libraries(asiopq ws2_32)
else()
	#	Using non-Boost ASIO on Linux seems to require
	#	linking against libpthread
	if(NOT USE_BOOST_ASIO)
		target_link_libraries(asiopq pthread)
	endif()
endif()

if((DEFINED CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug") OR (DEFINED BUILD_TESTS AND BUILD_TESTS))
	if(NOT DEFINED PQ_HOST_ADDR)
		set(PQ_HOST_ADDR 127.0.0.1)
	endif()
	if(NOT DEFINED PQ_PORT)
		set(PQ_PORT 5432)
	endif()
	if(NOT DEFINED PQ_DATABASE_NAME)
		set(PQ_DATABASE_NAME postgres)
	endif()
	if(NOT DEFINED PQ_USERNAME)
		set(PQ_USERNAME postgres)
	endif()
	if(NOT DEFINED PQ_PASSWORD)
		set(PQ_PASSWORD "")
	endif()
	configure_file(src/test/login.hpp.in src/test/login.hpp ESCAPE_QUOTES)
	add_executable(tests
		src/test/integration.cpp
		src/test/main.cpp
		src/test/scope.cpp
	)
	target_link_libraries(tests asiopq)
	#	Catch triggers -Wexit-time-destructors like crazy
	#	so we turn it off only for the tests
	if (DEFINED CMAKE_CXX_COMPILER_ID AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND DEFINED CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_compile_options(tests PRIVATE -Wno-exit-time-destructors)
	endif()
	add_custom_target(tests_run ALL
		COMMAND tests
		DEPENDS tests
		WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		COMMENT "Run test suite"
	)
endif()
