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
    task/window/processinputeventtask.h task/window/processinputeventtask.cpp
    task/vulkan/initvulkantask.h task/vulkan/initvulkantask.cpp
    task/vulkan/setupcubetask.h task/vulkan/setupcubetask.cpp
    task/vulkan/rendercubetask.h task/vulkan/rendercubetask.cpp)

set(EXAMPLE_WINDOW_STATE_SOURCES
    state/initstate.h state/initstate.cpp
    state/workstate.h state/workstate.cpp
    state/errorstate.h state/errorstate.cpp
    state/closestate.h state/closestate.cpp)

set(EXAMPLE_WINDOW_SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/shader)
set(EXAMPLE_WINDOW_SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/bin/shaders)

find_program(GLSLC_EXECUTABLE glslc
    HINTS
    $ENV{VULKAN_SDK}/Bin
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK or add glslc to PATH.")
endif()

set(EXAMPLE_WINDOW_SHADER_SOURCES
    ${EXAMPLE_WINDOW_SHADER_DIR}/cube.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/cube.frag
)

set(EXAMPLE_WINDOW_SHADER_OUTPUTS
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.frag.spv
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/cube.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/cube.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/cube.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/cube.frag
    VERBATIM
)

add_custom_target(example_window_shaders ALL DEPENDS ${EXAMPLE_WINDOW_SHADER_OUTPUTS})

add_executable(${PROJECT_WINDOW_NAME}
    main.cpp
    ${EXAMPLE_WINDOW_HANDLER_SOURCES}
    ${EXAMPLE_WINDOW_TASK_SOURCES}
    ${EXAMPLE_WINDOW_STATE_SOURCES})

add_dependencies(${PROJECT_WINDOW_NAME} example_window_shaders)

target_link_libraries(${PROJECT_WINDOW_NAME}
    PRIVATE
    vigine
)

target_include_directories(${PROJECT_WINDOW_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
