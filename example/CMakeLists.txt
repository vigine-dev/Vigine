cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
set(PROJECT_EXAMPLE_NAME example)

project(${PROJECT_EXAMPLE_NAME})

# Add the executable for the example
add_executable(${PROJECT_EXAMPLE_NAME} main.cpp
    state/initstate.h state/initstate.cpp
    state/workstate.h state/workstate.cpp
    state/errorstate.h state/errorstate.cpp
    state/closestate.h state/closestate.cpp
    task/initbdtask.h task/initbdtask.cpp
    task/checkbdshecmetask.h task/checkbdshecmetask.cpp
    task/addsomedatatask.h task/addsomedatatask.cpp
    task/readsomedatatask.h task/readsomedatatask.cpp
    task/removesomedatatask.h task/removesomedatatask.cpp)

# Link the example executable with the library
target_link_libraries(${PROJECT_EXAMPLE_NAME}
    PRIVATE
    vigine
)

# Include directories
target_include_directories(${PROJECT_EXAMPLE_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}
)

