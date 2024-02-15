cmake_minimum_required(VERSION 3.17)

project(args)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)
add_subdirectory(include)

option(ARGS_BUILD_EXAMPLES "Build TUI examples" OFF)

if (ARGS_BUILD_EXAMPLES)
    add_executable(args-example)
    target_sources(args-example PRIVATE example/main.cpp)
    target_link_libraries(args-example PRIVATE args)
endif ()