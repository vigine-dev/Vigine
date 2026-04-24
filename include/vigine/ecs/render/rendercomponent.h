#pragma once

/**
 * @file rendercomponent.h
 * @brief Per-entity renderable aggregating mesh, transform, text, and shader.
 */

#include "graphicshandles.h"
#include "meshcomponent.h"
#include "shadercomponent.h"
#include "textcomponent.h"
#include "transformcomponent.h"

#include <glm/mat4x4.hpp>
#include <memory>
#include <vector>

namespace vigine
{
namespace graphics
{

/**
 * @brief Render payload for one Entity.
 *
 * Bundles mesh, transform, optional text, and shader state for a
 * single renderable. Exposes getters for each sub-component, a
 * picking flag, an optional texture binding, and helpers to emit
 * model matrices and SDF glyph quads. Also manages an incremental
 * per-visual-line cache for SDF text layout (rebuildSdfGlyphQuads,
 * incrementalRebuildSdf, scrollVertical, translateGlyphVertices).
 */
class RenderComponent
{
  public:
    RenderComponent();
    ~RenderComponent();

    void render();

    void setMesh(const MeshComponent &mesh);
    void setTransform(const TransformComponent &transform);
    bool setText(const TextComponent &textComponent);
    void setShader(const ShaderComponent &shader);
    void setPickable(bool pickable);
    void setTextureHandle(TextureHandle handle);

    MeshComponent &getMesh() { return _mesh; }
    const MeshComponent &getMesh() const { return _mesh; }
    TransformComponent &getTransform() { return _transform; }
    const TransformComponent &getTransform() const { return _transform; }
    TextComponent &getText() { return _text; }
    const TextComponent &getText() const { return _text; }
    ShaderComponent &getShader() { return _shader; }
    const ShaderComponent &getShader() const { return _shader; }
    bool isPickable() const { return _pickable; }
    TextureHandle textureHandle() const { return _textureHandle; }

    void appendModelMatrices(std::vector<glm::mat4> &modelMatrices) const;
    void appendModelMatrices(std::vector<glm::mat4> &modelMatrices,
                             const glm::mat4 &anchorOverride) const;
    void appendGlyphQuadVertices(std::vector<GlyphQuadVertex> &vertices) const;
    const std::vector<uint8_t> *getSdfAtlasPixels() const;
    uint32_t getSdfAtlasGeneration() const;
    void updateCursorVisible(bool visible);
    float sdfContentHeight() const { return _sdfContentHeight; }
    const std::vector<CursorSlot> &cursorSlots() const { return _cursorSlots; }
    std::vector<CursorSlot> takeCursorSlots() { return std::move(_cursorSlots); }
    bool refreshSdfGlyphQuads();
    bool incrementalRebuildSdf(std::size_t editBytePos);
    // Shift all cached world vertices by deltaAnchorOffsetY world units (for scroll).
    // O(total_verts) vector additions — no layout rebuild.
    void scrollVertical(float deltaAnchorOffsetY);
    // Translate all glyph vertices by a world-space delta (for panel movement).
    // O(total_verts) — does not change anchorOffset or trigger re-layout.
    void translateGlyphVertices(const glm::vec3 &delta);

  private:
    bool rebuildTextVoxelOffsets();
    bool rebuildSdfGlyphQuads();
    void emitWorldVerticesFromLines(std::size_t dirtyFrom, std::size_t dirtyEnd, float cursorPenX,
                                    float cursorPenY);

    struct SdfQuadPx
    {
        float x0, y0, x1, y1;
        float u0, v0, u1, v1;
    };

    // Per-visual-line cache for incremental rebuild.
    struct LineCacheEntry
    {
        std::size_t byteBegin{0};      // first byte offset of this visual line
        std::size_t byteEnd{0};        // byte offset past the last char of this line
        float penYStart{0.0f};         // penY at the start of this line
        float penXEnd{0.0f};           // penX after last char of this line
        std::vector<SdfQuadPx> quads;  // pixel-space quads for this line
        std::vector<CursorSlot> slots; // pixel-space cursor slots for this line
        std::vector<GlyphQuadVertex> cachedWorldVerts; // world-space verts (6 per quad)
        float localMinX{0.0f}, localMaxX{0.0f};
        float localMinY{0.0f}, localMaxY{0.0f};
    };

    MeshComponent _mesh;
    TransformComponent _transform;
    TextComponent _text;
    ShaderComponent _shader;
    TextureHandle _textureHandle;
    bool _pickable{true};
    std::vector<GlyphQuadVertex> _cachedBodyWorldVertices;
    std::vector<GlyphQuadVertex> _cachedCursorWorldVertices;
    std::vector<CursorSlot> _cursorSlots;
    std::vector<SdfQuadPx> _bodyQuadsPx;
    std::vector<LineCacheEntry> _lineCache;
    float _sdfContentHeight{0.0f};
    float _cachedAnchorX{1e30f}; // sentinel — forces full re-transform on first call
    float _cachedAnchorY{1e30f};
};

using RenderComponentUPtr = std::unique_ptr<RenderComponent>;
using RenderComponentSPtr = std::shared_ptr<RenderComponent>;

} // namespace graphics
} // namespace vigine
