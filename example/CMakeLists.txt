cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

project(example-projects)

option(BUILD_EXAMPLE_POSTGRESQL "Build combined PostgreSQL example project" ON)
option(BUILD_EXAMPLE_WINDOW "Build example-window project" ON)

if(BUILD_EXAMPLE_POSTGRESQL)
    add_subdirectory(postgresql)
endif()

if(BUILD_EXAMPLE_WINDOW)
    add_subdirectory(window)
endif()

