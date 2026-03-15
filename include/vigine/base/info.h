#pragma once

namespace vigine
{
namespace info
{
enum class BuildType
{
    Unknown,
    Debug,
    Release
};

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
enum class Type
{
    Windows,
    Linux,
    MacOS,
    Unknown
};

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

[[nodiscard]] constexpr bool isWindows() { return currentPlatform() == Type::Windows; }

[[nodiscard]] constexpr bool isLinux() { return currentPlatform() == Type::Linux; }

[[nodiscard]] constexpr bool isMacOS() { return currentPlatform() == Type::MacOS; }

} // namespace platform
} // namespace info
} // namespace vigine
