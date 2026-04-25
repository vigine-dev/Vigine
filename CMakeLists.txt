cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME vigine)

set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed")

project(${PROJECT_NAME})

# Store Vigine's root for Find modules (CMAKE_SOURCE_DIR may differ when used as subdirectory)
set(VIGINE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Vigine root directory")

#
# Set C++23 standard. CMAKE_CXX_EXTENSIONS OFF keeps the build portable
# across MSVC, libc++ / clang, and libstdc++ / gcc -- GNU extensions
# silently change semantics (e.g. VLAs, __restrict) that are not part
# of ISO C++.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Load the central compile-options helper. The helper is scoped to the
# vigine library target (not directory-wide) so bundled third-party
# sources like FreeType do not inherit /W4 / -Wpedantic.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/VigineCompileOptions.cmake)

# Optional sanitizer support (Linux + Clang only).
# Pass -DVIGINE_SANITIZER=asan|ubsan|tsan to enable. MSVC builds must
# leave this unset (the helper aborts with a clear error if called on MSVC).
set(VIGINE_SANITIZER "" CACHE STRING "Sanitizer to enable: asan | ubsan | tsan (empty = off)")
set_property(CACHE VIGINE_SANITIZER PROPERTY STRINGS "" asan ubsan tsan)
if(NOT VIGINE_SANITIZER STREQUAL "")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/VigineSanitizer.cmake)
endif()

if(MSVC)
    add_compile_options(/FS /utf-8)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set the CMake module path to include shared modules. The
# platform-specific module path is appended by the dispatcher's
# vigine_platform_setup_global() call below.
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# Platform dispatcher (R-PlatformPortability). Loads exactly one
# cmake/platform/<platform>/config.cmake fragment. Each fragment
# defines three callbacks consumed below: vigine_platform_setup_global,
# vigine_platform_collect_sources, and vigine_platform_apply_target.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/platform.cmake")
vigine_platform_setup_global()

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
# Vulkan 1.3 is the baseline. Try 1.3 first; if the build host only has
# 1.2, downgrade with a visible WARNING so the mismatch is audible in
# configure logs. CI jobs pin SDK 1.3.
find_package(Vulkan 1.3 QUIET)
if(NOT Vulkan_FOUND)
    message(WARNING
        "Vulkan 1.3 SDK not found; falling back to Vulkan >= 1.2. "
        "CI jobs require 1.3 -- install VulkanSDK 1.3+ for a parity build.")
    find_package(Vulkan 1.2 REQUIRED)
endif()
find_package(VulkanHpp REQUIRED)

# std::thread / std::mutex need the Threads package (Threads::Threads)
# on POSIX targets to pull in -pthread. A no-op on MSVC.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# libxcb / XCB discovery is handled by the Linux platform fragment
# (cmake/platform/linux/config.cmake) loaded via the platform
# dispatcher above. Other platforms have no XCB dependency.

# Experimental subsystems are gated behind a single opt-in flag so the
# default build keeps the public surface small. Postgres is the first
# subsystem under this umbrella; future experimental features land here
# too.
option(VIGINE_ENABLE_EXPERIMENTAL "Build experimental subsystems (postgresql, etc.)" OFF)

# PostgreSQL (libpqxx) is optional; disable on platforms without vcpkg postgres.
# The legacy ENABLE_POSTGRESQL switch is kept as a per-subsystem toggle but is
# now gated by the experimental umbrella -- if VIGINE_ENABLE_EXPERIMENTAL is
# OFF, postgres is unconditionally excluded.
if(VIGINE_ENABLE_EXPERIMENTAL)
    option(ENABLE_POSTGRESQL "Enable PostgreSQL support (requires libpqxx via vcpkg)" ON)
    if(ENABLE_POSTGRESQL)
        find_package(libpqxx CONFIG REQUIRED)
    endif()
else()
    set(ENABLE_POSTGRESQL OFF CACHE BOOL "Enable PostgreSQL support (requires libpqxx via vcpkg)" FORCE)
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

# Per-example opt-in switches for the new examples landing under the
# v0.1.0 readiness umbrella. Default OFF; the matching subdirectory
# stubs hold only a placeholder until a follow-up PR replaces them
# with real sources, so flipping these ON today is a safe no-op
# guarded by EXISTS in example/CMakeLists.txt.
option(BUILD_EXAMPLE_THREADED_BUS "Build the threaded_bus example" OFF)
option(BUILD_EXAMPLE_PARALLEL_FSM "Build the parallel_fsm example" OFF)
option(BUILD_EXAMPLE_FANOUT_FSM "Build the fanout_fsm example" OFF)

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
    ${INCLUDE_DIR}/vigine/abstracttask.h
    ${INCLUDE_DIR}/vigine/abstractservice.h
    ${INCLUDE_DIR}/vigine/context.h
    ${INCLUDE_DIR}/vigine/property.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractsystem.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractcomponent.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractentity.h
    ${INCLUDE_DIR}/vigine/impl/ecs/entity.h
    ${INCLUDE_DIR}/vigine/impl/ecs/componentmanager.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractcomponentmanager.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractentitymanager.h
    ${INCLUDE_DIR}/vigine/api/ecs/icomponent.h
    ${INCLUDE_DIR}/vigine/api/ecs/ientity.h
    ${INCLUDE_DIR}/vigine/api/ecs/isystem.h
    ${INCLUDE_DIR}/vigine/contextholder.h
    ${INCLUDE_DIR}/vigine/impl/ecs/entitymanager.h
    ${INCLUDE_DIR}/vigine/entitybindinghost.h
    ${INCLUDE_DIR}/vigine/base/constant.h
    ${INCLUDE_DIR}/vigine/core/graph/nodeid.h
    ${INCLUDE_DIR}/vigine/core/graph/edgeid.h
    ${INCLUDE_DIR}/vigine/core/graph/traverse_mode.h
    ${INCLUDE_DIR}/vigine/core/graph/visit_result.h
    ${INCLUDE_DIR}/vigine/core/graph/kind.h
    ${INCLUDE_DIR}/vigine/core/graph/iedgedata.h
    ${INCLUDE_DIR}/vigine/core/graph/inode.h
    ${INCLUDE_DIR}/vigine/core/graph/iedge.h
    ${INCLUDE_DIR}/vigine/core/graph/igraphvisitor.h
    ${INCLUDE_DIR}/vigine/core/graph/igraphquery.h
    ${INCLUDE_DIR}/vigine/core/graph/igraph.h
    ${INCLUDE_DIR}/vigine/core/graph/factory.h
    ${INCLUDE_DIR}/vigine/core/graph/abstractgraph.h
    ${INCLUDE_DIR}/vigine/core/threading/threadaffinity.h
    ${INCLUDE_DIR}/vigine/core/threading/namedthreadid.h
    ${INCLUDE_DIR}/vigine/core/threading/threadmanagerconfig.h
    ${INCLUDE_DIR}/vigine/core/threading/irunnable.h
    ${INCLUDE_DIR}/vigine/core/threading/itaskhandle.h
    ${INCLUDE_DIR}/vigine/core/threading/imutex.h
    ${INCLUDE_DIR}/vigine/core/threading/isemaphore.h
    ${INCLUDE_DIR}/vigine/core/threading/ibarrier.h
    ${INCLUDE_DIR}/vigine/core/threading/imessagechannel.h
    ${INCLUDE_DIR}/vigine/core/threading/ithreadmanager.h
    ${INCLUDE_DIR}/vigine/core/threading/abstractthreadmanager.h
    ${INCLUDE_DIR}/vigine/core/threading/threadmanager.h
    ${INCLUDE_DIR}/vigine/core/threading/factory.h
    ${INCLUDE_DIR}/vigine/core/threading/parallel_for.h
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
    ${INCLUDE_DIR}/vigine/messaging/busid.h
    ${INCLUDE_DIR}/vigine/messaging/busconfig.h
    ${INCLUDE_DIR}/vigine/messaging/messagekind.h
    ${INCLUDE_DIR}/vigine/messaging/routemode.h
    ${INCLUDE_DIR}/vigine/messaging/messagefilter.h
    ${INCLUDE_DIR}/vigine/messaging/subscriptionslot.h
    ${INCLUDE_DIR}/vigine/messaging/imessagepayload.h
    ${INCLUDE_DIR}/vigine/messaging/imessage.h
    ${INCLUDE_DIR}/vigine/messaging/isubscriber.h
    ${INCLUDE_DIR}/vigine/messaging/isubscriptiontoken.h
    ${INCLUDE_DIR}/vigine/messaging/imessagebus.h
    ${INCLUDE_DIR}/vigine/messaging/abstractmessagebus.h
    ${INCLUDE_DIR}/vigine/messaging/systemmessagebus.h
    ${INCLUDE_DIR}/vigine/messaging/factory.h
    ${INCLUDE_DIR}/vigine/statemachine/istatemachine.h
    ${INCLUDE_DIR}/vigine/api/context/icontext.h
    ${INCLUDE_DIR}/vigine/api/ecs/ientitymanager.h
    ${INCLUDE_DIR}/vigine/api/ecs/componentkind.h
    ${INCLUDE_DIR}/vigine/fsm/kind.h
    ${INCLUDE_DIR}/vigine/taskflow/kind.h
)

set(SOURCES
    ${SRC_DIR}/vigine.cpp
    ${SRC_DIR}/result.cpp
    ${SRC_DIR}/statemachine.cpp
    ${SRC_DIR}/taskflow.cpp
    ${SRC_DIR}/context.cpp
    ${SRC_DIR}/impl/ecs/componentmanager.cpp
    ${SRC_DIR}/impl/ecs/entity.cpp
    ${SRC_DIR}/abstracttask.cpp
    ${SRC_DIR}/abstractservice.cpp
    ${SRC_DIR}/contextholder.cpp
    ${SRC_DIR}/impl/ecs/entitymanager.cpp
    ${SRC_DIR}/api/ecs/abstractsystem.cpp
    ${SRC_DIR}/entitybindinghost.cpp
    ${SRC_DIR}/core/graph/abstractgraph.cpp
    ${SRC_DIR}/core/graph/defaultgraph.cpp
    ${SRC_DIR}/core/graph/defaultgraph_traverse.cpp
    ${SRC_DIR}/core/graph/defaultgraph_query.cpp
    ${SRC_DIR}/core/graph/defaultgraph_export.cpp
    ${SRC_DIR}/core/graph/factory.cpp
    ${SRC_DIR}/core/threading/abstractthreadmanager.cpp
    ${SRC_DIR}/core/threading/threadmanager.cpp
    ${SRC_DIR}/core/threading/defaultmutex.h
    ${SRC_DIR}/core/threading/defaultmutex.cpp
    ${SRC_DIR}/core/threading/defaultsemaphore.h
    ${SRC_DIR}/core/threading/defaultsemaphore.cpp
    ${SRC_DIR}/core/threading/defaultbarrier.h
    ${SRC_DIR}/core/threading/defaultbarrier.cpp
    ${SRC_DIR}/core/threading/defaultmessagechannel.h
    ${SRC_DIR}/core/threading/defaultmessagechannel.cpp
    ${SRC_DIR}/core/threading/defaultthreadmanager_factories.cpp
    ${SRC_DIR}/core/threading/factory.cpp
    ${SRC_DIR}/core/threading/parallel_for.cpp
    ${SRC_DIR}/payload/abstractpayloadregistry.h
    ${SRC_DIR}/payload/abstractpayloadregistry.cpp
    ${SRC_DIR}/payload/defaultpayloadregistry.h
    ${SRC_DIR}/payload/defaultpayloadregistry.cpp
    ${SRC_DIR}/payload/factory.cpp
    ${SRC_DIR}/messaging/abstractmessagetarget.cpp
    ${SRC_DIR}/messaging/connectiontoken.cpp
    ${SRC_DIR}/messaging/ibuscontrolblock_default.h
    ${SRC_DIR}/messaging/ibuscontrolblock_default.cpp
    ${SRC_DIR}/messaging/abstractmessagebus.cpp
    ${SRC_DIR}/messaging/systemmessagebus.cpp
    ${SRC_DIR}/messaging/createmessagebus.cpp
    ${SRC_DIR}/messaging/routing/firstmatch.cpp
    ${SRC_DIR}/messaging/routing/fanout.cpp
    ${SRC_DIR}/messaging/routing/chain.cpp
    ${SRC_DIR}/messaging/routing/bubble.cpp
    ${SRC_DIR}/messaging/routing/broadcast.cpp
)

# ECS graphics subsystem (#280) -- public interfaces under api/, concrete
# components / systems / services and Vulkan helpers under impl/.
#
# The platform-specific Vulkan surface factory (Win32 / Metal / XCB) is
# NOT listed here -- it is appended onto SOURCES_ECS_GRAPHICS by the
# active platform fragment via vigine_platform_collect_sources() below
# (R-PlatformPortability: no if(WIN32/APPLE/UNIX) blocks in root
# CMakeLists.txt).
set(HEADER_ECS_GRAPHICS
    ${INCLUDE_DIR}/vigine/api/ecs/graphics/igraphicsbackend.h
    ${INCLUDE_DIR}/vigine/api/ecs/graphics/isurfacefactory.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/camera.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/graphicshandles.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/graphicsservice.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/meshcomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/pipelinecache.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/rendercomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/rendersystem.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/shadercomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/textcomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/texturecomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/transformcomponent.h
    ${INCLUDE_DIR}/vigine/impl/ecs/graphics/vulkanapi.h
)

set(SOURCES_ECS_GRAPHICS
    ${SRC_DIR}/impl/ecs/graphics/camera.cpp
    ${SRC_DIR}/impl/ecs/graphics/graphicsservice.cpp
    ${SRC_DIR}/impl/ecs/graphics/meshcomponent.cpp
    ${SRC_DIR}/impl/ecs/graphics/pipelinecache.cpp
    ${SRC_DIR}/impl/ecs/graphics/rendercomponent.cpp
    ${SRC_DIR}/impl/ecs/graphics/rendersystem.cpp
    ${SRC_DIR}/impl/ecs/graphics/shadercomponent.cpp
    ${SRC_DIR}/impl/ecs/graphics/surfacefactory.h
    ${SRC_DIR}/impl/ecs/graphics/texturecomponent.cpp
    ${SRC_DIR}/impl/ecs/graphics/transformcomponent.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkanapi.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkandevice.h
    ${SRC_DIR}/impl/ecs/graphics/vulkandevice.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkanframerenderer.h
    ${SRC_DIR}/impl/ecs/graphics/vulkanframerenderer.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkanpipelinestore.h
    ${SRC_DIR}/impl/ecs/graphics/vulkanpipelinestore.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkanswapchain.h
    ${SRC_DIR}/impl/ecs/graphics/vulkanswapchain.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkantexturestore.h
    ${SRC_DIR}/impl/ecs/graphics/vulkantexturestore.cpp
    ${SRC_DIR}/impl/ecs/graphics/vulkantypes.h
)

# Add source files
set(HEADER_SERVICE
    ${INCLUDE_DIR}/vigine/service/databaseservice.h
    # Level-1 service wrapper (R.4.1) — public surface.
    ${INCLUDE_DIR}/vigine/api/service/serviceid.h
    ${INCLUDE_DIR}/vigine/api/service/dependencykind.h
    ${INCLUDE_DIR}/vigine/api/service/initpolicy.h
    ${INCLUDE_DIR}/vigine/api/service/iservice.h
    ${INCLUDE_DIR}/vigine/api/service/abstractservice.h
    ${INCLUDE_DIR}/vigine/api/service/factory.h
)

set(SOURCES_SERVICE
    ${SRC_DIR}/service/databaseservice.cpp
    # Level-1 service wrapper (R.4.1) — internal registry + base impl.
    ${SRC_DIR}/service/serviceregistry.h
    ${SRC_DIR}/service/serviceregistry.cpp
    ${SRC_DIR}/api/service/abstractservice.cpp
    ${SRC_DIR}/service/defaultservice.h
    ${SRC_DIR}/service/defaultservice.cpp
    ${SRC_DIR}/service/factory.cpp
)

# Level-1 ECS wrapper (R.4.2) -- public surface.
set(HEADER_ECS_CORE
    ${INCLUDE_DIR}/vigine/api/ecs/ecstypes.h
    ${INCLUDE_DIR}/vigine/api/ecs/iecs.h
    ${INCLUDE_DIR}/vigine/api/ecs/icomponentmanager.h
    ${INCLUDE_DIR}/vigine/api/ecs/abstractecs.h
    ${INCLUDE_DIR}/vigine/impl/ecs/factory.h
)

# Level-1 ECS wrapper (R.4.2) -- internal entity world + base impl.
set(SOURCES_ECS_CORE
    ${SRC_DIR}/ecs/entityworld.h
    ${SRC_DIR}/ecs/entityworld.cpp
    ${SRC_DIR}/api/ecs/abstractecs.cpp
    ${SRC_DIR}/ecs/defaultecs.h
    ${SRC_DIR}/ecs/defaultecs.cpp
    ${SRC_DIR}/impl/ecs/factory.cpp
)

# Level-1 state machine wrapper (R.4.3) -- public surface.
set(HEADER_STATEMACHINE_CORE
    ${INCLUDE_DIR}/vigine/statemachine/stateid.h
    ${INCLUDE_DIR}/vigine/statemachine/routemode.h
    ${INCLUDE_DIR}/vigine/statemachine/abstractstatemachine.h
    ${INCLUDE_DIR}/vigine/statemachine/factory.h
)

# Level-1 state machine wrapper (R.4.3) -- internal state topology + base impl.
set(SOURCES_STATEMACHINE_CORE
    ${SRC_DIR}/statemachine/statetopology.h
    ${SRC_DIR}/statemachine/statetopology.cpp
    ${SRC_DIR}/statemachine/abstractstatemachine.cpp
    ${SRC_DIR}/statemachine/defaultstatemachine.h
    ${SRC_DIR}/statemachine/defaultstatemachine.cpp
    ${SRC_DIR}/statemachine/factory.cpp
)

# Level-1 task flow wrapper (R.4.4) -- public surface.
set(HEADER_TASKFLOW_CORE
    ${INCLUDE_DIR}/vigine/taskflow/taskid.h
    ${INCLUDE_DIR}/vigine/taskflow/resultcode.h
    ${INCLUDE_DIR}/vigine/taskflow/routemode.h
    ${INCLUDE_DIR}/vigine/taskflow/itaskflow.h
    ${INCLUDE_DIR}/vigine/taskflow/abstracttaskflow.h
    ${INCLUDE_DIR}/vigine/taskflow/factory.h
)

# Level-1 task flow wrapper (R.4.4) -- internal orchestrator + base impl.
set(SOURCES_TASKFLOW_CORE
    ${SRC_DIR}/taskflow/taskorchestrator.h
    ${SRC_DIR}/taskflow/taskorchestrator.cpp
    ${SRC_DIR}/taskflow/abstracttaskflow.cpp
    ${SRC_DIR}/taskflow/defaulttaskflow.h
    ${SRC_DIR}/taskflow/defaulttaskflow.cpp
    ${SRC_DIR}/taskflow/factory.cpp
)

# Context aggregator (R.4.5) -- public surface.
set(HEADER_CONTEXT_AGGREGATOR
    ${INCLUDE_DIR}/vigine/api/context/contextconfig.h
    ${INCLUDE_DIR}/vigine/api/context/abstractcontext.h
    ${INCLUDE_DIR}/vigine/api/context/factory.h
    ${INCLUDE_DIR}/vigine/impl/context/context.h
)

# Context aggregator (R.4.5) -- internal concrete + factory + base impl.
set(SOURCES_CONTEXT_AGGREGATOR
    ${SRC_DIR}/api/context/abstractcontext.cpp
    ${SRC_DIR}/impl/context/context.cpp
    ${SRC_DIR}/context/factory.cpp
)

# Signal-emitter facade (R.3.3.1.1 / plan_15) -- public surface.
set(HEADER_SIGNALEMITTER
    ${INCLUDE_DIR}/vigine/signalemitter/isignalpayload.h
    ${INCLUDE_DIR}/vigine/signalemitter/isignalemitter.h
    ${INCLUDE_DIR}/vigine/signalemitter/abstractsignalemitter.h
    ${INCLUDE_DIR}/vigine/signalemitter/defaultsignalemitter.h
)

# Signal-emitter facade (R.3.3.1.1 / plan_15) -- internal concrete + factory.
set(SOURCES_SIGNALEMITTER
    ${SRC_DIR}/signalemitter/abstractsignalemitter.cpp
    ${SRC_DIR}/signalemitter/defaultsignalemitter.cpp
)

# Event-scheduler facade -- public surface. The concrete
# DefaultEventScheduler type is NOT in the public tree; callers reach
# the scheduler exclusively through `createEventScheduler` from
# factory.h, which returns a `std::unique_ptr<IEventScheduler>`.
set(HEADER_EVENTSCHEDULER
    ${INCLUDE_DIR}/vigine/eventscheduler/ossignal.h
    ${INCLUDE_DIR}/vigine/eventscheduler/eventconfig.h
    ${INCLUDE_DIR}/vigine/eventscheduler/ieventhandle.h
    ${INCLUDE_DIR}/vigine/eventscheduler/itimersource.h
    ${INCLUDE_DIR}/vigine/eventscheduler/iossignalsource.h
    ${INCLUDE_DIR}/vigine/eventscheduler/ieventscheduler.h
    ${INCLUDE_DIR}/vigine/eventscheduler/abstracteventscheduler.h
    ${INCLUDE_DIR}/vigine/eventscheduler/factory.h
)

# Event-scheduler facade -- internal concrete + platform.
set(SOURCES_EVENTSCHEDULER
    ${SRC_DIR}/eventscheduler/abstracteventscheduler.cpp
    ${SRC_DIR}/eventscheduler/defaulteventscheduler.h
    ${SRC_DIR}/eventscheduler/defaulteventscheduler.cpp
    ${SRC_DIR}/eventscheduler/itimersource_default.h
    ${SRC_DIR}/eventscheduler/itimersource_default.cpp
)

# Platform-specific OS-signal source (iossignalsource_*.{h,cpp,mm}) is
# appended onto SOURCES_PLATFORM via the dispatcher callback below.
# Keeping it on a single platform list (instead of per-facade) is the
# R-PlatformPortability shape: each platform has ONE collection point.

# Topic-bus facade (R.3.3.1.3 / plan_17) -- public surface.
set(HEADER_TOPICBUS
    ${INCLUDE_DIR}/vigine/api/topicbus/topicid.h
    ${INCLUDE_DIR}/vigine/api/topicbus/itopicbus.h
    ${INCLUDE_DIR}/vigine/api/topicbus/abstracttopicbus.h
    ${INCLUDE_DIR}/vigine/api/topicbus/factory.h
    ${INCLUDE_DIR}/vigine/impl/topicbus/topicbus.h
)

# Topic-bus facade (R.3.3.1.3 / plan_17) -- internal concrete + factory.
set(SOURCES_TOPICBUS
    ${SRC_DIR}/api/topicbus/abstracttopicbus.cpp
    ${SRC_DIR}/impl/topicbus/topicbus.cpp
    ${SRC_DIR}/topicbus/factory.cpp
)

# Channel factory facade (R.3.3.1.4 / plan_18) -- public surface.
set(HEADER_CHANNELFACTORY
    ${INCLUDE_DIR}/vigine/api/channelfactory/channelkind.h
    ${INCLUDE_DIR}/vigine/api/channelfactory/ichannel.h
    ${INCLUDE_DIR}/vigine/api/channelfactory/ichannelfactory.h
    ${INCLUDE_DIR}/vigine/api/channelfactory/abstractchannelfactory.h
    ${INCLUDE_DIR}/vigine/api/channelfactory/factory.h
    ${INCLUDE_DIR}/vigine/impl/channelfactory/channelfactory.h
)

# Channel factory facade (R.3.3.1.4 / plan_18) -- internal concrete + factory.
set(SOURCES_CHANNELFACTORY
    ${SRC_DIR}/api/channelfactory/abstractchannelfactory.cpp
    ${SRC_DIR}/impl/channelfactory/channelfactory.cpp
    ${SRC_DIR}/channelfactory/defaultchannel.h
    ${SRC_DIR}/channelfactory/defaultchannel.cpp
    ${SRC_DIR}/channelfactory/factory.cpp
)

# Request-bus facade (R.3.3.2.1 / plan_19) -- public surface.
set(HEADER_REQUESTBUS
    ${INCLUDE_DIR}/vigine/api/requestbus/ifuture.h
    ${INCLUDE_DIR}/vigine/api/requestbus/requestconfig.h
    ${INCLUDE_DIR}/vigine/api/requestbus/irequestbus.h
    ${INCLUDE_DIR}/vigine/api/requestbus/abstractrequestbus.h
    ${INCLUDE_DIR}/vigine/api/requestbus/factory.h
    ${INCLUDE_DIR}/vigine/impl/requestbus/requestbus.h
)

# Request-bus facade (R.3.3.2.1 / plan_19) -- internal concrete + factory.
set(SOURCES_REQUESTBUS
    ${SRC_DIR}/api/requestbus/abstractrequestbus.cpp
    ${SRC_DIR}/impl/requestbus/requestbus.cpp
    ${SRC_DIR}/requestbus/factory.cpp
)

# Reactive-stream facade -- public surface. The `factory.h` entry
# matches the convention used by every other facade (signal / topic
# / channel / request / actor / pipeline / event scheduler): a
# factory-only public header that does not pull the concrete
# `ReactiveStream` into callers' translation units.
set(HEADER_REACTIVESTREAM
    ${INCLUDE_DIR}/vigine/api/reactivestream/ireactivesubscription.h
    ${INCLUDE_DIR}/vigine/api/reactivestream/ireactivesubscriber.h
    ${INCLUDE_DIR}/vigine/api/reactivestream/ireactivepublisher.h
    ${INCLUDE_DIR}/vigine/api/reactivestream/ireactivestream.h
    ${INCLUDE_DIR}/vigine/api/reactivestream/abstractreactivestream.h
    ${INCLUDE_DIR}/vigine/api/reactivestream/factory.h
    ${INCLUDE_DIR}/vigine/impl/reactivestream/reactivestream.h
)

# Reactive-stream facade (R.3.3.2.2 / plan_20) -- internal concrete + factory.
set(SOURCES_REACTIVESTREAM
    ${SRC_DIR}/api/reactivestream/abstractreactivestream.cpp
    ${SRC_DIR}/impl/reactivestream/reactivestream.cpp
    ${SRC_DIR}/reactivestream/factory.cpp
)

# Actor-host facade (R.3.3.2.3 / plan_21) -- public surface.
set(HEADER_ACTORHOST
    ${INCLUDE_DIR}/vigine/api/actorhost/actorid.h
    ${INCLUDE_DIR}/vigine/api/actorhost/iactor.h
    ${INCLUDE_DIR}/vigine/api/actorhost/iactormailbox.h
    ${INCLUDE_DIR}/vigine/api/actorhost/iactorhost.h
    ${INCLUDE_DIR}/vigine/api/actorhost/abstractactorhost.h
    ${INCLUDE_DIR}/vigine/api/actorhost/factory.h
    ${INCLUDE_DIR}/vigine/impl/actorhost/actorhost.h
)

# Actor-host facade (R.3.3.2.3 / plan_21) -- internal concrete + factory.
set(SOURCES_ACTORHOST
    ${SRC_DIR}/api/actorhost/abstractactorhost.cpp
    ${SRC_DIR}/impl/actorhost/actorhost.cpp
    ${SRC_DIR}/actorhost/factory.cpp
)

# Pipeline-builder facade (R.3.3.2.4 / plan_22) -- public surface.
set(HEADER_PIPELINEBUILDER
    ${INCLUDE_DIR}/vigine/api/pipelinebuilder/ipipelinestage.h
    ${INCLUDE_DIR}/vigine/api/pipelinebuilder/ipipeline.h
    ${INCLUDE_DIR}/vigine/api/pipelinebuilder/ipipelinebuilder.h
    ${INCLUDE_DIR}/vigine/api/pipelinebuilder/abstractpipelinebuilder.h
    ${INCLUDE_DIR}/vigine/api/pipelinebuilder/factory.h
    ${INCLUDE_DIR}/vigine/impl/pipelinebuilder/pipelinebuilder.h
)

# Pipeline-builder facade (R.3.3.2.4 / plan_22) -- internal concrete + factory.
set(SOURCES_PIPELINEBUILDER
    ${SRC_DIR}/api/pipelinebuilder/abstractpipelinebuilder.cpp
    ${SRC_DIR}/impl/pipelinebuilder/pipelinebuilder.cpp
    ${SRC_DIR}/pipelinebuilder/factory.cpp
)

# Engine runtime + lifecycle (R.5.1 / plan_23) -- public surface.
set(HEADER_ENGINE
    ${INCLUDE_DIR}/vigine/api/engine/engineconfig.h
    ${INCLUDE_DIR}/vigine/api/engine/iengine.h
    ${INCLUDE_DIR}/vigine/api/engine/abstractengine.h
    ${INCLUDE_DIR}/vigine/api/engine/factory.h
    ${INCLUDE_DIR}/vigine/impl/engine/engine.h
)

# Engine runtime + lifecycle (R.5.1 / plan_23) -- internal concrete + factory.
set(SOURCES_ENGINE
    ${SRC_DIR}/api/engine/abstractengine.cpp
    ${SRC_DIR}/impl/engine/engine.cpp
    ${SRC_DIR}/engine/factory.cpp
)

# Engine-token scaffolding -- pure interface (#220) + thin abstract
# base + concrete EngineToken final (#287). The FSM-level invalidation
# hook ships alongside this leaf as an extension to AbstractStateMachine
# (no new sources needed under SOURCES_STATEMACHINE; the hook bodies live
# in src/statemachine/abstractstatemachine.cpp).
set(HEADER_ENGINE_TOKEN
    ${INCLUDE_DIR}/vigine/api/engine/iengine_token.h
    ${INCLUDE_DIR}/vigine/api/engine/abstractengine_token.h
    ${INCLUDE_DIR}/vigine/api/messaging/payload/stateinvalidatedpayload.h
    ${INCLUDE_DIR}/vigine/impl/engine/enginetoken.h
)

# Engine-token concrete implementation (#287).
set(SOURCES_ENGINE_TOKEN
    ${SRC_DIR}/impl/engine/enginetoken.cpp
)

# Postgres ECS subsystem lives under the experimental opt-in umbrella.
# Both VIGINE_ENABLE_EXPERIMENTAL (umbrella) and ENABLE_POSTGRESQL
# (per-subsystem) must be ON to compile postgres. The umbrella defaults
# to OFF so the public default build never pulls libpqxx headers or
# emits postgres symbols.
if(VIGINE_ENABLE_EXPERIMENTAL AND ENABLE_POSTGRESQL)
    set(HEADER_POSTGRESQL
        ${INCLUDE_DIR}/vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h
        ${SRC_DIR}/experimental/ecs/postgresql/impl/postgresqlcomponent.h
        ${INCLUDE_DIR}/vigine/experimental/ecs/postgresql/impl/table.h
        ${INCLUDE_DIR}/vigine/experimental/ecs/postgresql/impl/column.h
    )

    set(SOURCES_POSTGRESQL
        ${SRC_DIR}/experimental/ecs/postgresql/impl/postgresqlcomponent.cpp
        ${SRC_DIR}/experimental/ecs/postgresql/impl/postgresqlsystem.cpp
        ${SRC_DIR}/experimental/ecs/postgresql/impl/table.cpp
        ${SRC_DIR}/experimental/ecs/postgresql/impl/column.cpp
    )

    set(SOURCES_POSTGRESQL_EXTRA
        include/vigine/experimental/ecs/postgresql/impl/databaseconfiguration.h src/experimental/ecs/postgresql/impl/databaseconfiguration.cpp
        include/vigine/experimental/ecs/postgresql/impl/connectiondata.h src/experimental/ecs/postgresql/impl/connectiondata.cpp
        include/vigine/experimental/ecs/postgresql/impl/data.h src/experimental/ecs/postgresql/impl/data.cpp
        include/vigine/experimental/ecs/postgresql/impl/columntype.h
        include/vigine/experimental/ecs/postgresql/impl/postgresqlresult.h src/experimental/ecs/postgresql/impl/postgresqlresult.cpp
        include/vigine/experimental/ecs/postgresql/impl/query/querybuilder.h src/experimental/ecs/postgresql/impl/query/querybuilder.cpp
        include/vigine/experimental/ecs/postgresql/impl/tablerows.h src/experimental/ecs/postgresql/impl/tablerows.cpp
        include/vigine/experimental/ecs/postgresql/impl/row.h src/experimental/ecs/postgresql/impl/row.cpp
        include/vigine/experimental/ecs/postgresql/impl/postgresqltypeconverter.h src/experimental/ecs/postgresql/impl/postgresqltypeconverter.cpp
    )
endif()

# ECS platform subsystem (#280) -- public IWindowEventHandlerComponent
# under api/, concrete WindowSystem + window backends under impl/.
set(HEADER_ECS_PLATFORM
    ${INCLUDE_DIR}/vigine/api/ecs/platform/iwindoweventhandler.h
    ${INCLUDE_DIR}/vigine/impl/ecs/platform/platformservice.h
    ${INCLUDE_DIR}/vigine/impl/ecs/platform/windowsystem.h
    ${SRC_DIR}/impl/ecs/platform/windowcomponent.h
    ${SRC_DIR}/impl/ecs/platform/windoweventdispatcher.h
)

set(SOURCES_ECS_PLATFORM
    ${SRC_DIR}/impl/ecs/platform/platformservice.cpp
    ${SRC_DIR}/impl/ecs/platform/windowsystem.cpp
    ${SRC_DIR}/impl/ecs/platform/windowcomponent.cpp
    ${SRC_DIR}/impl/ecs/platform/windoweventdispatcher.cpp
)

# Append every platform-specific source / header onto the per-subsystem
# lists. The active platform's fragment (cmake/platform/<X>/config.cmake)
# decides which window component, Vulkan surface factory, and OS-signal
# source get pulled in. The fragment was loaded by the platform
# dispatcher near the top of this file.
#
# Argument order (positional, see cmake/platform/platform.cmake):
#   1. HEADER_ECS_PLATFORM   -- per-platform window component header
#                               (winapicomponent.h / cocoawindowcomponent.h
#                               / xcbwindowbackend.h)
#   2. SOURCES_ECS_PLATFORM  -- per-platform window component source
#                               (winapicomponent.cpp /
#                               cocoawindowcomponent.mm /
#                               xcbwindowbackend.cpp)
#   3. SOURCES_ECS_GRAPHICS  -- per-platform Vulkan surface factory
#                               (win32surfacefactory.cpp /
#                               metalsurfacefactory.cpp /
#                               vulkan_surface_xcb.cpp)
#   4. SOURCES_EVENTSCHEDULER -- per-platform OS-signal source
#                                (iossignalsource_win.{h,cpp} /
#                                iossignalsource_macos.{h,mm} /
#                                iossignalsource_posix.{h,cpp})
vigine_platform_collect_sources(
    HEADER_ECS_PLATFORM
    SOURCES_ECS_PLATFORM
    SOURCES_ECS_GRAPHICS
    SOURCES_EVENTSCHEDULER
)

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
    ${HEADER_ECS_CORE}
    ${SOURCES_ECS_CORE}
    ${HEADER_STATEMACHINE_CORE}
    ${SOURCES_STATEMACHINE_CORE}
    ${HEADER_TASKFLOW_CORE}
    ${SOURCES_TASKFLOW_CORE}
    ${HEADER_CONTEXT_AGGREGATOR}
    ${SOURCES_CONTEXT_AGGREGATOR}
    ${HEADER_SIGNALEMITTER}
    ${SOURCES_SIGNALEMITTER}
    ${HEADER_EVENTSCHEDULER}
    ${SOURCES_EVENTSCHEDULER}
    ${HEADER_TOPICBUS}
    ${SOURCES_TOPICBUS}
    ${HEADER_CHANNELFACTORY}
    ${SOURCES_CHANNELFACTORY}
    ${HEADER_REQUESTBUS}
    ${SOURCES_REQUESTBUS}
    ${HEADER_REACTIVESTREAM}
    ${SOURCES_REACTIVESTREAM}
    ${HEADER_ACTORHOST}
    ${SOURCES_ACTORHOST}
    ${HEADER_PIPELINEBUILDER}
    ${SOURCES_PIPELINEBUILDER}
    ${HEADER_ENGINE}
    ${SOURCES_ENGINE}
    ${HEADER_ENGINE_TOKEN}
    ${SOURCES_ENGINE_TOKEN}
    ${HEADER_POSTGRESQL}
    ${SOURCES_POSTGRESQL}
    ${SOURCES_POSTGRESQL_EXTRA}
    ${HEADER_ECS_GRAPHICS}
    ${SOURCES_ECS_GRAPHICS}
    ${HEADER_ECS_PLATFORM}
    ${SOURCES_ECS_PLATFORM}
    include/vigine/base/name.h src/base/name.cpp
    include/vigine/base/password.h src/base/password.cpp
    include/vigine/base/macros.h
    include/vigine/base/info.h
)

# Apply the warning / strict-mode compile options to the vigine target.
# Scoped via target_compile_options inside the helper so FreeType and
# other vendored dependencies are not affected.
vigine_apply_compile_options(${PROJECT_NAME})

# Apply target-scoped platform configuration (compile definitions,
# include directories, link libraries). On Windows this pushes
# WIN32_LEAN_AND_MEAN PRIVATE so the bundled FreeType build does not
# see the macro on its compile line and emit C4005 redefinition. On
# macOS this links Cocoa / QuartzCore / Metal frameworks. On Linux
# this pulls in the XCB include path + library discovered by the
# fragment's setup_global() call.
vigine_platform_apply_target(${PROJECT_NAME})

# Apply sanitizer flags when VIGINE_SANITIZER is set. Guarded by the same
# condition used at include-time above so the helper is only called when the
# module has been loaded.
if(NOT VIGINE_SANITIZER STREQUAL "")
    vigine_apply_sanitizer(${PROJECT_NAME} ${VIGINE_SANITIZER})
endif()

# Include directories
# Three-layer include/src layout: core/ + api/ + impl/ (R-Structure v0.1.0)
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

if(VIGINE_ENABLE_EXPERIMENTAL AND ENABLE_POSTGRESQL)
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

# Platform-specific framework / include / link wiring lives in
# cmake/platform/<X>/config.cmake and is applied via
# vigine_platform_apply_target() above.

# Install the library and headers
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build/include)
