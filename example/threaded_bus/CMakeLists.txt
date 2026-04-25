cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(PROJECT_THREADED_BUS_NAME example-threaded-bus)
project(${PROJECT_THREADED_BUS_NAME})

add_executable(${PROJECT_THREADED_BUS_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_THREADED_BUS_NAME}
    PRIVATE
    vigine
)

target_include_directories(${PROJECT_THREADED_BUS_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
