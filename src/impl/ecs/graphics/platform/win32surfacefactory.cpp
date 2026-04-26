#include "impl/ecs/graphics/surfacefactory.h"

#include <iostream>
#include <vulkan/vulkan_win32.h>
#include <windows.h>

namespace vigine
{
namespace ecs
{
namespace graphics
{

class Win32SurfaceFactory : public SurfaceFactory
{
  public:
    vk::SurfaceKHR createSurface(vk::Instance instance, void *nativeHandle) override
    {
        HWND hwnd = reinterpret_cast<HWND>(nativeHandle);
        if (!hwnd || !instance)
            return vk::SurfaceKHR{};

        try
        {
            VkWin32SurfaceCreateInfoKHR createInfo{};
            createInfo.sType            = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            createInfo.hwnd             = hwnd;
            createInfo.hinstance        = GetModuleHandleA(nullptr);

            VkSurfaceKHR rawSurface     = VK_NULL_HANDLE;
            const VkResult createResult = vkCreateWin32SurfaceKHR(
                static_cast<VkInstance>(instance), &createInfo, nullptr, &rawSurface);

            if (createResult != VK_SUCCESS)
            {
                std::cerr << "Failed to create Win32 surface, VkResult="
                          << static_cast<int>(createResult) << std::endl;
                return vk::SurfaceKHR{};
            }

            std::cout << "Vulkan Win32 surface created successfully" << std::endl;
            return vk::SurfaceKHR(rawSurface);
        } catch (const std::exception &e)
        {
            std::cerr << "Failed to create Win32 surface: " << e.what() << std::endl;
            return vk::SurfaceKHR{};
        }
    }

    std::vector<const char *> requiredInstanceExtensions() const override
    {
        return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    }
};

std::unique_ptr<SurfaceFactory> getPlatformSurfaceFactory()
{
    return std::make_unique<Win32SurfaceFactory>();
}

} // namespace graphics
} // namespace ecs
} // namespace vigine
