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
    task/vulkan/setuphelpergeometrytask.h task/vulkan/setuphelpergeometrytask.cpp
    task/vulkan/setuptexttask.h task/vulkan/setuptexttask.cpp
    task/vulkan/setuptextedittask.h task/vulkan/setuptextedittask.cpp
    task/vulkan/loadtexturestask.h task/vulkan/loadtexturestask.cpp
    task/vulkan/setuptexturedplanestask.h task/vulkan/setuptexturedplanestask.cpp
    task/vulkan/imageloader.h task/vulkan/imageloader.cpp
    task/vulkan/rendercubetask.h task/vulkan/rendercubetask.cpp)

set(EXAMPLE_WINDOW_SYSTEM_SOURCES
    system/texteditorsystem.h system/texteditorsystem.cpp
    system/selectionsystem.h system/selectionsystem.cpp
    system/manipulationsystem.h system/manipulationsystem.cpp
    system/uisystem.h system/uisystem.cpp)

set(EXAMPLE_WINDOW_SERVICE_SOURCES
    service/uiservice.h service/uiservice.cpp)

set(EXAMPLE_WINDOW_COMPONENT_SOURCES
    component/settingspanelcomponent.h)

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
    ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/panel.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/panel.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/grid.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/grid.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/sun.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/sun.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.frag
    ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.vert
    ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.frag
)

set(EXAMPLE_WINDOW_SHADER_OUTPUTS
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/cube.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.frag.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.vert.spv
    ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.frag.spv
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

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textvoxel.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/textvoxel.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/panel.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/panel.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/panel.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/panel.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/panel.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/glyph.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/pyramid.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/pyramid.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/grid.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/grid.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/grid.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/grid.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/grid.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/sun.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/sun.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/sun.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sun.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/sun.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/sphere.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/sphere.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/glyph_sdf.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/glyph_sdf.frag
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.vert -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.vert.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.vert
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXAMPLE_WINDOW_SHADER_OUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.frag -o ${EXAMPLE_WINDOW_SHADER_OUT_DIR}/textured_plane.frag.spv
    DEPENDS ${EXAMPLE_WINDOW_SHADER_DIR}/textured_plane.frag
    VERBATIM
)

add_custom_target(example_window_shaders ALL DEPENDS ${EXAMPLE_WINDOW_SHADER_OUTPUTS})

add_executable(${PROJECT_WINDOW_NAME}
    main.cpp
    ${EXAMPLE_WINDOW_HANDLER_SOURCES}
    ${EXAMPLE_WINDOW_TASK_SOURCES}
    ${EXAMPLE_WINDOW_SYSTEM_SOURCES}
    ${EXAMPLE_WINDOW_SERVICE_SOURCES}
    ${EXAMPLE_WINDOW_COMPONENT_SOURCES}
    ${EXAMPLE_WINDOW_STATE_SOURCES})

add_dependencies(${PROJECT_WINDOW_NAME} example_window_shaders)

add_custom_command(TARGET ${PROJECT_WINDOW_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_LIST_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_WINDOW_NAME}>/assets
)

target_link_libraries(${PROJECT_WINDOW_NAME}
    PRIVATE
    vigine
)

target_include_directories(${PROJECT_WINDOW_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
