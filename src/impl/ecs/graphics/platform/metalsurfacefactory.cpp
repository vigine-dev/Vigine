#include "impl/ecs/graphics/surfacefactory.h"

#include <iostream>
#include <vulkan/vulkan_metal.h>

namespace vigine
{
namespace ecs
{
namespace graphics
{

class MetalSurfaceFactory : public SurfaceFactory
{
  public:
    vk::SurfaceKHR createSurface(vk::Instance instance, void *nativeHandle) override
    {
        auto *metalLayer = static_cast<const CAMetalLayer *>(nativeHandle);
        if (!metalLayer || !instance)
            return vk::SurfaceKHR{};

        try
        {
            VkMetalSurfaceCreateInfoEXT createInfo{};
            createInfo.sType        = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
            createInfo.pLayer       = metalLayer;

            VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
            const VkResult res      = vkCreateMetalSurfaceEXT(static_cast<VkInstance>(instance),
                                                              &createInfo, nullptr, &rawSurface);

            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create Metal surface, VkResult=" << static_cast<int>(res)
                          << std::endl;
                return vk::SurfaceKHR{};
            }

            std::cout << "Vulkan Metal surface created successfully" << std::endl;
            return vk::SurfaceKHR(rawSurface);
        } catch (const std::exception &e)
        {
            std::cerr << "Failed to create Metal surface: " << e.what() << std::endl;
            return vk::SurfaceKHR{};
        }
    }

    std::vector<const char *> requiredInstanceExtensions() const override
    {
        return {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                "VK_KHR_portability_enumeration"};
    }

    vk::InstanceCreateFlags instanceCreateFlags() const override
    {
        return vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    }
};

std::unique_ptr<SurfaceFactory> getPlatformSurfaceFactory()
{
    return std::make_unique<MetalSurfaceFactory>();
}

} // namespace graphics
} // namespace ecs
} // namespace vigine
