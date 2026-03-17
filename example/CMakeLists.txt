cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(PROJECT_EXAMPLE_NAME example)

project(${PROJECT_EXAMPLE_NAME})

set(EXAMPLE_STATE_SOURCES
    state/initstate.h state/initstate.cpp
    state/workstate.h state/workstate.cpp
    state/errorstate.h state/errorstate.cpp
    state/closestate.h state/closestate.cpp)

set(EXAMPLE_DB_TASK_SOURCES
    task/db/initbdtask.h task/db/initbdtask.cpp
    task/db/checkbdshecmetask.h task/db/checkbdshecmetask.cpp)

set(EXAMPLE_WINDOWS_SOURCES
    handler/windoweventhandler.h handler/windoweventhandler.cpp
    task/window/initwindowtask.h task/window/initwindowtask.cpp
    task/window/runwindowtask.h task/window/runwindowtask.cpp
    task/window/windoweventsignal.h task/window/windoweventsignal.cpp
    task/window/processinputeventtask.h task/window/processinputeventtask.cpp)

set(EXAMPLE_DATA_TASK_SOURCES
    task/data/addsomedatatask.h task/data/addsomedatatask.cpp
    task/data/readsomedatatask.h task/data/readsomedatatask.cpp
    task/data/removesomedatatask.h task/data/removesomedatatask.cpp)

# Add the executable for the example
add_executable(${PROJECT_EXAMPLE_NAME} main.cpp
    ${EXAMPLE_STATE_SOURCES}
    ${EXAMPLE_WINDOWS_SOURCES}
    ${EXAMPLE_DB_TASK_SOURCES}
    ${EXAMPLE_DATA_TASK_SOURCES})

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

