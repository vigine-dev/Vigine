cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(PROJECT_WINDOW_NAME example-window)
project(${PROJECT_WINDOW_NAME})

# Portability skip-guard: this example needs the Vulkan SDK (glslc) and
# the Win32 platform service. Linux/macOS CI runs without the Vulkan SDK
# installed and without the Win32 platform implementation, so opting in
# only on Windows keeps the cross-platform configure step clean. The
# guard MUST be the first effective statement so we never reach
# find_program(GLSLC_EXECUTABLE) on non-Windows hosts.
if(NOT WIN32)
    return()
endif()

set(EXAMPLE_WINDOW_HANDLER_SOURCES
    handler/windoweventhandler.h handler/windoweventhandler.cpp)

set(EXAMPLE_WINDOW_TASK_SOURCES
    task/window/initwindowtask.h task/window/initwindowtask.cpp
    task/window/runwindowtask.h task/window/runwindowtask.cpp
    task/window/windoweventpayload.h
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
    system/texteditorsystem.h system/texteditorsystem.cpp)

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
    ${EXAMPLE_WINDOW_STATE_SOURCES})

add_dependencies(${PROJECT_WINDOW_NAME} example_window_shaders)

add_custom_command(TARGET ${PROJECT_WINDOW_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_LIST_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_WINDOW_NAME}>/assets
)

# Mirror the engine-root `resource/` tree next to the exe so that
# LoadTexturesTask finds `resource/img/` when the binary is launched
# with its own directory as cwd. The task also walks cwd-relative
# fallbacks, so this copy plus the fallbacks cover every common
# launch path (engine root, exe dir, build/bin, build/bin/Release).
add_custom_command(TARGET ${PROJECT_WINDOW_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resource
    $<TARGET_FILE_DIR:${PROJECT_WINDOW_NAME}>/resource
)

target_link_libraries(${PROJECT_WINDOW_NAME}
    PRIVATE
    vigine
)

# WIN32_LEAN_AND_MEAN trims <windows.h>. Scoped per-target so it stays
# off the bundled FreeType command line (which redefines the macro
# internally and would otherwise emit C4005).
if(WIN32)
    target_compile_definitions(${PROJECT_WINDOW_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
endif()

# vigine target PUBLIC-exports its `include/` directory, so consumers
# linking against it (PRIVATE here) inherit `vigine/...` headers
# automatically -- the manual reach into ${CMAKE_SOURCE_DIR}/include is
# unnecessary and would leak the wrong include root when Vigine is
# added via add_subdirectory under a parent project. The
# ${CMAKE_SOURCE_DIR} entry below stays in place because
# task/vulkan/imageloader.cpp resolves the bundled stb header through
# `external/stb/stb_image.h`, which needs the engine root on the path.
target_include_directories(${PROJECT_WINDOW_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}
)

# Apply the central warning / strict-mode compile flags so the example
# is held to the same /WX + -Werror gate as the vigine library it
# depends on. Single source of truth for compile flags across the
# build matrix (vigine + tests + examples).
vigine_apply_compile_options(${PROJECT_WINDOW_NAME})
