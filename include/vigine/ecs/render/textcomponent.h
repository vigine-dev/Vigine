#pragma once

/**
 * @file textcomponent.h
 * @brief Text layout input for the SDF text renderer.
 */

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace vigine
{
namespace graphics
{
/**
 * @brief Single vertex of a glyph quad used by the SDF text shader.
 */
struct GlyphQuadVertex
{
    glm::vec3 pos;
    glm::vec2 uv;
};

/**
 * @brief World-space cursor position tied to a byte offset in the source text.
 */
struct CursorSlot
{
    std::size_t byteOffset;
    float worldX;
    float worldY;
};

/**
 * @brief Describes a renderable text string with layout and cursor state.
 *
 * Carries the text content, font path, pixel size, per-voxel scale,
 * anchor / wrap parameters, cursor byte position and visibility, and
 * optional per-character voxel offsets. Consumed by RenderComponent
 * which converts it to glyph quads via the SDF text pipeline.
 */
class TextComponent
{
  public:
    void setText(std::string value) { _text = std::move(value); }
    void setFontPath(std::string value) { _fontPath = std::move(value); }
    void setPixelSize(uint32_t value) { _pixelSize = value; }
    void setVoxelSize(float value) { _voxelSize = value; }
    void setEnabled(bool value) { _enabled = value; }
    void setDrawBaseInstance(bool value) { _drawBaseInstance = value; }
    void setVoxelOffsets(std::vector<glm::vec3> offsets) { _voxelOffsets = std::move(offsets); }
    void setTopLeftAnchor(bool value) { _topLeftAnchor = value; }
    void setAnchorOffset(glm::vec2 value) { _anchorOffset = value; }
    // Maximum line width in world units; 0 = no wrap.
    void setMaxLineWorldWidth(float value) { _maxLineWorldWidth = value; }
    void setCursorBytePos(std::size_t pos) { _cursorBytePos = pos; }
    void setCursorVisible(bool value) { _cursorVisible = value; }

    [[nodiscard]] const std::string &text() const { return _text; }
    [[nodiscard]] const std::string &fontPath() const { return _fontPath; }
    [[nodiscard]] uint32_t pixelSize() const { return _pixelSize; }
    [[nodiscard]] float voxelSize() const { return _voxelSize; }
    [[nodiscard]] bool enabled() const { return _enabled; }
    [[nodiscard]] bool drawBaseInstance() const { return _drawBaseInstance; }
    [[nodiscard]] bool topLeftAnchor() const { return _topLeftAnchor; }
    [[nodiscard]] const glm::vec2 &anchorOffset() const { return _anchorOffset; }
    [[nodiscard]] float maxLineWorldWidth() const { return _maxLineWorldWidth; }
    [[nodiscard]] std::size_t cursorBytePos() const { return _cursorBytePos; }
    [[nodiscard]] bool cursorVisible() const { return _cursorVisible; }
    [[nodiscard]] const std::vector<glm::vec3> &voxelOffsets() const { return _voxelOffsets; }

  private:
    std::string _text;
    std::string _fontPath;
    uint32_t _pixelSize{24};
    float _voxelSize{0.03f};
    bool _enabled{false};
    bool _drawBaseInstance{true};
    bool _topLeftAnchor{false};
    glm::vec2 _anchorOffset{0.0f, 0.0f};
    float _maxLineWorldWidth{0.0f};
    std::size_t _cursorBytePos{0};
    bool _cursorVisible{false};
    std::vector<glm::vec3> _voxelOffsets;
};

} // namespace graphics
} // namespace vigine
