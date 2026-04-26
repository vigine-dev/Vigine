#pragma once

/**
 * @file info.h
 * @brief Compile-time build and platform introspection.
 *
 * All helpers in this header are @c constexpr so that downstream code can
 * branch on build flavour (Debug/Release) and host platform (Windows/Linux/
 * macOS) at compile time.
 */

namespace vigine
{
namespace info
{
/**
 * @brief Build flavour the engine was compiled with.
 */
enum class BuildType
{
    Unknown,
    Debug,
    Release
};

/**
 * @brief Returns the @ref BuildType that matches the compile-time
 *        @c BUILD_TYPE / @c DEBUG / @c RELEASE macros.
 *
 * Falls back to @c BuildType::Unknown when the macros are not recognised,
 * keeping the helper total so call sites can rely on it in a @c constexpr
 * context.
 */
[[nodiscard]] constexpr BuildType buildType()
{
    if constexpr (BUILD_TYPE == DEBUG)
    {
        return BuildType::Debug;
    } else if constexpr (BUILD_TYPE == RELEASE)
    {
        return BuildType::Release;
    } else
    {
        return BuildType::Unknown;
    }
}

namespace platform
{
/**
 * @brief Host operating system family the translation unit is compiled for.
 */
enum class Type
{
    Windows,
    Linux,
    MacOS,
    Unknown
};

/**
 * @brief Returns the platform @ref Type the current translation unit is
 *        being compiled for, based on standard preprocessor macros.
 */
[[nodiscard]] constexpr Type currentPlatform()
{
#if defined(_WIN32) || defined(_WIN64)
    return Type::Windows;
#elif defined(__linux__)
    return Type::Linux;
#elif defined(__APPLE__) && defined(__MACH__)
    return Type::MacOS;
#else
    return Type::Unknown;
#endif
}

/** @brief @c true when the current platform is Windows. */
[[nodiscard]] constexpr bool isWindows() { return currentPlatform() == Type::Windows; }

/** @brief @c true when the current platform is Linux. */
[[nodiscard]] constexpr bool isLinux() { return currentPlatform() == Type::Linux; }

/** @brief @c true when the current platform is macOS. */
[[nodiscard]] constexpr bool isMacOS() { return currentPlatform() == Type::MacOS; }

} // namespace platform
} // namespace info
} // namespace vigine
