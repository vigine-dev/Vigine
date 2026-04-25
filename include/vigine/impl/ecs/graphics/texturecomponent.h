#pragma once

#include "graphicshandles.h"

#include "vigine/base/macros.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace vigine
{
namespace ecs
{
namespace graphics
{

/**
 * @brief Texture ECS component.
 *
 * Stores CPU pixel data (or reference), dimensions, format, and GPU TextureHandle.
 * Used for SDF atlases, UI textures, and future texture-based rendering.
 */
class TextureComponent
{
  public:
    TextureComponent() = default;
    TextureComponent(uint32_t width, uint32_t height, TextureFormat format)
        : _width(width), _height(height), _format(format)
    {
    }

    // Dimensions
    void setDimensions(uint32_t width, uint32_t height)
    {
        if (_width != width || _height != height)
        {
            _width  = width;
            _height = height;
            markDirty();
        }
    }
    uint32_t width() const { return _width; }
    uint32_t height() const { return _height; }

    // Format
    void setFormat(TextureFormat format)
    {
        if (_format != format)
        {
            _format = format;
            markDirty();
        }
    }
    TextureFormat format() const { return _format; }

    // Pixel data (CPU side)
    void setPixelData(const std::vector<uint8_t> &pixels)
    {
        _pixelData = pixels;
        markDirty();
    }
    void setPixelData(std::vector<uint8_t> &&pixels)
    {
        _pixelData = std::move(pixels);
        markDirty();
    }
    const std::vector<uint8_t> &pixelData() const { return _pixelData; }

    // Sampler settings
    void setFilterMode(TextureFilter minFilter, TextureFilter magFilter)
    {
        _minFilter = minFilter;
        _magFilter = magFilter;
    }
    TextureFilter minFilter() const { return _minFilter; }
    TextureFilter magFilter() const { return _magFilter; }

    void setWrapMode(TextureWrapMode wrapU, TextureWrapMode wrapV)
    {
        _wrapU = wrapU;
        _wrapV = wrapV;
    }
    TextureWrapMode wrapU() const { return _wrapU; }
    TextureWrapMode wrapV() const { return _wrapV; }

    // GPU handle (assigned by backend after upload)
    void setTextureHandle(TextureHandle handle) { _textureHandle = handle; }
    TextureHandle textureHandle() const { return _textureHandle; }
    bool hasGpuTexture() const { return _textureHandle.isValid(); }

    // Descriptor set handle (backend-specific, for shader binding)
    void setDescriptorSet(uint64_t descriptorSet) { _descriptorSet = descriptorSet; }
    uint64_t descriptorSet() const { return _descriptorSet; }
    bool hasDescriptorSet() const { return _descriptorSet != 0; }

    // Generation counter (incremented when pixel data changes)
    uint32_t generation() const { return _generation; }
    void markDirty()
    {
        _generation++;
        _dirty = true;
    }
    bool isDirty() const { return _dirty; }
    void clearDirty() { _dirty = false; }

  private:
    uint32_t _width{0};
    uint32_t _height{0};
    TextureFormat _format{TextureFormat::RGBA8_UNORM};

    std::vector<uint8_t> _pixelData;

    TextureFilter _minFilter{TextureFilter::Linear};
    TextureFilter _magFilter{TextureFilter::Linear};
    TextureWrapMode _wrapU{TextureWrapMode::Repeat};
    TextureWrapMode _wrapV{TextureWrapMode::Repeat};

    TextureHandle _textureHandle{};
    uint64_t _descriptorSet{0};

    uint32_t _generation{0};
    bool _dirty{true};
};

using TextureComponentUPtr = std::unique_ptr<TextureComponent>;
using TextureComponentSPtr = std::shared_ptr<TextureComponent>;

} // namespace graphics
} // namespace ecs
} // namespace vigine
