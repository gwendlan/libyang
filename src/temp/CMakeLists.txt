cmake_minimum_required (VERSION 2.6)
project (Main)


SET(CMAKE_CXX_COMPILER "/usr/bin/g++")
SET(CMAKE_C_COMPILER "/usr/bin/gcc")

SET(CMAKE_CXX_FLAGS "-Wall -Wextra -g")
SET(CMAKE_C_FLAGS "-Wall -Wextra -Wno-missing-field-initializers -std=c99 -g3 -O0 -fPIC -fvisibility=hidden")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
ENABLE_TESTING()

add_executable(test_wrapper test_wrapper.cpp new.c)
add_executable(test_node_rel test_node_rel.cpp new.c)
add_executable(test_print_node test_print_node.cpp new.c)
add_executable(test_print_line test_print_line.cpp new.c)
add_executable(test_node_break test_node_break.cpp new.c)
add_executable(test_keyword_stmt test_keyword_stmt.cpp new.c)
add_executable(test_print_nodes test_print_nodes.cpp new.c)
add_executable(test_unified test_unified.cpp new.c)
#add_executable(main main.cpp new.c)

ADD_TEST(wrapper test_wrapper)
ADD_TEST(node_rel test_node_rel)
ADD_TEST(print_node test_print_node)
ADD_TEST(print_line test_print_line)
ADD_TEST(node_break test_node_break)
ADD_TEST(keyword_stmt test_keyword_stmt)
ADD_TEST(print_nodes test_print_nodes)
ADD_TEST(unified test_unified)
