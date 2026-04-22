# VigineSanitizer.cmake
#
# Per-sanitizer compile + link flag helper. Provides vigine_apply_sanitizer()
# which applies the appropriate -fsanitize=... flags PRIVATE to a given target.
# Only GCC / Clang on Linux are supported; calling this on MSVC is a
# configuration error and will abort CMake with a clear message.
#
# Supported modes (case-insensitive):
#   asan   - Address Sanitizer: OOB reads/writes, UAF, double-free, leaks.
#   ubsan  - Undefined Behavior Sanitizer: null-deref, signed overflow,
#            type-punning, shift UB.
#   tsan   - Thread Sanitizer: data races across threads.
#
# Usage:
#   include(VigineSanitizer)
#   vigine_apply_sanitizer(vigine asan)
#
# Or via the VIGINE_SANITIZER CMake option:
#   cmake -DVIGINE_SANITIZER=asan ...

function(vigine_apply_sanitizer target mode)
    if(MSVC)
        message(FATAL_ERROR
            "vigine_apply_sanitizer: ASAN/UBSAN/TSAN require Clang on Linux. "
            "MSVC sanitizers are not supported by this helper.")
    endif()

    string(TOLOWER "${mode}" _san_lower)

    if(_san_lower STREQUAL "asan")
        set(_san_compile_flags
            -fsanitize=address
            -fno-omit-frame-pointer
            -g
        )
        set(_san_link_flags
            -fsanitize=address
        )

    elseif(_san_lower STREQUAL "ubsan")
        set(_san_compile_flags
            -fsanitize=undefined
            -fno-omit-frame-pointer
            -g
        )
        set(_san_link_flags
            -fsanitize=undefined
        )

    elseif(_san_lower STREQUAL "tsan")
        set(_san_compile_flags
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )
        set(_san_link_flags
            -fsanitize=thread
        )

    else()
        message(FATAL_ERROR
            "vigine_apply_sanitizer: unknown mode '${mode}'. "
            "Valid values: asan, ubsan, tsan.")
    endif()

    target_compile_options(${target} PRIVATE ${_san_compile_flags})
    target_link_options(${target}    PRIVATE ${_san_link_flags})
endfunction()
