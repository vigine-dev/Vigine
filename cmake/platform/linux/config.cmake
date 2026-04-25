# Linux platform fragment (R-PlatformPortability)
#
# Consolidates every Linux-specific block previously scattered through
# the root CMakeLists.txt. Loaded by cmake/platform/platform.cmake when
# CMAKE_SYSTEM_NAME == "Linux".

# Cache results of find_package / pkg_check_modules between callbacks
# so vigine_platform_apply_target can pick up the include / link
# values that vigine_platform_setup_global discovered.
set(VIGINE_LINUX_XCB_INCLUDE_DIRS "" CACHE INTERNAL "Linux XCB include dirs")
set(VIGINE_LINUX_XCB_LIBRARIES "" CACHE INTERNAL "Linux XCB libraries")

# vigine_platform_setup_global
#
# Directory-scope state that must land before vigine is added: module
# path for find_*.cmake helpers, and an XCB discovery so missing
# system headers fail at configure rather than compile.
function(vigine_platform_setup_global)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

    # libxcb is required for the XCB window backend + VK_KHR_xcb_surface.
    # The pkg_check_modules() call below intentionally omits REQUIRED
    # so the manual XCB_FOUND check can emit a hint pointing the
    # developer at the right system package.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB xcb)
    if(NOT XCB_FOUND)
        message(FATAL_ERROR "libxcb not found. Install libxcb-dev (Ubuntu) or libxcb-devel (Fedora).")
    endif()

    # Cache for vigine_platform_apply_target to pick up.
    set(VIGINE_LINUX_XCB_INCLUDE_DIRS "${XCB_INCLUDE_DIRS}" CACHE INTERNAL "Linux XCB include dirs" FORCE)
    set(VIGINE_LINUX_XCB_LIBRARIES "${XCB_LIBRARIES}" CACHE INTERNAL "Linux XCB libraries" FORCE)
endfunction()

# vigine_platform_collect_sources
#
# Append Linux-specific source paths onto the caller-named lists. The
# Vulkan XCB surface factory + the POSIX OS-signal source + the XCB
# window backend live here.
function(vigine_platform_collect_sources headers_var sources_var)
    set(_headers "${${headers_var}}")
    set(_sources "${${sources_var}}")

    list(APPEND _sources
        "${SRC_DIR}/platform/linux/vulkan_surface_xcb.cpp"
        "${SRC_DIR}/platform/linux/iossignalsource_posix.h"
        "${SRC_DIR}/platform/linux/iossignalsource_posix.cpp"
    )

    list(APPEND _headers
        "${SRC_DIR}/platform/linux/xcbwindowbackend.h"
    )

    list(APPEND _sources
        "${SRC_DIR}/platform/linux/xcbwindowbackend.cpp"
    )

    set(${headers_var} "${_headers}" PARENT_SCOPE)
    set(${sources_var} "${_sources}" PARENT_SCOPE)
endfunction()

# vigine_platform_apply_target
#
# Target-scoped Linux configuration applied to `target` after
# add_library() has created it. Pulls in the XCB include path + link
# library that pkg_check_modules discovered above.
function(vigine_platform_apply_target target)
    target_include_directories(${target}
        PRIVATE
        ${VIGINE_LINUX_XCB_INCLUDE_DIRS}
    )

    target_link_libraries(${target}
        PRIVATE
        ${VIGINE_LINUX_XCB_LIBRARIES}
    )
endfunction()
