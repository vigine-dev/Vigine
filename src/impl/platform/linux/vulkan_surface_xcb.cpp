#if defined(__linux__)

#include "impl/ecs/graphics/surfacefactory.h"

#include <iostream>

#include <vulkan/vulkan_xcb.h>
#include <xcb/xcb.h>

namespace vigine
{
namespace ecs
{
namespace graphics
{

/**
 * @brief Vulkan surface factory for Linux via VK_KHR_xcb_surface.
 *
 * Expects nativeHandle to point to a XcbSurfaceHandles struct that carries
 * both the xcb_connection_t* and the xcb_window_t.  The struct is POD and
 * defined in this translation unit to keep the dependency inside
 * src/platform/linux/.
 *
 * Returns an empty vk::SurfaceKHR on any failure rather than throwing or
 * crashing — the engine checks isValid() before use.
 */

struct XcbSurfaceHandles
{
    xcb_connection_t *connection{nullptr};
    xcb_window_t      window{0};
};

class XcbSurfaceFactory : public SurfaceFactory
{
  public:
    vk::SurfaceKHR createSurface(vk::Instance instance, void *nativeHandle) override
    {
        if (!nativeHandle || !instance)
        {
            std::cerr << "XcbSurfaceFactory: null instance or handle" << std::endl;
            return vk::SurfaceKHR{};
        }

        const auto *handles = static_cast<const XcbSurfaceHandles *>(nativeHandle);
        if (!handles->connection || handles->window == 0)
        {
            std::cerr << "XcbSurfaceFactory: invalid XCB handles" << std::endl;
            return vk::SurfaceKHR{};
        }

        try
        {
            VkXcbSurfaceCreateInfoKHR createInfo{};
            createInfo.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            createInfo.connection = handles->connection;
            createInfo.window     = handles->window;

            VkSurfaceKHR rawSurface  = VK_NULL_HANDLE;
            const VkResult res = vkCreateXcbSurfaceKHR(
                static_cast<VkInstance>(instance), &createInfo, nullptr, &rawSurface);

            if (res != VK_SUCCESS)
            {
                std::cerr << "XcbSurfaceFactory: vkCreateXcbSurfaceKHR failed, VkResult="
                          << static_cast<int>(res) << std::endl;
                return vk::SurfaceKHR{};
            }

            std::cout << "Vulkan XCB surface created successfully" << std::endl;
            return vk::SurfaceKHR(rawSurface);
        }
        catch (const std::exception &e)
        {
            std::cerr << "XcbSurfaceFactory: exception: " << e.what() << std::endl;
            return vk::SurfaceKHR{};
        }
    }

    std::vector<const char *> requiredInstanceExtensions() const override
    {
        return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME};
    }
};

std::unique_ptr<SurfaceFactory> getPlatformSurfaceFactory()
{
    return std::make_unique<XcbSurfaceFactory>();
}

} // namespace graphics
} // namespace ecs
} // namespace vigine

#endif // __linux__
