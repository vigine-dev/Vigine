#pragma once

/**
 * @file isurfacefactory.h
 * @brief Marker interface for platform-specific Vulkan surface factories.
 *
 * The concrete SurfaceFactory in impl/ecs/graphics/surfacefactory.h pulls in
 * the Vulkan SDK headers, so it lives behind the public surface. This pure
 * interface keeps the ISurfaceFactory contract free of Vulkan-SDK types so
 * downstream callers can name a factory by interface without picking up the
 * Vulkan include path.
 *
 * Concrete factories may extend this interface and add their own backend-
 * specific entry points (e.g. vk::SurfaceKHR createSurface in the Vulkan
 * concrete) inside impl/.
 */

namespace vigine
{
namespace ecs
{
namespace graphics
{

/**
 * @brief Marker interface for platform surface factories.
 *
 * Held only as a polymorphic destruction handle today. Backend-specific
 * factory methods are added by concrete subclasses inside impl/ where
 * pulling Vulkan / Metal / D3D headers is acceptable.
 */
class ISurfaceFactory
{
  public:
    virtual ~ISurfaceFactory() = default;
};

} // namespace graphics
} // namespace ecs
} // namespace vigine
