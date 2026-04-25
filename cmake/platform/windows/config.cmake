# Windows platform fragment (R-PlatformPortability)
#
# Consolidates every Windows-specific block previously scattered
# through the root CMakeLists.txt. Loaded by cmake/platform/platform.cmake
# when CMAKE_SYSTEM_NAME == "Windows".

# vigine_platform_setup_global
#
# Directory-scope state that must land BEFORE add_library(vigine ...)
# is invoked: module path entries used by find_package(Vulkan), and
# Win32 compile definitions that pin the API surface and disable the
# legacy <windows.h> min/max macros.
#
# WIN32_LEAN_AND_MEAN is intentionally NOT pushed at directory scope:
# bundled FreeType (added via add_subdirectory ... EXCLUDE_FROM_ALL)
# would otherwise inherit it on its compile line and re-define the
# macro internally, producing C4005 macro-redefinition warnings.
# WIN32_LEAN_AND_MEAN is instead applied target-scoped to vigine in
# vigine_platform_apply_target() below.
function(vigine_platform_setup_global)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

    # Windows 10+ baseline. Pins every Win32 API surface to the
    # Windows 10 RTM (NTDDI_WIN10, value 0x0A000000); an older API
    # accidentally used elsewhere would fail to compile here. Bump
    # `_WIN32_WINNT` + `NTDDI_VERSION` together when a newer feature
    # set is needed (e.g. NTDDI_WIN10_VB = 0x0A000008 for 1903+).
    add_compile_definitions(
        VK_USE_PLATFORM_WIN32_KHR
        _WIN32_WINNT=0x0A00
        NTDDI_VERSION=0x0A000000
        NOMINMAX
    )
endfunction()

# vigine_platform_collect_sources
#
# Append Windows-specific source paths onto the caller-named lists.
# Two source families live here:
#   - the Vulkan WIN32_KHR surface factory under src/ecs/render/platform/,
#   - the OS signal source under src/eventscheduler/,
#   - the WinAPI window component under src/ecs/platform/.
function(vigine_platform_collect_sources headers_var sources_var)
    set(_headers "${${headers_var}}")
    set(_sources "${${sources_var}}")

    list(APPEND _sources
        "${SRC_DIR}/ecs/render/platform/win32surfacefactory.cpp"
        "${SRC_DIR}/eventscheduler/iossignalsource_win.h"
        "${SRC_DIR}/eventscheduler/iossignalsource_win.cpp"
    )

    list(APPEND _headers
        "${SRC_DIR}/ecs/platform/winapicomponent.h"
    )

    list(APPEND _sources
        "${SRC_DIR}/ecs/platform/winapicomponent.cpp"
    )

    set(${headers_var} "${_headers}" PARENT_SCOPE)
    set(${sources_var} "${_sources}" PARENT_SCOPE)
endfunction()

# vigine_platform_apply_target
#
# Target-scoped Windows configuration applied to `target` after
# add_library() has created it. WIN32_LEAN_AND_MEAN is scoped here
# (PRIVATE) so the bundled FreeType build does not see the macro on
# its own command line and emit C4005 redefinition.
function(vigine_platform_apply_target target)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN)
endfunction()
