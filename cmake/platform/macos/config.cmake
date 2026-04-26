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
# Append macOS-specific source paths onto the four caller-named lists.
# Each macOS source file targets one of the four lists by subsystem:
#   - ECS platform (window component header / source) -- Cocoa window
#     component lives at src/impl/ecs/platform/cocoawindowcomponent.{h,mm}
#   - ECS graphics (Vulkan surface factory source) -- Metal-backed factory
#     lives at src/impl/ecs/graphics/platform/metalsurfacefactory.cpp
#   - Event scheduler (OS-signal source) -- macOS signal source (.mm)
#     lives at src/impl/eventscheduler/iossignalsource_macos.{h,mm}
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
        "${SRC_DIR}/impl/ecs/platform/cocoawindowcomponent.h"
    )
    list(APPEND _sources_ecs_platform
        "${SRC_DIR}/impl/ecs/platform/cocoawindowcomponent.mm"
    )

    list(APPEND _sources_ecs_graphics
        "${SRC_DIR}/impl/ecs/graphics/platform/metalsurfacefactory.cpp"
    )

    list(APPEND _sources_eventscheduler
        "${SRC_DIR}/impl/eventscheduler/iossignalsource_macos.h"
        "${SRC_DIR}/impl/eventscheduler/iossignalsource_macos.mm"
    )

    set(${headers_ecs_platform_var} "${_headers_ecs_platform}" PARENT_SCOPE)
    set(${sources_ecs_platform_var} "${_sources_ecs_platform}" PARENT_SCOPE)
    set(${sources_ecs_graphics_var} "${_sources_ecs_graphics}" PARENT_SCOPE)
    set(${sources_eventscheduler_var} "${_sources_eventscheduler}" PARENT_SCOPE)
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
