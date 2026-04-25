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
#   vigine_platform_collect_sources(<headers_list_var> <sources_list_var>)
#       Append platform-specific header / source paths onto the named
#       lists. Used to compose the SOURCES_PLATFORM / HEADER_PLATFORM
#       sets and the platform-specific surface factory + os-signal
#       source pulled into the vigine target.
#
#   vigine_platform_apply_target(<target>)
#       Apply target-scoped compile definitions, include directories,
#       and link libraries to the named target (typically `vigine`).
#       Called after add_library() has produced the target.

if(NOT DEFINED VIGINE_PLATFORM OR VIGINE_PLATFORM STREQUAL "")
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _vigine_platform_lower)
    if(_vigine_platform_lower STREQUAL "darwin")
        set(VIGINE_PLATFORM "macos")
    else()
        set(VIGINE_PLATFORM "${_vigine_platform_lower}")
    endif()
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
