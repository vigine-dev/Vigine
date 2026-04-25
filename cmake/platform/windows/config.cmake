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
# Append Windows-specific source paths onto the four caller-named lists.
# Each Windows source file targets one of the four lists by subsystem:
#   - ECS platform (window component header / source) -- WinAPI window
#     component lives at src/impl/ecs/platform/winapicomponent.{h,cpp}
#   - ECS graphics (Vulkan surface factory source) -- WIN32_KHR factory
#     lives at src/impl/ecs/graphics/platform/win32surfacefactory.cpp
#   - Event scheduler (OS-signal source) -- Win32 signal source lives at
#     src/eventscheduler/iossignalsource_win.{h,cpp}
function(vigine_platform_collect_sources
        headers_ecs_platform_var
        sources_ecs_platform_var
        sources_ecs_graphics_var
        sources_eventscheduler_var)
    set(_headers_ecs_platform "${${headers_ecs_platform_var}}")
    set(_sources_ecs_platform "${${sources_ecs_platform_var}}")
    set(_sources_ecs_graphics "${${sources_ecs_graphics_var}}")
    set(_sources_eventscheduler "${${sources_eventscheduler_var}}")

    list(APPEND _headers_ecs_platform
        "${SRC_DIR}/impl/ecs/platform/winapicomponent.h"
    )
    list(APPEND _sources_ecs_platform
        "${SRC_DIR}/impl/ecs/platform/winapicomponent.cpp"
    )

    list(APPEND _sources_ecs_graphics
        "${SRC_DIR}/impl/ecs/graphics/platform/win32surfacefactory.cpp"
    )

    list(APPEND _sources_eventscheduler
        "${SRC_DIR}/eventscheduler/iossignalsource_win.h"
        "${SRC_DIR}/eventscheduler/iossignalsource_win.cpp"
    )

    set(${headers_ecs_platform_var} "${_headers_ecs_platform}" PARENT_SCOPE)
    set(${sources_ecs_platform_var} "${_sources_ecs_platform}" PARENT_SCOPE)
    set(${sources_ecs_graphics_var} "${_sources_ecs_graphics}" PARENT_SCOPE)
    set(${sources_eventscheduler_var} "${_sources_eventscheduler}" PARENT_SCOPE)
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
