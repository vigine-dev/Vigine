#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

/**
 * @brief Abstract factory for creating platform-specific Vulkan surfaces
 */
class SurfaceFactory
{
  public:
    virtual ~SurfaceFactory() = default;

    /**
     * @brief Create a Vulkan surface for the given native window handle
     * @param instance Vulkan instance
     * @param nativeHandle Platform-specific window handle (HWND on Windows, CAMetalLayer* on macOS)
     * @return Created surface, or null surface on failure
     */
    virtual vk::SurfaceKHR createSurface(vk::Instance instance, void *nativeHandle) = 0;

    /**
     * @brief Get required instance extensions for this platform
     * @return Vector of extension name strings
     */
    virtual std::vector<const char *> requiredInstanceExtensions() const = 0;

    /**
     * @brief Get optional instance creation flags for this platform
     * @return Instance create flags (e.g., portability enumeration on macOS)
     */
    virtual vk::InstanceCreateFlags instanceCreateFlags() const { return {}; }
};

/**
 * @brief Get the platform-specific surface factory
 * @return Owning unique_ptr to the appropriate platform surface factory
 */
std::unique_ptr<SurfaceFactory> getPlatformSurfaceFactory();

} // namespace graphics
} // namespace vigine
