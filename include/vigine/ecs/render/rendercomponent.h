#pragma once

#include "meshcomponent.h"
#include "textcomponent.h"
#include "transformcomponent.h"

#include "vigine/base/macros.h"

#include <glm/mat4x4.hpp>
#include <vector>

namespace vigine
{
namespace graphics
{

class RenderComponent
{
  public:
    enum class ShaderProfile
    {
        Cube,
        TextVoxel,
        Panel,
        Glyph,
        Sphere
    };

    RenderComponent();
    ~RenderComponent();

    void render();

    void setMesh(const MeshComponent &mesh);
    void setTransform(const TransformComponent &transform);
    bool setText(const TextComponent &textComponent);
    void setShaderProfile(ShaderProfile profile);
    void setPickable(bool pickable);

    MeshComponent &getMesh() { return _mesh; }
    const MeshComponent &getMesh() const { return _mesh; }
    TransformComponent &getTransform() { return _transform; }
    const TransformComponent &getTransform() const { return _transform; }
    TextComponent &getText() { return _text; }
    const TextComponent &getText() const { return _text; }
    ShaderProfile getShaderProfile() const { return _shaderProfile; }
    bool isPickable() const { return _pickable; }

    void appendModelMatrices(std::vector<glm::mat4> &modelMatrices) const;
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
    ShaderProfile _shaderProfile{ShaderProfile::Cube};
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

BUILD_SMART_PTR(RenderComponent);

} // namespace graphics
} // namespace vigine
