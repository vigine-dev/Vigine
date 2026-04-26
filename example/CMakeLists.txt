cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

project(example-projects)

# The window example migrated to the modern
# @c vigine::engine::IEngine front door + the modern service
# container, so it defaults ON on platforms its skip-guard accepts
# (Win32 today; @c example/window/CMakeLists.txt early-returns on
# non-Windows hosts, keeping a Linux / macOS configure clean).
#
# The PostgreSQL example still consumes the legacy
# @c vigine::Context::service(id, name, Property) factory and stays
# gated OFF until its own consumer migration ships through #329.
option(BUILD_EXAMPLE_POSTGRESQL "Build combined PostgreSQL example project (requires #329 consumer migration)" OFF)
option(BUILD_EXAMPLE_WINDOW "Build example-window project" ON)

# Postgres demo lives under example/experimental and is gated by the
# experimental umbrella so the default build never tries to link
# libpqxx through the example tree.
if(VIGINE_ENABLE_EXPERIMENTAL AND BUILD_EXAMPLE_POSTGRESQL)
    add_subdirectory(experimental/postgres_demo)
endif()

if(BUILD_EXAMPLE_WINDOW)
    add_subdirectory(window)
endif()

# New examples landing under the v0.1.0 readiness umbrella. Each
# stub directory currently holds only a placeholder file, so the
# `EXISTS` guard makes today's configure step a clean no-op even
# when the matching option is flipped ON. A follow-up PR replaces
# each stub with a real CMakeLists.txt + sources.
if(BUILD_EXAMPLE_THREADED_BUS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/threaded_bus/CMakeLists.txt")
    add_subdirectory(threaded_bus)
endif()

if(BUILD_EXAMPLE_PARALLEL_FSM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/parallel_fsm/CMakeLists.txt")
    add_subdirectory(parallel_fsm)
endif()

if(BUILD_EXAMPLE_FANOUT_FSM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fanout_fsm/CMakeLists.txt")
    add_subdirectory(fanout_fsm)
endif()

