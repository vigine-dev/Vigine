cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(PROJECT_WINDOW_NAME example-window)
project(${PROJECT_WINDOW_NAME})

set(EXAMPLE_WINDOW_HANDLER_SOURCES
    handler/windoweventhandler.h handler/windoweventhandler.cpp)

set(EXAMPLE_WINDOW_TASK_SOURCES
    task/window/initwindowtask.h task/window/initwindowtask.cpp
    task/window/runwindowtask.h task/window/runwindowtask.cpp
    task/window/windoweventsignal.h task/window/windoweventsignal.cpp
    task/window/processinputeventtask.h task/window/processinputeventtask.cpp)

set(EXAMPLE_WINDOW_STATE_SOURCES
    state/initstate.h state/initstate.cpp
    state/workstate.h state/workstate.cpp
    state/errorstate.h state/errorstate.cpp
    state/closestate.h state/closestate.cpp)

add_executable(${PROJECT_WINDOW_NAME}
    main.cpp
    ${EXAMPLE_WINDOW_HANDLER_SOURCES}
    ${EXAMPLE_WINDOW_TASK_SOURCES}
    ${EXAMPLE_WINDOW_STATE_SOURCES})

target_link_libraries(${PROJECT_WINDOW_NAME}
    PRIVATE
    vigine
)

target_include_directories(${PROJECT_WINDOW_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
