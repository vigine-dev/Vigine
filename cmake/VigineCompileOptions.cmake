# Three-layer include/src layout: core/ + api/ + impl/ (R-Structure v0.1.0)
# VigineCompileOptions.cmake
#
# Central compile-option helper. Provides vigine_apply_compile_options()
# which applies the warning / strict-mode flag set that the vigine
# library (and only vigine, never its vendored dependencies) compiles
# under. Bundled third-party sources like FreeType must NOT inherit the
# stricter warning set or the log would drown in unrelated noise.
#
# Baseline:
#   * MSVC / clang-cl : /W4 /WX /permissive- /Zc:__cplusplus
#   * GCC / Clang     : -Wall -Wextra -Wpedantic -Werror
#   * C++ standard    : c++23, no GNU extensions (set at root via
#                       CMAKE_CXX_EXTENSIONS OFF)
#
# Warnings are now hard errors. The render-subsystem cleanup landed
# the in-house warning surface to zero; any new warning fails the
# build, preventing regressions from sneaking in unnoticed. Keep this
# scoped to the vigine target via target_compile_options so the
# bundled FreeType / GoogleTest / etc. translation units are not
# subjected to the stricter gate -- their own warnings are not under
# our control.

function(vigine_apply_compile_options target)
    if(MSVC)
        # /Zc:__cplusplus lets feature-test macros report the real
        # standard (MSVC otherwise reports 199711L for any -std setting).
        # /permissive- disables MSVC-specific language extensions;
        # paired with CMAKE_CXX_EXTENSIONS OFF it keeps the code ISO-C++
        # portable. /WX promotes /W4 warnings to errors.
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
    else()
        # GCC and Clang share the same flag spellings. -Wpedantic
        # catches GNU extensions we'd otherwise miss on Linux / macOS.
        # -Werror promotes -Wall / -Wextra / -Wpedantic warnings to
        # errors.
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
