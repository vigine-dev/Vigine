# FindVulkanHpp.cmake
# Finds Vulkan-Hpp (header-only) — supports being provided by parent project
#
# Variables set:
#   VulkanHpp_FOUND (INTERNAL CACHE BOOL)
#   VulkanHpp_INCLUDE_DIRS (CACHE STRING)
#   VulkanHpp_LIBRARIES (CACHE STRING)  -- target alias to link with (VulkanHpp::VulkanHpp)

# Name of internal target to create (without namespace)
set(TARGET_VULKAN_HPP "VulkanHpp" CACHE INTERNAL "Internal name for VulkanHpp target")
set(TARGET_VULKAN_HPP_ALIAS ${TARGET_VULKAN_HPP}::${TARGET_VULKAN_HPP})

# If target or namespaced alias already provided by parent, mark found and return
if(TARGET ${TARGET_VULKAN_HPP} OR TARGET ${TARGET_VULKAN_HPP_ALIAS})
    set(VulkanHpp_FOUND TRUE CACHE INTERNAL "VulkanHpp already provided by parent")
    # Try to set VulkanHpp_LIBRARIES if alias exists
    if(TARGET ${TARGET_VULKAN_HPP_ALIAS})
        set(VulkanHpp_LIBRARIES "${TARGET_VULKAN_HPP_ALIAS}" CACHE STRING "VulkanHpp target")
    elseif(TARGET ${TARGET_VULKAN_HPP})
        set(VulkanHpp_LIBRARIES "${TARGET_VULKAN_HPP}" CACHE STRING "VulkanHpp target")
    endif()
    return()
endif()

# Default search path (can be overridden with -DVULKAN_HPP_DIR=...)
if(NOT DEFINED VULKAN_HPP_DIR)
    set(VULKAN_HPP_DIR "${CMAKE_SOURCE_DIR}/external/vulkan-hpp")
endif()

set(VulkanHpp_FOUND FALSE)
set(VulkanHpp_INCLUDE_DIRS "")

# Try a few common candidate locations relative to project
set(_candidates
    "${VULKAN_HPP_DIR}"
)

foreach(_candidate IN LISTS _candidates)
    if(_candidate AND EXISTS "${_candidate}/vulkan/vulkan.hpp")
        set(VulkanHpp_INCLUDE_DIRS "${_candidate}")
        set(VulkanHpp_FOUND TRUE)
        break()
    endif()
endforeach()

if(VulkanHpp_FOUND)
    # Create INTERFACE target only if it does not exist
    if(NOT TARGET ${TARGET_VULKAN_HPP})
        add_library(${TARGET_VULKAN_HPP} INTERFACE)
        target_include_directories(${TARGET_VULKAN_HPP} INTERFACE "${VulkanHpp_INCLUDE_DIRS}")
    endif()

    # Create namespaced alias if not present (requires real target to exist)
    if(NOT TARGET ${TARGET_VULKAN_HPP_ALIAS})
        add_library(${TARGET_VULKAN_HPP_ALIAS} ALIAS ${TARGET_VULKAN_HPP})
    endif()

    # Export variables to cache for callers
    set(VulkanHpp_INCLUDE_DIRS "${VulkanHpp_INCLUDE_DIRS}" CACHE STRING "VulkanHpp include directory")
    set(VulkanHpp_FOUND TRUE CACHE INTERNAL "VulkanHpp found")
    set(VulkanHpp_LIBRARIES "${TARGET_VULKAN_HPP_ALIAS}" CACHE STRING "VulkanHpp target to link with")
else()
    set(VulkanHpp_FOUND FALSE CACHE INTERNAL "VulkanHpp not found")
    # Ensure the LIBRARIES variable is unset/empty in cache
    set(VulkanHpp_LIBRARIES "" CACHE STRING "VulkanHpp target to link with")
endif()

# Hide internal variable from CMake GUIs
if(COMMAND mark_as_advanced)
    mark_as_advanced(VulkanHpp_INCLUDE_DIRS)
endif()

unset(TARGET_VULKAN_HPP)
unset(TARGET_VULKAN_HPP_ALIAS)
unset(_candidates)
unset(_candidate)
