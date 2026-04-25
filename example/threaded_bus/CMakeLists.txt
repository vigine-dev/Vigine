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

# The vigine target PUBLIC-exports its include dirs, so consumers
# linking against it (PRIVATE here) inherit them automatically. No
# need to add ${CMAKE_SOURCE_DIR}/include manually -- that variable
# would point at a parent project when Vigine is added via
# add_subdirectory and could leak the wrong include root.
