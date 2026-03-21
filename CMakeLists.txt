cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME vigine)

set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed")

project(${PROJECT_NAME})
#
# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/FS)
endif()

if(WIN32)
    add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Set the CMake module path to include the cmake directory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Build FreeType from external submodule.
# Keep optional compression/shaping deps disabled for a predictable local build.
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "Disable zlib in bundled FreeType build")
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "Disable bzip2 in bundled FreeType build")
set(FT_DISABLE_PNG TRUE CACHE BOOL "Disable libpng in bundled FreeType build")
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz in bundled FreeType build")
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable Brotli in bundled FreeType build")
set(SKIP_INSTALL_ALL TRUE CACHE BOOL "Disable install targets for bundled third-party libs")
add_subdirectory(${CMAKE_SOURCE_DIR}/external/freetype EXCLUDE_FROM_ALL)

# Find and include glm
find_package(GLM REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHpp REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
#

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

set(BUILD_TYPE_DEBUG_VALUE 1)
set(BUILD_TYPE_RELEASE_VALUE 2)

add_compile_definitions(
    DEBUG=${BUILD_TYPE_DEBUG_VALUE}
    RELEASE=${BUILD_TYPE_RELEASE_VALUE}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BUILD_TYPE=${BUILD_TYPE_DEBUG_VALUE})
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(BUILD_TYPE=${BUILD_TYPE_RELEASE_VALUE})
else()
    message(FATAL_ERROR "Unsupported build type: ${CMAKE_BUILD_TYPE}")
endif()

# Option to build examples
option(ENABLE_EXAMPLE "Build example projects" ON)
option(ENABLE_UNITTEST "Enable tests" OFF)

if(ENABLE_UNITTEST AND EXISTS "${CMAKE_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
endif()

# Add the example project if the option is enabled
if(ENABLE_EXAMPLE AND EXISTS "${CMAKE_SOURCE_DIR}/example/CMakeLists.txt")
    add_subdirectory(example)
endif()

# Add source files
set(HEADER
    ${INCLUDE_DIR}/vigine/vigine.h
    ${INCLUDE_DIR}/vigine/result.h
    ${INCLUDE_DIR}/vigine/statemachine.h
    ${INCLUDE_DIR}/vigine/abstractstate.h
    ${INCLUDE_DIR}/vigine/taskflow.h
    ${INCLUDE_DIR}/vigine/signal/isignal.h
    ${INCLUDE_DIR}/vigine/signal/isignalemiter.h
    ${INCLUDE_DIR}/vigine/signal/isignalbinder.h
    ${INCLUDE_DIR}/vigine/abstracttask.h
    ${INCLUDE_DIR}/vigine/abstractservice.h
    ${INCLUDE_DIR}/vigine/context.h
    ${INCLUDE_DIR}/vigine/property.h
    ${INCLUDE_DIR}/vigine/ecs/abstractsystem.h
    ${INCLUDE_DIR}/vigine/ecs/abstractcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/entity.h
    ${INCLUDE_DIR}/vigine/ecs/componentmanager.h
    ${INCLUDE_DIR}/vigine/ecs/render/rendersystem.h
    ${INCLUDE_DIR}/vigine/ecs/render/rendercomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/vulkanapi.h
    ${INCLUDE_DIR}/vigine/ecs/render/transformcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/meshcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/shadercomponent.h
    ${INCLUDE_DIR}/vigine/contextholder.h
    ${INCLUDE_DIR}/vigine/ecs/entitymanager.h
    ${INCLUDE_DIR}/vigine/entitybindinghost.h
    ${INCLUDE_DIR}/vigine/base/constant.h
    ${INCLUDE_DIR}/vigine/base/function.h
)

set(SOURCES
    ${SRC_DIR}/vigine.cpp
    ${SRC_DIR}/result.cpp
    ${SRC_DIR}/statemachine.cpp
    ${SRC_DIR}/taskflow.cpp
    ${SRC_DIR}/context.cpp
    ${SRC_DIR}/ecs/componentmanager.cpp
    ${SRC_DIR}/ecs/render/rendersystem.cpp
    ${SRC_DIR}/ecs/render/rendercomponent.cpp
    ${SRC_DIR}/ecs/render/vulkanapi.cpp
    ${SRC_DIR}/ecs/render/transformcomponent.cpp
    ${SRC_DIR}/ecs/render/meshcomponent.cpp
    ${SRC_DIR}/ecs/render/shadercomponent.cpp
    ${SRC_DIR}/ecs/entity.cpp
    ${SRC_DIR}/abstracttask.cpp
    ${SRC_DIR}/abstractservice.cpp
    ${SRC_DIR}/contextholder.cpp
    ${SRC_DIR}/signal/isignalemiter.cpp
    ${SRC_DIR}/ecs/entitymanager.cpp
    ${SRC_DIR}/ecs/abstractsystem.cpp
    ${SRC_DIR}/entitybindinghost.cpp
)

# Add source files
set(HEADER_SERVICE
    ${INCLUDE_DIR}/vigine/service/databaseservice.h
    ${INCLUDE_DIR}/vigine/service/graphicsservice.h
    ${INCLUDE_DIR}/vigine/service/platformservice.h
)

set(SOURCES_SERVICE
    ${SRC_DIR}/service/databaseservice.cpp
    ${SRC_DIR}/service/graphicsservice.cpp
    ${SRC_DIR}/service/platformservice.cpp
)

# Add source files
set(HEADER_POSTGRESQL
    ${INCLUDE_DIR}/vigine/ecs/postgresql/postgresqlsystem.h
    ${SRC_DIR}/ecs/postgresql/postgresqlcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/postgresql/table.h
    ${INCLUDE_DIR}/vigine/ecs/postgresql/column.h
)

set(SOURCES_POSTGRESQL
    ${SRC_DIR}/ecs/postgresql/postgresqlcomponent.cpp
    ${SRC_DIR}/ecs/postgresql/postgresqlsystem.cpp
    ${SRC_DIR}/ecs/postgresql/table.cpp
    ${SRC_DIR}/ecs/postgresql/column.cpp
)

set(HEADER_PLATFORM
    ${INCLUDE_DIR}/vigine/ecs/platform/windowsystem.h
    ${SRC_DIR}/ecs/platform/windowcomponent.h
    ${SRC_DIR}/ecs/platform/windoweventdispatcher.h
)

set(SOURCES_PLATFORM
    ${SRC_DIR}/ecs/platform/windowsystem.cpp
    ${SRC_DIR}/ecs/platform/windowcomponent.cpp
    ${SRC_DIR}/ecs/platform/windoweventdispatcher.cpp
)

if(WIN32)
    list(APPEND HEADER_PLATFORM
        ${SRC_DIR}/ecs/platform/winapicomponent.h
    )

    list(APPEND SOURCES_PLATFORM
        ${SRC_DIR}/ecs/platform/winapicomponent.cpp
    )
endif()

# add vulkan
# Add a library
add_library(${PROJECT_NAME}
    STATIC
    ${HEADER}
    ${SOURCES}
    ${HEADER_SERVICE}
    ${SOURCES_SERVICE}
    ${HEADER_POSTGRESQL}
    ${SOURCES_POSTGRESQL}
    ${HEADER_PLATFORM}
    ${SOURCES_PLATFORM}
    include/vigine/ecs/postgresql/databaseconfiguration.h src/ecs/postgresql/databaseconfiguration.cpp
    include/vigine/ecs/postgresql/connectiondata.h src/ecs/postgresql/connectiondata.cpp
    include/vigine/base/name.h src/base/name.cpp
    include/vigine/base/password.h src/base/password.cpp
    include/vigine/base/macros.h
    include/vigine/ecs/postgresql/data.h src/ecs/postgresql/data.cpp
    include/vigine/ecs/postgresql/columntype.h
    include/vigine/base/info.h
    include/vigine/ecs/postgresql/postgresqlresult.h src/ecs/postgresql/postgresqlresult.cpp
    include/vigine/ecs/postgresql/query/querybuilder.h src/ecs/postgresql/query/querybuilder.cpp
    include/vigine/ecs/postgresql/tablerows.h src/ecs/postgresql/tablerows.cpp
    include/vigine/ecs/postgresql/row.h src/ecs/postgresql/row.cpp
    include/vigine/ecs/postgresql/postgresqltypeconverter.h src/ecs/postgresql/postgresqltypeconverter.cpp
)
# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GLM_INCLUDE_DIRS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${GLM_LIBRARIES}
    libpqxx::pqxx
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${Vulkan_LIBRARIES}
    ${Vulkan_Hpp_LIBRARIES}
    freetype
)

# Install the library and headers
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/build/lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${CMAKE_SOURCE_DIR}/build/include)
