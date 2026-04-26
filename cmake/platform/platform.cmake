# Platform dispatcher (R-PlatformPortability)
#
# Single entry point for selecting platform-specific CMake fragments.
# The root CMakeLists.txt includes this file once, early, before
# defining any compile options that depend on the target OS. The
# dispatcher resolves ${CMAKE_SYSTEM_NAME} (or honors a user-supplied
# ${VIGINE_PLATFORM}) and includes EXACTLY ONE
# cmake/platform/<platform>/config.cmake fragment.
#
# Adding a new platform requires only:
#   1. cmake/platform/<platform>/config.cmake (this dispatcher needs no edit)
#   2. src/impl/platform/<platform>/ concretes
#   3. a CI workflow snippet
#
# The fragment is expected to define three callbacks consumed by the
# root CMakeLists.txt:
#
#   vigine_platform_setup_global()
#       Runs immediately. Adjust CMAKE_MODULE_PATH, declare
#       directory-scope compile definitions, find packages whose
#       absence is fatal on this platform, etc.
#
#   vigine_platform_collect_sources(
#           <headers_ecs_platform_var>
#           <sources_ecs_platform_var>
#           <sources_ecs_graphics_var>
#           <sources_eventscheduler_var>)
#       Append platform-specific header / source paths onto the four
#       named lists. The lists target distinct subsystems:
#         1. ECS platform (window component header / source)
#         2. ECS graphics (Vulkan surface factory source)
#         3. Event scheduler (OS-signal source)
#       Each fragment appends its concretes via the SAME positional API
#       so the root CMakeLists.txt has a single dispatcher call and zero
#       if(WIN32 / APPLE / UNIX) branches (R-PlatformPortability).
#
#   vigine_platform_apply_target(<target>)
#       Apply target-scoped compile definitions, include directories,
#       and link libraries to the named target (typically `vigine`).
#       Called after add_library() has produced the target.

if(NOT DEFINED VIGINE_PLATFORM OR VIGINE_PLATFORM STREQUAL "")
    set(VIGINE_PLATFORM "${CMAKE_SYSTEM_NAME}")
endif()

# Normalise the resolved name regardless of whether it came from the
# user (-DVIGINE_PLATFORM=Windows) or from CMAKE_SYSTEM_NAME. The
# fragment lookup below is case-sensitive on POSIX filesystems, so a
# mixed-case input would fail to resolve cmake/platform/<x>/config.cmake.
string(TOLOWER "${VIGINE_PLATFORM}" VIGINE_PLATFORM)

# Map well-known aliases onto the canonical fragment-directory names
# (e.g. CMAKE_SYSTEM_NAME=Darwin -> "macos"). Add new entries here when
# adopting a new platform.
if(VIGINE_PLATFORM STREQUAL "darwin")
    set(VIGINE_PLATFORM "macos")
endif()

set(_vigine_platform_dir "${CMAKE_CURRENT_LIST_DIR}/${VIGINE_PLATFORM}")
if(NOT EXISTS "${_vigine_platform_dir}/config.cmake")
    message(FATAL_ERROR
        "Unsupported platform: '${CMAKE_SYSTEM_NAME}' (resolved to "
        "VIGINE_PLATFORM='${VIGINE_PLATFORM}'). No fragment found at "
        "${_vigine_platform_dir}/config.cmake. Add a new "
        "cmake/platform/${VIGINE_PLATFORM}/config.cmake to support this platform."
    )
endif()

include("${_vigine_platform_dir}/config.cmake")
