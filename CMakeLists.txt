cmake_minimum_required(VERSION 3.20)
project(libdie LANGUAGES C VERSION 0.2)

include(CTest)

# Default to enable link-time-optimizations if supported.                                                                               
include(CheckIPOSupported)                                                                                                              
check_ipo_supported(RESULT lto_supported)                                                                                               
if (lto_supported)                                                                                                                      
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION true)                                                                                    
endif()

add_library(die
	libdie.c
	parse_exp.c
	list_defs.c
	parse_operation.c
	string_ops.c)

target_link_libraries(die PRIVATE m)

# Tests.
add_executable(libdie_tests
	tests/main.test.c
	tests/libdie.test.c
	tests/string_ops.test.c
	list/tests/list.test.c)
target_link_libraries(libdie_tests PRIVATE die)

add_test(NAME libdie_tests COMMAND libdie_tests)
