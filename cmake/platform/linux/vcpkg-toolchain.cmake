# Linux-specific wrapper around vcpkg toolchain.
if(NOT DEFINED VCPKG_ROOT)
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    elseif(EXISTS "$ENV{HOME}/vcpkg")
        set(VCPKG_ROOT "$ENV{HOME}/vcpkg")
    endif()
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-linux")
endif()

set(_VCPKG_TOOLCHAIN "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_VCPKG_TOOLCHAIN}")
    message(FATAL_ERROR "Cannot find vcpkg toolchain at ${_VCPKG_TOOLCHAIN}. Set VCPKG_ROOT.")
endif()

include("${_VCPKG_TOOLCHAIN}")
