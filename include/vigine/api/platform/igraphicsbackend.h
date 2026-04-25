#pragma once

/**
 * @file igraphicsbackend.h
 * @brief Pure-virtual platform graphics-backend abstraction (R-PlatformPortability skeleton).
 *
 * Lower-level platform graphics-context interface. Distinct from
 * @c include/vigine/ecs/render/graphicsbackend.h
 * (@c vigine::graphics::GraphicsBackend), which is the ECS rendering-
 * side abstraction over draw calls / pipelines / buffers / textures.
 *
 * Two layers, two responsibilities:
 *
 *   - @ref IGraphicsBackend (this file) selects the platform graphics
 *     API (Vulkan vs Metal vs D3D vs WebGPU) and reports whether the
 *     selected backend is available on the running host. It is the
 *     CPU-side platform-graphics-context selector.
 *   - @c vigine::graphics::GraphicsBackend (ECS side, from #280) is
 *     the rendering-side draw-call surface used by the ECS render
 *     system once a backend has been picked.
 *
 * TODO(#293): Reconcile with @c include/vigine/ecs/render/graphicsbackend.h
 * once the platform concretes land in @c src/impl/platform/<X>/. A
 * follow-up leaf will decide whether one wraps the other, or whether
 * the platform side feeds metadata into the ECS side.
 *
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies.
 */

#include <cstdint>

namespace vigine
{
namespace platform
{

/**
 * @brief Identifier for a platform graphics backend.
 *
 * The set of values is open-ended; future platforms (Android Vulkan,
 * iOS Metal, WebGPU) extend it without breaking existing concretes.
 */
enum class GraphicsBackendKind : std::uint8_t
{
    Unknown = 0,
    Vulkan,  ///< Desktop Vulkan (Linux, Windows, macOS-via-MoltenVK).
    Metal,   ///< Native macOS / iOS Metal.
    D3D12,   ///< Direct3D 12 (Windows-only).
    WebGPU,  ///< WebGPU (browser / Wasm).
};

/**
 * @brief Pure-virtual root interface for platform graphics-backend selection.
 *
 * The contract is intentionally narrow: report which backend kind
 * this concrete implements, and whether it is currently usable on
 * the running host (driver present, SDK loaded, runtime API version
 * sufficient, etc.). Higher-level rendering operations live on the
 * ECS-side @c vigine::graphics::GraphicsBackend interface.
 */
class IGraphicsBackend
{
  public:
    virtual ~IGraphicsBackend() = default;

    /**
     * @brief Identifier for the platform graphics backend that this
     *        concrete implements.
     */
    [[nodiscard]] virtual GraphicsBackendKind kind() const noexcept = 0;

    /**
     * @brief Whether the backend is available on the running host.
     *
     * Returns false when, e.g., a Vulkan ICD is missing, the Metal
     * device is not present, or a D3D12 feature level is too low.
     * Concrete implementations are responsible for the runtime probe.
     */
    [[nodiscard]] virtual bool isAvailable() const = 0;

    IGraphicsBackend(const IGraphicsBackend &)            = delete;
    IGraphicsBackend &operator=(const IGraphicsBackend &) = delete;
    IGraphicsBackend(IGraphicsBackend &&)                 = delete;
    IGraphicsBackend &operator=(IGraphicsBackend &&)      = delete;

  protected:
    IGraphicsBackend() = default;
};

} // namespace platform
} // namespace vigine
