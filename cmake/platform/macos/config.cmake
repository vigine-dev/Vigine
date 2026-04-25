# macOS platform fragment (R-PlatformPortability)
#
# Consolidates every macOS-specific block previously scattered through
# the root CMakeLists.txt. Loaded by cmake/platform/platform.cmake when
# CMAKE_SYSTEM_NAME == "Darwin".

# vigine_platform_setup_global
#
# Directory-scope state that must land before vigine is added.
function(vigine_platform_setup_global)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

# vigine_platform_collect_sources
#
# Append macOS-specific source paths onto the caller-named lists. The
# Metal-backed Vulkan surface factory + the macOS OS-signal source
# (Objective-C++ .mm) + the Cocoa window component live here.
function(vigine_platform_collect_sources headers_var sources_var)
    set(_headers "${${headers_var}}")
    set(_sources "${${sources_var}}")

    list(APPEND _sources
        "${SRC_DIR}/ecs/render/platform/metalsurfacefactory.cpp"
        "${SRC_DIR}/eventscheduler/iossignalsource_macos.h"
        "${SRC_DIR}/eventscheduler/iossignalsource_macos.mm"
    )

    list(APPEND _headers
        "${SRC_DIR}/ecs/platform/cocoawindowcomponent.h"
    )

    list(APPEND _sources
        "${SRC_DIR}/ecs/platform/cocoawindowcomponent.mm"
    )

    set(${headers_var} "${_headers}" PARENT_SCOPE)
    set(${sources_var} "${_sources}" PARENT_SCOPE)
endfunction()

# vigine_platform_apply_target
#
# Target-scoped macOS configuration applied to `target` after
# add_library() has created it. Links the Cocoa / QuartzCore / Metal
# frameworks consumed by the Cocoa window backend and the Metal
# surface factory.
function(vigine_platform_apply_target target)
    target_link_libraries(${target}
        PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
    )
endfunction()
