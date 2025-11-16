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

constexpr BuildType buildType()
{
    if constexpr (BUILD_TYPE == DEBUG)
        {
            return BuildType::Debug;
        }
    else if constexpr (BUILD_TYPE == RELEASE)
        {
            return BuildType::Release;
        }
    else
        {
            return BuildType::Unknown;
        }
}
} // namespace info
} // namespace vigine
