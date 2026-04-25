#include "vigine/ecs/render/rendercomponent.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

namespace
{
std::vector<uint32_t> decodeUtf8(const std::string &text)
{
    std::vector<uint32_t> codepoints;

    for (std::size_t i = 0; i < text.size();)
    {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        uint32_t codepoint       = 0;
        std::size_t extraBytes   = 0;

        if ((byte & 0x80u) == 0u)
        {
            codepoint  = byte;
            extraBytes = 0;
        } else if ((byte & 0xE0u) == 0xC0u)
        {
            codepoint  = byte & 0x1Fu;
            extraBytes = 1;
        } else if ((byte & 0xF0u) == 0xE0u)
        {
            codepoint  = byte & 0x0Fu;
            extraBytes = 2;
        } else if ((byte & 0xF8u) == 0xF0u)
        {
            codepoint  = byte & 0x07u;
            extraBytes = 3;
        } else
        {
            ++i;
            continue;
        }

        if (i + extraBytes >= text.size())
            break;

        bool valid = true;
        for (std::size_t j = 1; j <= extraBytes; ++j)
        {
            const unsigned char nextByte = static_cast<unsigned char>(text[i + j]);
            if ((nextByte & 0xC0u) != 0x80u)
            {
                valid = false;
                break;
            }

            codepoint = (codepoint << 6u) | static_cast<uint32_t>(nextByte & 0x3Fu);
        }

        if (valid)
            codepoints.push_back(codepoint);

        i += (extraBytes + 1);
    }

    return codepoints;
}
// --- FreeType singletons ---------------------------------------------------
// FT_Library is expensive to init; keep one for the process lifetime.
// FT_Face is expensive to load; cache by (fontPath, pixelSize).
FT_Library ftLibrarySingleton()
{
    static FT_Library lib = nullptr;
    if (!lib)
        FT_Init_FreeType(&lib);
    return lib;
}

struct FaceKey
{
    std::string fontPath;
    uint32_t pixelSize{};
    bool operator<(const FaceKey &o) const
    {
        if (fontPath != o.fontPath)
            return fontPath < o.fontPath;
        return pixelSize < o.pixelSize;
    }
};

FT_Face cachedFace(const std::string &fontPath, uint32_t pixelSize)
{
    static std::map<FaceKey, FT_Face> cache;
    FaceKey key{fontPath, pixelSize};
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    FT_Face face = nullptr;
    if (FT_New_Face(ftLibrarySingleton(), fontPath.c_str(), 0, &face) != 0)
        return nullptr;
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
    static_cast<void>(FT_Select_Charmap(face, FT_ENCODING_UNICODE));
    cache[key] = face;
    return face;
}

// --- SDF Font Atlas --------------------------------------------------------
// Atlas size constants: 1024x1024 pixels, glyphs rendered at pixelSize with
// FT_RENDER_MODE_SDF. Each glyph gets a slot of (pixelSize+4) x (pixelSize+4)
// with 2-pixel SDF padding.
constexpr uint32_t kAtlasSize = 1024;
constexpr int kPad            = 2; // SDF border padding in pixels

struct GlyphInfo
{
    int atlasX{0}, atlasY{0};
    int bitmapW{0}, bitmapH{0};
    int bearingX{0}, bearingY{0};
    int advanceX{0};
    bool valid{false};
};

struct FontAtlas
{
    std::vector<uint8_t> pixels; // kAtlasSize * kAtlasSize, R8
    std::unordered_map<uint32_t, GlyphInfo> glyphs;
    int penAtlasX{0};
    int penAtlasY{0};
    int rowHeight{0};
    bool built{false};
    uint32_t generation{0};
};

struct AtlasKey
{
    std::string fontPath;
    uint32_t pixelSize{};
    bool operator<(const AtlasKey &o) const
    {
        if (fontPath != o.fontPath)
            return fontPath < o.fontPath;
        return pixelSize < o.pixelSize;
    }
};

// Returns (atlas, glyphInfo-or-null) for a codepoint.
// Builds atlas lazily on first call per codepoint.
FontAtlas &cachedAtlas(const std::string &fontPath, uint32_t pixelSize)
{
    static std::map<AtlasKey, FontAtlas> atlases;
    AtlasKey key{fontPath, pixelSize};
    auto &atlas = atlases[key];
    if (!atlas.built)
    {
        atlas.pixels.assign(kAtlasSize * kAtlasSize, 0);
        atlas.built = true;
    }
    return atlas;
}

const GlyphInfo &ensureGlyph(FontAtlas &atlas, FT_Face face, uint32_t codepoint,
                             [[maybe_unused]] uint32_t pixelSize)
{
    auto it = atlas.glyphs.find(codepoint);
    if (it != atlas.glyphs.end())
        return it->second;

    GlyphInfo info;
    FT_UInt gi = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
    if (gi == 0 && codepoint != static_cast<uint32_t>(' '))
        gi = FT_Get_Char_Index(face, static_cast<FT_ULong>('?'));

    if (gi != 0 && FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) == 0 &&
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF) == 0)
    {
        const auto &bm  = face->glyph->bitmap;
        const int bw    = static_cast<int>(bm.width);
        const int bh    = static_cast<int>(bm.rows);
        const int slotW = bw + kPad * 2;
        const int slotH = bh + kPad * 2;

        // Wrap to next row if needed.
        if (atlas.penAtlasX + slotW > static_cast<int>(kAtlasSize))
        {
            atlas.penAtlasX  = 0;
            atlas.penAtlasY += atlas.rowHeight + kPad;
            atlas.rowHeight  = 0;
        }

        if (atlas.penAtlasY + slotH <= static_cast<int>(kAtlasSize) && bw > 0 && bh > 0)
        {
            const int destX = atlas.penAtlasX + kPad;
            const int destY = atlas.penAtlasY + kPad;

            for (int row = 0; row < bh; ++row)
            {
                for (int col = 0; col < bw; ++col)
                {
                    const auto src = bm.buffer[static_cast<std::size_t>(row) *
                                                   static_cast<std::size_t>(std::abs(bm.pitch)) +
                                               static_cast<std::size_t>(col)];
                    atlas.pixels[static_cast<std::size_t>(destY + row) * kAtlasSize +
                                 static_cast<std::size_t>(destX + col)] = src;
                }
            }

            info.atlasX      = destX;
            info.atlasY      = destY;
            info.bitmapW     = bw;
            info.bitmapH     = bh;
            info.bearingX    = face->glyph->bitmap_left;
            info.bearingY    = face->glyph->bitmap_top;
            info.advanceX    = static_cast<int>(face->glyph->advance.x >> 6);
            info.valid       = true;

            atlas.penAtlasX += slotW;
            atlas.rowHeight  = (std::max)(atlas.rowHeight, slotH);
            ++atlas.generation;
        } else if (gi != 0)
        {
            // Glyph does not fit, store advance only.
            info.advanceX = static_cast<int>(face->glyph->advance.x >> 6);
        }
    } else if (gi != 0 && FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) == 0)
    {
        info.advanceX = static_cast<int>(face->glyph->advance.x >> 6);
    }

    atlas.glyphs[codepoint] = info;
    return atlas.glyphs[codepoint];
}
// ---------------------------------------------------------------------------

} // namespace

using namespace vigine::graphics;

RenderComponent::RenderComponent() {}

RenderComponent::~RenderComponent() {};

void RenderComponent::render() {}

void RenderComponent::setMesh(const MeshComponent &mesh) { _mesh = mesh; }

void RenderComponent::setTransform(const TransformComponent &transform) { _transform = transform; }

void RenderComponent::setShader(const ShaderComponent &shader) { _shader = shader; }

void RenderComponent::setTextureHandle(TextureHandle handle) { _textureHandle = handle; }

void RenderComponent::setPickable(bool pickable) { _pickable = pickable; }

bool RenderComponent::setText(const TextComponent &textComponent)
{
    _text = textComponent;

    // TextVoxel entities still need per-pixel offsets for the instanced voxel draw.
    if (_shader.useVoxelTextLayout())
    {
        const bool voxelOk = rebuildTextVoxelOffsets();
        // SDF quads not used by TextVoxel pipeline, skip.
        return voxelOk;
    }

    // All other profiles (Glyph): clear voxel offsets, build SDF atlas quads.
    _text.setVoxelOffsets({});
    return rebuildSdfGlyphQuads();
}

bool RenderComponent::refreshSdfGlyphQuads() { return rebuildSdfGlyphQuads(); }

void RenderComponent::appendModelMatrices(std::vector<glm::mat4> &modelMatrices) const
{
    const glm::mat4 anchorMatrix = _transform.getModelMatrix();

    if (_text.drawBaseInstance())
        modelMatrices.push_back(anchorMatrix);

    if (!_text.enabled())
        return;

    for (const auto &offset : _text.voxelOffsets())
    {
        glm::mat4 model = anchorMatrix;
        model           = glm::translate(model, offset);
        model           = glm::scale(model, glm::vec3(_text.voxelSize()));
        modelMatrices.push_back(std::move(model));
    }
}

void RenderComponent::appendModelMatrices(std::vector<glm::mat4> &modelMatrices,
                                          const glm::mat4 &anchorOverride) const
{
    const glm::mat4 anchorMatrix = anchorOverride;

    if (_text.drawBaseInstance())
        modelMatrices.push_back(anchorMatrix);

    if (!_text.enabled())
        return;

    for (const auto &offset : _text.voxelOffsets())
    {
        glm::mat4 model = anchorMatrix;
        model           = glm::translate(model, offset);
        model           = glm::scale(model, glm::vec3(_text.voxelSize()));
        modelMatrices.push_back(std::move(model));
    }
}

bool RenderComponent::rebuildTextVoxelOffsets()
{
    if (!_text.enabled())
    {
        _text.setVoxelOffsets({});
        return true;
    }

    if (_text.text().empty() || _text.fontPath().empty() || _text.pixelSize() == 0)
    {
        _text.setVoxelOffsets({});
        return false;
    }

    FT_Face face = cachedFace(_text.fontPath(), _text.pixelSize());
    if (!face)
    {
        _text.setVoxelOffsets({});
        return false;
    }

    std::vector<glm::vec3> offsets;
    float penX             = 0.0f;
    float penY             = 0.0f;
    const float lineHeight = static_cast<float>(_text.pixelSize()) * 1.25f;
    float minX             = std::numeric_limits<float>::max();
    float maxX             = std::numeric_limits<float>::lowest();
    float minY             = std::numeric_limits<float>::max();
    float maxY             = std::numeric_limits<float>::lowest();
    const float z          = 0.0f;

    const auto codepoints  = decodeUtf8(_text.text());
    for (uint32_t codepoint : codepoints)
    {
        if (codepoint == static_cast<uint32_t>('\n'))
        {
            penX  = 0.0f;
            penY -= lineHeight;
            continue;
        }

        FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));

        // Missing glyphs are replaced with '?' so non-Latin codepoints remain visible.
        if (glyphIndex == 0 && codepoint != static_cast<uint32_t>(' '))
            glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>('?'));

        if (glyphIndex == 0)
        {
            penX += static_cast<float>(_text.pixelSize()) * 0.35f;
            continue;
        }

        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0)
            continue;

        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
            continue;

        const float advancePx = static_cast<float>(face->glyph->advance.x >> 6);
        const float maxPx =
            _text.maxLineWorldWidth() > 0.0f ? _text.maxLineWorldWidth() / _text.voxelSize() : 0.0f;
        if (maxPx > 0.0f && penX > 0.0f && penX + advancePx > maxPx)
        {
            penX  = 0.0f;
            penY -= lineHeight;
        }

        const auto &bitmap    = face->glyph->bitmap;
        const float glyphLeft = penX + static_cast<float>(face->glyph->bitmap_left);
        const float glyphTop  = penY + static_cast<float>(face->glyph->bitmap_top);

        for (unsigned int row = 0; row < bitmap.rows; ++row)
        {
            for (unsigned int col = 0; col < bitmap.width; ++col)
            {
                const auto index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(bitmap.pitch) +
                    static_cast<std::size_t>(col);
                if (bitmap.buffer[index] < 120)
                    continue;

                const float x = glyphLeft + static_cast<float>(col);
                const float y = glyphTop - static_cast<float>(row);

                minX          = (std::min)(minX, x);
                maxX          = (std::max)(maxX, x);
                minY          = (std::min)(minY, y);
                maxY          = (std::max)(maxY, y);

                offsets.emplace_back(x, y, z);
            }
        }

        penX += advancePx;
    }

    // face and library are cached — do not call FT_Done_Face / FT_Done_FreeType here.

    if (offsets.empty())
    {
        _text.setVoxelOffsets({});
        return false;
    }

    const float anchorX  = _text.topLeftAnchor() ? minX : (minX + maxX) * 0.5f;
    const float anchorY  = _text.topLeftAnchor() ? maxY : (minY + maxY) * 0.5f;
    const auto &localOff = _text.anchorOffset();

    for (auto &offset : offsets)
    {
        offset.x = (offset.x - anchorX) * _text.voxelSize() + localOff.x;
        offset.y = (offset.y - anchorY) * _text.voxelSize() + localOff.y;
        offset.z = 0.0f;
    }

    _text.setVoxelOffsets(std::move(offsets));
    return true;
}

void RenderComponent::appendGlyphQuadVertices(std::vector<GlyphQuadVertex> &vertices) const
{
    vertices.insert(vertices.end(), _cachedBodyWorldVertices.begin(),
                    _cachedBodyWorldVertices.end());
    if (_text.cursorVisible())
    {
        vertices.insert(vertices.end(), _cachedCursorWorldVertices.begin(),
                        _cachedCursorWorldVertices.end());
    }
}

void RenderComponent::updateCursorVisible(bool visible) { _text.setCursorVisible(visible); }

bool RenderComponent::rebuildSdfGlyphQuads()
{
    _cachedBodyWorldVertices.clear();
    _cachedCursorWorldVertices.clear();
    _cursorSlots.clear();
    _bodyQuadsPx.clear();
    _lineCache.clear();
    _sdfContentHeight = 0.0f;

    if (!_text.enabled())
        return true;

    if (_text.fontPath().empty() || _text.pixelSize() == 0)
        return false;

    const std::string &str = _text.text();
    const bool hasText     = !str.empty();

    FT_Face face           = cachedFace(_text.fontPath(), _text.pixelSize());
    if (!face)
        return false;

    FontAtlas &atlas         = cachedAtlas(_text.fontPath(), _text.pixelSize());

    const float atlasF       = static_cast<float>(kAtlasSize);
    const float ws           = _text.voxelSize();
    const float lineHeightPx = static_cast<float>(_text.pixelSize()) * 1.25f;
    const float maxLinePx =
        _text.maxLineWorldWidth() > 0.0f ? _text.maxLineWorldWidth() / ws : 0.0f;
    const float spaceAdvPx          = static_cast<float>(_text.pixelSize()) * 0.3f;

    const std::size_t cursorBytePos = _text.cursorBytePos();
    float cursorPenX                = 0.0f;
    float cursorPenY                = 0.0f;
    bool cursorPosFound             = false;

    if (hasText)
    {
        float penX = 0.0f;
        float penY = 0.0f;

        LineCacheEntry currentLine;
        currentLine.byteBegin = 0;
        currentLine.penYStart = penY;
        currentLine.localMinX = std::numeric_limits<float>::max();
        currentLine.localMaxX = std::numeric_limits<float>::lowest();
        currentLine.localMinY = std::numeric_limits<float>::max();
        currentLine.localMaxY = std::numeric_limits<float>::lowest();

        auto finishLine       = [&]() {
            currentLine.byteEnd = 0; // will be set by caller
            currentLine.penXEnd = penX;
            _lineCache.push_back(std::move(currentLine));
            currentLine           = {};
            currentLine.localMinX = std::numeric_limits<float>::max();
            currentLine.localMaxX = std::numeric_limits<float>::lowest();
            currentLine.localMinY = std::numeric_limits<float>::max();
            currentLine.localMaxY = std::numeric_limits<float>::lowest();
        };

        std::size_t i = 0;
        while (i < str.size())
        {
            if (!cursorPosFound && i >= cursorBytePos)
            {
                cursorPenX     = penX;
                cursorPenY     = penY;
                cursorPosFound = true;
            }

            const std::size_t byteStart = i;
            const auto b                = static_cast<unsigned char>(str[i]);
            uint32_t cp                 = 0;

            if ((b & 0x80u) == 0u)
            {
                cp  = b;
                i  += 1;
            } else if ((b & 0xE0u) == 0xC0u && i + 1 < str.size())
            {
                cp  = ((b & 0x1Fu) << 6u) | (static_cast<unsigned char>(str[i + 1]) & 0x3Fu);
                i  += 2;
            } else if ((b & 0xF0u) == 0xE0u && i + 2 < str.size())
            {
                cp = ((b & 0x0Fu) << 12u) |
                     ((static_cast<unsigned char>(str[i + 1]) & 0x3Fu) << 6u) |
                     (static_cast<unsigned char>(str[i + 2]) & 0x3Fu);
                i += 3;
            } else if ((b & 0xF8u) == 0xF0u && i + 3 < str.size())
            {
                cp = ((b & 0x07u) << 18u) |
                     ((static_cast<unsigned char>(str[i + 1]) & 0x3Fu) << 12u) |
                     ((static_cast<unsigned char>(str[i + 2]) & 0x3Fu) << 6u) |
                     (static_cast<unsigned char>(str[i + 3]) & 0x3Fu);
                i += 4;
            } else
            {
                ++i;
                continue;
            }

            currentLine.slots.push_back({byteStart, penX, penY});

            if (cp == static_cast<uint32_t>('\n'))
            {
                currentLine.byteEnd = i;
                currentLine.penXEnd = penX;
                _lineCache.push_back(std::move(currentLine));
                penX                   = 0.0f;
                penY                  -= lineHeightPx;
                currentLine            = {};
                currentLine.byteBegin  = i;
                currentLine.penYStart  = penY;
                currentLine.localMinX  = std::numeric_limits<float>::max();
                currentLine.localMaxX  = std::numeric_limits<float>::lowest();
                currentLine.localMinY  = std::numeric_limits<float>::max();
                currentLine.localMaxY  = std::numeric_limits<float>::lowest();
                continue;
            }

            if (cp == static_cast<uint32_t>(' '))
            {
                penX += spaceAdvPx;
                continue;
            }

            const GlyphInfo &gi = ensureGlyph(atlas, face, cp, _text.pixelSize());
            const float advPx   = static_cast<float>(gi.advanceX);

            if (maxLinePx > 0.0f && penX > 0.0f && penX + advPx > maxLinePx)
            {
                // Word wrap: finish current line, start new visual line.
                currentLine.byteEnd = byteStart;
                currentLine.penXEnd = penX;
                _lineCache.push_back(std::move(currentLine));
                penX                   = 0.0f;
                penY                  -= lineHeightPx;
                currentLine            = {};
                currentLine.byteBegin  = byteStart;
                currentLine.penYStart  = penY;
                currentLine.localMinX  = std::numeric_limits<float>::max();
                currentLine.localMaxX  = std::numeric_limits<float>::lowest();
                currentLine.localMinY  = std::numeric_limits<float>::max();
                currentLine.localMaxY  = std::numeric_limits<float>::lowest();
                // Re-add this slot at new penX position on the new line.
                currentLine.slots.push_back({byteStart, penX, penY});
            }

            if (gi.valid)
            {
                const float gx0 = penX + static_cast<float>(gi.bearingX);
                const float gy0 =
                    penY + static_cast<float>(gi.bearingY) - static_cast<float>(gi.bitmapH);
                const float gx1 = gx0 + static_cast<float>(gi.bitmapW);
                const float gy1 = penY + static_cast<float>(gi.bearingY);

                currentLine.quads.push_back({gx0, gy0, gx1, gy1,
                                             static_cast<float>(gi.atlasX) / atlasF,
                                             static_cast<float>(gi.atlasY) / atlasF,
                                             static_cast<float>(gi.atlasX + gi.bitmapW) / atlasF,
                                             static_cast<float>(gi.atlasY + gi.bitmapH) / atlasF});

                currentLine.localMinX = (std::min)(currentLine.localMinX, gx0);
                currentLine.localMaxX = (std::max)(currentLine.localMaxX, gx1);
                currentLine.localMinY = (std::min)(currentLine.localMinY, gy0);
                currentLine.localMaxY = (std::max)(currentLine.localMaxY, gy1);
            }

            penX += advPx;
        }

        // Finish last line.
        currentLine.byteEnd = str.size();
        currentLine.penXEnd = penX;
        _lineCache.push_back(std::move(currentLine));

        // Sentinel cursor slot at end of text.
        if (!_lineCache.empty())
            _lineCache.back().slots.push_back({str.size(), penX, penY});

        if (!cursorPosFound)
        {
            cursorPenX = penX;
            cursorPenY = penY;
        }
    } else
    {
        cursorPenX = 0.0f;
        cursorPenY = 0.0f;
    }

    // Build _bodyQuadsPx, _cursorSlots, cursor quad, vertices — all via shared helper.
    // Full rebuild: all lines are dirty.
    emitWorldVerticesFromLines(0, _lineCache.size(), cursorPenX, cursorPenY);

    return !_cachedBodyWorldVertices.empty() || !_cachedCursorWorldVertices.empty();
}

bool RenderComponent::incrementalRebuildSdf(std::size_t editBytePos)
{
    // Fallback to full rebuild when no line cache exists yet.
    if (_lineCache.empty())
        return rebuildSdfGlyphQuads();

    if (!_text.enabled())
        return rebuildSdfGlyphQuads();

    if (_text.fontPath().empty() || _text.pixelSize() == 0)
        return false;

    const std::string &str = _text.text();
    if (str.empty())
        return rebuildSdfGlyphQuads();

    FT_Face face = cachedFace(_text.fontPath(), _text.pixelSize());
    if (!face)
        return false;

    FontAtlas &atlas = cachedAtlas(_text.fontPath(), _text.pixelSize());

    // Find the first line affected by the edit.
    std::size_t dirtyLineIdx = 0;
    for (std::size_t li = 0; li < _lineCache.size(); ++li)
    {
        if (_lineCache[li].byteBegin <= editBytePos &&
            (editBytePos < _lineCache[li].byteEnd || li + 1 == _lineCache.size()))
        {
            dirtyLineIdx = li;
            break;
        }
        if (_lineCache[li].byteBegin > editBytePos)
        {
            dirtyLineIdx = (li > 0) ? li - 1 : 0;
            break;
        }
    }

    // Rebuild layout from dirtyLineIdx onwards.
    // penY comes from the dirty line's cached penYStart.
    const float atlasF       = static_cast<float>(kAtlasSize);
    const float ws           = _text.voxelSize();
    const float lineHeightPx = static_cast<float>(_text.pixelSize()) * 1.25f;
    const float maxLinePx =
        _text.maxLineWorldWidth() > 0.0f ? _text.maxLineWorldWidth() / ws : 0.0f;
    const float spaceAdvPx = static_cast<float>(_text.pixelSize()) * 0.3f;

    float penX             = 0.0f;
    float penY             = _lineCache[dirtyLineIdx].penYStart;

    // Save tail lines before erase — needed to restore cachedWorldVerts at convergence.
    std::vector<LineCacheEntry> savedLines(
        std::make_move_iterator(_lineCache.begin() + static_cast<std::ptrdiff_t>(dirtyLineIdx)),
        std::make_move_iterator(_lineCache.end()));

    // Trim _lineCache to keep only lines before the dirty one.
    _lineCache.erase(_lineCache.begin() + static_cast<std::ptrdiff_t>(dirtyLineIdx),
                     _lineCache.end());

    const std::size_t layoutStart   = _lineCache.empty() ? 0 : _lineCache.back().byteEnd;

    const std::size_t cursorBytePos = _text.cursorBytePos();
    float cursorPenX                = 0.0f;
    float cursorPenY                = 0.0f;
    bool cursorPosFound             = false;

    // Check if cursor is in a retained line.
    for (const auto &line : _lineCache)
    {
        for (const auto &slot : line.slots)
        {
            if (slot.byteOffset == cursorBytePos)
            {
                cursorPenX     = slot.worldX; // still pixel-space in line cache
                cursorPenY     = slot.worldY;
                cursorPosFound = true;
            }
        }
    }

    // Layout from layoutStart to end of string.
    LineCacheEntry currentLine;
    currentLine.byteBegin = layoutStart;
    currentLine.penYStart = penY;
    currentLine.localMinX = std::numeric_limits<float>::max();
    currentLine.localMaxX = std::numeric_limits<float>::lowest();
    currentLine.localMinY = std::numeric_limits<float>::max();
    currentLine.localMaxY = std::numeric_limits<float>::lowest();

    std::size_t i         = layoutStart;
    while (i < str.size())
    {
        if (!cursorPosFound && i >= cursorBytePos)
        {
            cursorPenX     = penX;
            cursorPenY     = penY;
            cursorPosFound = true;
        }

        const std::size_t byteStart = i;
        const auto b                = static_cast<unsigned char>(str[i]);
        uint32_t cp                 = 0;

        if ((b & 0x80u) == 0u)
        {
            cp  = b;
            i  += 1;
        } else if ((b & 0xE0u) == 0xC0u && i + 1 < str.size())
        {
            cp  = ((b & 0x1Fu) << 6u) | (static_cast<unsigned char>(str[i + 1]) & 0x3Fu);
            i  += 2;
        } else if ((b & 0xF0u) == 0xE0u && i + 2 < str.size())
        {
            cp = ((b & 0x0Fu) << 12u) | ((static_cast<unsigned char>(str[i + 1]) & 0x3Fu) << 6u) |
                 (static_cast<unsigned char>(str[i + 2]) & 0x3Fu);
            i += 3;
        } else if ((b & 0xF8u) == 0xF0u && i + 3 < str.size())
        {
            cp = ((b & 0x07u) << 18u) | ((static_cast<unsigned char>(str[i + 1]) & 0x3Fu) << 12u) |
                 ((static_cast<unsigned char>(str[i + 2]) & 0x3Fu) << 6u) |
                 (static_cast<unsigned char>(str[i + 3]) & 0x3Fu);
            i += 4;
        } else
        {
            ++i;
            continue;
        }

        currentLine.slots.push_back({byteStart, penX, penY});

        if (cp == static_cast<uint32_t>('\n'))
        {
            currentLine.byteEnd = i;
            currentLine.penXEnd = penX;
            _lineCache.push_back(std::move(currentLine));
            penX                   = 0.0f;
            penY                  -= lineHeightPx;
            currentLine            = {};
            currentLine.byteBegin  = i;
            currentLine.penYStart  = penY;
            currentLine.localMinX  = std::numeric_limits<float>::max();
            currentLine.localMaxX  = std::numeric_limits<float>::lowest();
            currentLine.localMinY  = std::numeric_limits<float>::max();
            currentLine.localMaxY  = std::numeric_limits<float>::lowest();
            continue;
        }

        if (cp == static_cast<uint32_t>(' '))
        {
            penX += spaceAdvPx;
            continue;
        }

        const GlyphInfo &gi = ensureGlyph(atlas, face, cp, _text.pixelSize());
        const float advPx   = static_cast<float>(gi.advanceX);

        if (maxLinePx > 0.0f && penX > 0.0f && penX + advPx > maxLinePx)
        {
            // Word wrap: the slot just added belongs to the new line.
            currentLine.slots.pop_back();
            currentLine.byteEnd = byteStart;
            currentLine.penXEnd = penX;
            _lineCache.push_back(std::move(currentLine));
            penX                   = 0.0f;
            penY                  -= lineHeightPx;
            currentLine            = {};
            currentLine.byteBegin  = byteStart;
            currentLine.penYStart  = penY;
            currentLine.localMinX  = std::numeric_limits<float>::max();
            currentLine.localMaxX  = std::numeric_limits<float>::lowest();
            currentLine.localMinY  = std::numeric_limits<float>::max();
            currentLine.localMaxY  = std::numeric_limits<float>::lowest();
            currentLine.slots.push_back({byteStart, penX, penY});
        }

        if (gi.valid)
        {
            const float gx0 = penX + static_cast<float>(gi.bearingX);
            const float gy0 =
                penY + static_cast<float>(gi.bearingY) - static_cast<float>(gi.bitmapH);
            const float gx1 = gx0 + static_cast<float>(gi.bitmapW);
            const float gy1 = penY + static_cast<float>(gi.bearingY);

            currentLine.quads.push_back({gx0, gy0, gx1, gy1, static_cast<float>(gi.atlasX) / atlasF,
                                         static_cast<float>(gi.atlasY) / atlasF,
                                         static_cast<float>(gi.atlasX + gi.bitmapW) / atlasF,
                                         static_cast<float>(gi.atlasY + gi.bitmapH) / atlasF});

            currentLine.localMinX = (std::min)(currentLine.localMinX, gx0);
            currentLine.localMaxX = (std::max)(currentLine.localMaxX, gx1);
            currentLine.localMinY = (std::min)(currentLine.localMinY, gy0);
            currentLine.localMaxY = (std::max)(currentLine.localMaxY, gy1);
        }

        penX += advPx;
    }

    // Finish last line.
    currentLine.byteEnd = str.size();
    currentLine.penXEnd = penX;
    _lineCache.push_back(std::move(currentLine));

    // Sentinel cursor slot.
    if (!_lineCache.empty())
        _lineCache.back().slots.push_back({str.size(), penX, penY});

    if (!cursorPosFound)
    {
        cursorPenX = penX;
        cursorPenY = penY;
    }

    // Find convergence: first new line whose byteBegin+penYStart match a saved line.
    // From that point, cachedWorldVerts from the saved entries are still valid.
    std::size_t dirtyEnd = _lineCache.size();
    {
        std::unordered_map<std::size_t, std::size_t> savedByByte;
        savedByByte.reserve(savedLines.size());
        for (std::size_t k = 0; k < savedLines.size(); ++k)
            savedByByte[savedLines[k].byteBegin] = k;

        for (std::size_t newLi = dirtyLineIdx + 1; newLi < _lineCache.size(); ++newLi)
        {
            auto it = savedByByte.find(_lineCache[newLi].byteBegin);
            if (it == savedByByte.end())
                continue;
            if (savedLines[it->second].penYStart != _lineCache[newLi].penYStart)
                continue;
            // Convergence found — restore cachedWorldVerts for all matching tail lines.
            dirtyEnd = newLi;
            for (std::size_t li2 = newLi; li2 < _lineCache.size(); ++li2)
            {
                auto it2 = savedByByte.find(_lineCache[li2].byteBegin);
                if (it2 != savedByByte.end() &&
                    savedLines[it2->second].penYStart == _lineCache[li2].penYStart)
                {
                    _lineCache[li2].cachedWorldVerts =
                        std::move(savedLines[it2->second].cachedWorldVerts);
                }
            }
            break;
        }
    }

    emitWorldVerticesFromLines(dirtyLineIdx, dirtyEnd, cursorPenX, cursorPenY);
    return true;
}

void RenderComponent::emitWorldVerticesFromLines(std::size_t dirtyFrom, std::size_t dirtyEnd,
                                                 float cursorPenX, float cursorPenY)
{
    _cachedBodyWorldVertices.clear();
    _cachedCursorWorldVertices.clear();
    _cursorSlots.clear();
    _bodyQuadsPx.clear();

    // Global bounding box (always recomputed — cheap, O(lines)).
    float minX  = std::numeric_limits<float>::max();
    float maxX  = std::numeric_limits<float>::lowest();
    float minY  = std::numeric_limits<float>::max();
    float maxYv = std::numeric_limits<float>::lowest();
    for (const auto &line : _lineCache)
    {
        if (line.localMinX < line.localMaxX)
        {
            minX  = (std::min)(minX, line.localMinX);
            maxX  = (std::max)(maxX, line.localMaxX);
            minY  = (std::min)(minY, line.localMinY);
            maxYv = (std::max)(maxYv, line.localMaxY);
        }
    }

    // Cursor quad (pixel-space).
    FontAtlas &atlas   = cachedAtlas(_text.fontPath(), _text.pixelSize());
    FT_Face face       = cachedFace(_text.fontPath(), _text.pixelSize());
    const float atlasF = static_cast<float>(kAtlasSize);
    const float ws     = _text.voxelSize();
    SdfQuadPx cursorQuadPx{};
    bool hasCursorQuad = false;
    {
        const GlyphInfo &cgi = ensureGlyph(atlas, face, '_', _text.pixelSize());
        if (cgi.valid)
        {
            cursorQuadPx.x0 = cursorPenX + static_cast<float>(cgi.bearingX);
            cursorQuadPx.y0 =
                cursorPenY + static_cast<float>(cgi.bearingY) - static_cast<float>(cgi.bitmapH);
            cursorQuadPx.x1 = cursorQuadPx.x0 + static_cast<float>(cgi.bitmapW);
            cursorQuadPx.y1 = cursorPenY + static_cast<float>(cgi.bearingY);
            cursorQuadPx.u0 = static_cast<float>(cgi.atlasX) / atlasF;
            cursorQuadPx.v0 = static_cast<float>(cgi.atlasY) / atlasF;
            cursorQuadPx.u1 = static_cast<float>(cgi.atlasX + cgi.bitmapW) / atlasF;
            cursorQuadPx.v1 = static_cast<float>(cgi.atlasY + cgi.bitmapH) / atlasF;
            hasCursorQuad   = true;

            minX            = (std::min)(minX, cursorQuadPx.x0);
            maxX            = (std::max)(maxX, cursorQuadPx.x1);
            minY            = (std::min)(minY, cursorQuadPx.y0);
            maxYv           = (std::max)(maxYv, cursorQuadPx.y1);
        }
    }

    // Content height.
    {
        const float lineHeightPx = static_cast<float>(_text.pixelSize()) * 1.25f;
        float lowestPenY         = cursorPenY;
        if (!_lineCache.empty())
            lowestPenY = (std::min)(lowestPenY, _lineCache.back().penYStart);
        _sdfContentHeight = (lineHeightPx - lowestPenY) * ws;
    }

    if (_lineCache.empty() && !hasCursorQuad)
        return;

    // Anchor.
    const float anchorX  = _text.topLeftAnchor() ? minX : (minX + maxX) * 0.5f;
    const float anchorY  = _text.topLeftAnchor() ? maxYv : (minY + maxYv) * 0.5f;
    const auto &localOff = _text.anchorOffset();

    const glm::mat4 M    = _transform.getModelMatrix();
    const glm::vec3 mc0(M[0][0], M[0][1], M[0][2]);
    const glm::vec3 mc1(M[1][0], M[1][1], M[1][2]);
    const glm::vec3 mc3(M[3][0], M[3][1], M[3][2]);

    // If anchor changed, all lines are dirty.
    const bool anchorChanged        = (anchorX != _cachedAnchorX || anchorY != _cachedAnchorY);
    const std::size_t transformFrom = anchorChanged ? 0 : dirtyFrom;
    const std::size_t transformEnd  = anchorChanged ? _lineCache.size() : dirtyEnd;
    _cachedAnchorX                  = anchorX;
    _cachedAnchorY                  = anchorY;

    // Helper: transform a SdfQuadPx to 6 world-space verts and append.
    auto emitQuad = [&](std::vector<GlyphQuadVertex> &out, const SdfQuadPx &q) {
        const float wx0    = (q.x0 - anchorX) * ws + localOff.x;
        const float wx1    = (q.x1 - anchorX) * ws + localOff.x;
        const float wy0    = (q.y0 - anchorY) * ws + localOff.y;
        const float wy1    = (q.y1 - anchorY) * ws + localOff.y;

        const glm::vec3 tl = mc0 * wx0 + mc1 * wy1 + mc3;
        const glm::vec3 tr = mc0 * wx1 + mc1 * wy1 + mc3;
        const glm::vec3 br = mc0 * wx1 + mc1 * wy0 + mc3;
        const glm::vec3 bl = mc0 * wx0 + mc1 * wy0 + mc3;

        out.push_back({
            tl, {q.u0, q.v0}
        });
        out.push_back({
            tr, {q.u1, q.v0}
        });
        out.push_back({
            br, {q.u1, q.v1}
        });
        out.push_back({
            tl, {q.u0, q.v0}
        });
        out.push_back({
            br, {q.u1, q.v1}
        });
        out.push_back({
            bl, {q.u0, q.v1}
        });
    };

    // Process each line.
    std::size_t totalVerts = 0;
    for (const auto &line : _lineCache)
        totalVerts += line.quads.size() * 6;
    _cachedBodyWorldVertices.reserve(totalVerts);

    for (std::size_t li = 0; li < _lineCache.size(); ++li)
    {
        auto &line = _lineCache[li];

        // Cursor slots (pixel→world, always recomputed for lines at or after cursor line).
        for (const auto &slot : line.slots)
        {
            float wx           = (slot.worldX - anchorX) * ws + localOff.x;
            float wy           = (slot.worldY - anchorY) * ws + localOff.y;
            const glm::vec3 wp = mc0 * wx + mc1 * wy + mc3;
            _cursorSlots.push_back({slot.byteOffset, wp.x, wp.y});
        }

        if (li < transformFrom || li >= transformEnd)
        {
            // Pre-edit or post-convergence: reuse cached world verts.
            _cachedBodyWorldVertices.insert(_cachedBodyWorldVertices.end(),
                                            line.cachedWorldVerts.begin(),
                                            line.cachedWorldVerts.end());
            continue;
        }

        // Recompute world verts for this line and update the cache.
        line.cachedWorldVerts.clear();
        line.cachedWorldVerts.reserve(line.quads.size() * 6);
        for (const auto &q : line.quads)
        {
            emitQuad(line.cachedWorldVerts, q);
        }
        _cachedBodyWorldVertices.insert(_cachedBodyWorldVertices.end(),
                                        line.cachedWorldVerts.begin(), line.cachedWorldVerts.end());
    }

    // Cursor quad.
    if (hasCursorQuad)
    {
        emitQuad(_cachedCursorWorldVertices, cursorQuadPx);
    }
}

void RenderComponent::scrollVertical(float deltaAnchorOffsetY)
{
    if (deltaAnchorOffsetY == 0.0f)
        return;

    // Compute world-space shift: only anchorOffset.y changes, model matrix is identity for SDF
    // text, so delta world pos = mc1 * deltaAnchorOffsetY.
    const glm::mat4 M = _transform.getModelMatrix();
    const glm::vec3 mc1(M[1][0], M[1][1], M[1][2]);
    const glm::vec3 dv = mc1 * deltaAnchorOffsetY;

    for (auto &v : _cachedBodyWorldVertices)
        v.pos += dv;
    for (auto &v : _cachedCursorWorldVertices)
        v.pos += dv;
    for (auto &line : _lineCache)
        for (auto &v : line.cachedWorldVerts)
            v.pos += dv;
    for (auto &slot : _cursorSlots)
    {
        slot.worldX += dv.x;
        slot.worldY += dv.y;
    }

    // Update anchorOffset so subsequent rebuilds use the new offset.
    const glm::vec2 off = _text.anchorOffset();
    _text.setAnchorOffset({off.x, off.y + deltaAnchorOffsetY});

    // Invalidate pixel-space anchor cache: next emitWorldVerticesFromLines will
    // force full re-transform from pixel quads (consistent with new anchorOffset).
    _cachedAnchorY = 1e30f;
}

void RenderComponent::translateGlyphVertices(const glm::vec3 &delta)
{
    if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        return;

    // Translate all cached world vertices by delta (for entity movement).
    for (auto &v : _cachedBodyWorldVertices)
        v.pos += delta;
    for (auto &v : _cachedCursorWorldVertices)
        v.pos += delta;
    for (auto &line : _lineCache)
        for (auto &v : line.cachedWorldVerts)
            v.pos += delta;
    for (auto &slot : _cursorSlots)
    {
        slot.worldX += delta.x;
        slot.worldY += delta.y;
    }

    // Unlike scrollVertical, anchorOffset stays unchanged (entity moved, not scrolled).
}

// Returns the SDF atlas pixel data for the given font/size (for GPU upload).
const std::vector<uint8_t> *RenderComponent::getSdfAtlasPixels() const
{
    if (_text.fontPath().empty() || _text.pixelSize() == 0)
        return nullptr;
    FontAtlas &atlas = cachedAtlas(_text.fontPath(), _text.pixelSize());
    return &atlas.pixels;
}

uint32_t RenderComponent::getSdfAtlasGeneration() const
{
    if (_text.fontPath().empty() || _text.pixelSize() == 0)
        return 0;
    FontAtlas &atlas = cachedAtlas(_text.fontPath(), _text.pixelSize());
    return atlas.generation;
}