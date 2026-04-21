cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME vigine)

set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed")

project(${PROJECT_NAME})

# Store Vigine's root for Find modules (CMAKE_SOURCE_DIR may differ when used as subdirectory)
set(VIGINE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Vigine root directory")

#
# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/FS /utf-8)
endif()

if(WIN32)
    add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set the CMake module path to include shared and platform-specific modules
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

if(APPLE)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/macos)
elseif(WIN32)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/windows)
elseif(UNIX)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/linux)
endif()

# Build FreeType from external submodule.
# Keep optional compression/shaping deps disabled for a predictable local build.
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "Disable zlib in bundled FreeType build")
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "Disable bzip2 in bundled FreeType build")
set(FT_DISABLE_PNG TRUE CACHE BOOL "Disable libpng in bundled FreeType build")
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz in bundled FreeType build")
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable Brotli in bundled FreeType build")
set(SKIP_INSTALL_ALL TRUE CACHE BOOL "Disable install targets for bundled third-party libs")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/freetype EXCLUDE_FROM_ALL)

# Find and include glm
find_package(GLM REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHpp REQUIRED)

# std::thread / std::mutex need the Threads package (Threads::Threads)
# on POSIX targets to pull in -pthread. A no-op on MSVC.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# PostgreSQL (libpqxx) is optional; disable on platforms without vcpkg postgres
option(ENABLE_POSTGRESQL "Enable PostgreSQL support (requires libpqxx via vcpkg)" ON)
if(ENABLE_POSTGRESQL)
    find_package(libpqxx CONFIG REQUIRED)
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
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

if(ENABLE_UNITTEST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    # enable_testing() must be called from the top-level CMakeLists.txt so
    # that ctest picks up tests registered with gtest_discover_tests in the
    # test subdirectory.
    enable_testing()
    add_subdirectory(test)
endif()

# Add the example project if the option is enabled
if(ENABLE_EXAMPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example/CMakeLists.txt")
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
    ${INCLUDE_DIR}/vigine/ecs/render/graphicsbackend.h
    ${INCLUDE_DIR}/vigine/ecs/render/graphicshandles.h
    ${INCLUDE_DIR}/vigine/ecs/render/camera.h
    ${INCLUDE_DIR}/vigine/ecs/render/surfacefactory.h
    ${INCLUDE_DIR}/vigine/ecs/render/vulkanapi.h
    ${INCLUDE_DIR}/vigine/ecs/render/transformcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/meshcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/shadercomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/texturecomponent.h
    ${INCLUDE_DIR}/vigine/ecs/render/pipelinecache.h
    ${INCLUDE_DIR}/vigine/contextholder.h
    ${INCLUDE_DIR}/vigine/ecs/entitymanager.h
    ${INCLUDE_DIR}/vigine/entitybindinghost.h
    ${INCLUDE_DIR}/vigine/base/constant.h
    ${INCLUDE_DIR}/vigine/base/function.h
    ${INCLUDE_DIR}/vigine/graph/nodeid.h
    ${INCLUDE_DIR}/vigine/graph/edgeid.h
    ${INCLUDE_DIR}/vigine/graph/traverse_mode.h
    ${INCLUDE_DIR}/vigine/graph/visit_result.h
    ${INCLUDE_DIR}/vigine/graph/kind.h
    ${INCLUDE_DIR}/vigine/graph/iedgedata.h
    ${INCLUDE_DIR}/vigine/graph/inode.h
    ${INCLUDE_DIR}/vigine/graph/iedge.h
    ${INCLUDE_DIR}/vigine/graph/igraphvisitor.h
    ${INCLUDE_DIR}/vigine/graph/igraphquery.h
    ${INCLUDE_DIR}/vigine/graph/igraph.h
    ${INCLUDE_DIR}/vigine/graph/factory.h
    ${INCLUDE_DIR}/vigine/graph/abstractgraph.h
    ${INCLUDE_DIR}/vigine/threading/threadaffinity.h
    ${INCLUDE_DIR}/vigine/threading/namedthreadid.h
    ${INCLUDE_DIR}/vigine/threading/threadmanagerconfig.h
    ${INCLUDE_DIR}/vigine/threading/irunnable.h
    ${INCLUDE_DIR}/vigine/threading/itaskhandle.h
    ${INCLUDE_DIR}/vigine/threading/ithreadmanager.h
    ${INCLUDE_DIR}/vigine/threading/abstractthreadmanager.h
    ${INCLUDE_DIR}/vigine/threading/defaultthreadmanager.h
    ${INCLUDE_DIR}/vigine/threading/factory.h
    ${INCLUDE_DIR}/vigine/payload/payloadtypeid.h
    ${INCLUDE_DIR}/vigine/payload/payloadrange.h
    ${INCLUDE_DIR}/vigine/payload/ipayloadregistry.h
    ${INCLUDE_DIR}/vigine/payload/factory.h
    ${INCLUDE_DIR}/vigine/messaging/kind.h
    ${INCLUDE_DIR}/vigine/messaging/connectionid.h
    ${INCLUDE_DIR}/vigine/messaging/targetkind.h
    ${INCLUDE_DIR}/vigine/messaging/iconnectiontoken.h
    ${INCLUDE_DIR}/vigine/messaging/ibuscontrolblock.h
    ${INCLUDE_DIR}/vigine/messaging/connectiontoken.h
    ${INCLUDE_DIR}/vigine/messaging/abstractmessagetarget.h
    ${INCLUDE_DIR}/vigine/statemachine/istatemachine.h
    ${INCLUDE_DIR}/vigine/context/icontext.h
    ${INCLUDE_DIR}/vigine/ecs/ientitymanager.h
    ${INCLUDE_DIR}/vigine/ecs/kind.h
    ${INCLUDE_DIR}/vigine/fsm/kind.h
    ${INCLUDE_DIR}/vigine/taskflow/kind.h
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
    ${SRC_DIR}/ecs/render/camera.cpp
    ${SRC_DIR}/ecs/render/vulkanapi.cpp
    ${SRC_DIR}/ecs/render/vulkandevice.cpp
    ${SRC_DIR}/ecs/render/vulkanswapchain.cpp
    ${SRC_DIR}/ecs/render/vulkantexturestore.cpp
    ${SRC_DIR}/ecs/render/vulkanpipelinestore.cpp
    ${SRC_DIR}/ecs/render/vulkanframerenderer.cpp
    ${SRC_DIR}/ecs/render/transformcomponent.cpp
    ${SRC_DIR}/ecs/render/meshcomponent.cpp
    ${SRC_DIR}/ecs/render/shadercomponent.cpp
    ${SRC_DIR}/ecs/render/texturecomponent.cpp
    ${SRC_DIR}/ecs/render/pipelinecache.cpp
    ${SRC_DIR}/ecs/entity.cpp
    ${SRC_DIR}/abstracttask.cpp
    ${SRC_DIR}/abstractservice.cpp
    ${SRC_DIR}/contextholder.cpp
    ${SRC_DIR}/signal/isignalemiter.cpp
    ${SRC_DIR}/ecs/entitymanager.cpp
    ${SRC_DIR}/ecs/abstractsystem.cpp
    ${SRC_DIR}/entitybindinghost.cpp
    ${SRC_DIR}/graph/abstractgraph.cpp
    ${SRC_DIR}/graph/defaultgraph.cpp
    ${SRC_DIR}/graph/defaultgraph_traverse.cpp
    ${SRC_DIR}/graph/defaultgraph_query.cpp
    ${SRC_DIR}/graph/defaultgraph_export.cpp
    ${SRC_DIR}/graph/factory.cpp
    ${SRC_DIR}/threading/abstractthreadmanager.cpp
    ${SRC_DIR}/threading/defaultthreadmanager.cpp
    ${SRC_DIR}/threading/factory.cpp
    ${SRC_DIR}/payload/abstractpayloadregistry.h
    ${SRC_DIR}/payload/abstractpayloadregistry.cpp
    ${SRC_DIR}/payload/defaultpayloadregistry.h
    ${SRC_DIR}/payload/defaultpayloadregistry.cpp
    ${SRC_DIR}/payload/factory.cpp
    ${SRC_DIR}/messaging/abstractmessagetarget.cpp
    ${SRC_DIR}/messaging/connectiontoken.cpp
    ${SRC_DIR}/messaging/ibuscontrolblock_default.h
    ${SRC_DIR}/messaging/ibuscontrolblock_default.cpp
    ${SRC_DIR}/messaging/cleanup.h
    ${SRC_DIR}/messaging/cleanup.cpp
)

# Add platform-specific surface factory
if(WIN32)
    list(APPEND SOURCES ${SRC_DIR}/ecs/render/platform/win32surfacefactory.cpp)
elseif(APPLE)
    list(APPEND SOURCES ${SRC_DIR}/ecs/render/platform/metalsurfacefactory.cpp)
endif()

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
if(ENABLE_POSTGRESQL)
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

    set(SOURCES_POSTGRESQL_EXTRA
        include/vigine/ecs/postgresql/databaseconfiguration.h src/ecs/postgresql/databaseconfiguration.cpp
        include/vigine/ecs/postgresql/connectiondata.h src/ecs/postgresql/connectiondata.cpp
        include/vigine/ecs/postgresql/data.h src/ecs/postgresql/data.cpp
        include/vigine/ecs/postgresql/columntype.h
        include/vigine/ecs/postgresql/postgresqlresult.h src/ecs/postgresql/postgresqlresult.cpp
        include/vigine/ecs/postgresql/query/querybuilder.h src/ecs/postgresql/query/querybuilder.cpp
        include/vigine/ecs/postgresql/tablerows.h src/ecs/postgresql/tablerows.cpp
        include/vigine/ecs/postgresql/row.h src/ecs/postgresql/row.cpp
        include/vigine/ecs/postgresql/postgresqltypeconverter.h src/ecs/postgresql/postgresqltypeconverter.cpp
    )
endif()

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

if(APPLE)
    list(APPEND HEADER_PLATFORM
        ${SRC_DIR}/ecs/platform/cocoawindowcomponent.h
    )

    list(APPEND SOURCES_PLATFORM
        ${SRC_DIR}/ecs/platform/cocoawindowcomponent.mm
    )
endif()

# add vulkan
# Add a library
option(VIGINE_BUILD_SHARED "Build Vigine as a shared library" OFF)
if(VIGINE_BUILD_SHARED)
    set(_VIGINE_LIB_TYPE SHARED)
else()
    set(_VIGINE_LIB_TYPE STATIC)
endif()

add_library(${PROJECT_NAME}
    ${_VIGINE_LIB_TYPE}
    ${HEADER}
    ${SOURCES}
    ${HEADER_SERVICE}
    ${SOURCES_SERVICE}
    ${HEADER_POSTGRESQL}
    ${SOURCES_POSTGRESQL}
    ${SOURCES_POSTGRESQL_EXTRA}
    ${HEADER_PLATFORM}
    ${SOURCES_PLATFORM}
    include/vigine/base/name.h src/base/name.cpp
    include/vigine/base/password.h src/base/password.cpp
    include/vigine/base/macros.h
    include/vigine/base/info.h
)
# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
    Threads::Threads
)

if(ENABLE_POSTGRESQL)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
        libpqxx::pqxx
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC VIGINE_POSTGRESQL=1)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${Vulkan_LIBRARIES}
    ${Vulkan_Hpp_LIBRARIES}
    freetype
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
    )
endif()

# Install the library and headers
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build/include)
