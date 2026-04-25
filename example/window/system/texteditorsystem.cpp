#include "texteditorsystem.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/rendersystem.h>
#include <vigine/service/graphicsservice.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kEditorTextPlaneZ        = 1.21f;
constexpr float kScrollbarPlaneZ         = 1.205f;
constexpr float kScrollbarInsetY         = 0.06f;
constexpr float kScrollbarInsetX         = 0.04f;
constexpr float kScrollbarWidth          = 0.06f;
constexpr float kScrollbarMinThumbHeight = 0.20f; // comfortable draggable size
constexpr float kFocusFrameZ             = 1.209f;
constexpr float kFocusFrameThickness     = 0.022f;
constexpr float kFocusFramePadding       = 0.012f;
} // namespace

TextEditorSystem::TextEditorSystem(std::shared_ptr<TextEditState> state) : _state(std::move(state))
{
}

void TextEditorSystem::bind(vigine::Context *context,
                            vigine::graphics::GraphicsService *graphicsService,
                            vigine::graphics::RenderSystem *renderSystem)
{
    _context         = context;
    _graphicsService = graphicsService;
    _renderSystem    = renderSystem;
}

void TextEditorSystem::setLayout(std::size_t /*maxColumns*/, float panelWidth, float panelHeight)
{
    _panelWidth  = panelWidth;
    _panelHeight = panelHeight;
}

void TextEditorSystem::offsetEditorFrame(float dx, float dy, float dz)
{
    _panelCenterX += dx;
    _panelTopY    += dy;
    _panelZBase   += dz;
}

void TextEditorSystem::refreshEditorLayout()
{
    updateScrollbarVisuals();
    updateFocusFrameVisual(_focused);

    // Update clip region when panel position changes.
    if (_renderSystem)
    {
        const float panelBottom = _panelTopY - _panelHeight;
        _renderSystem->setSdfClipY(panelBottom, _panelTopY);
    }
}

void TextEditorSystem::onFrame()
{
    if (!_state)
        return;

    // Cursor blink: toggle visibility every 500ms.
    {
        using namespace std::chrono;
        if (duration_cast<milliseconds>(steady_clock::now() - _lastBlink).count() >= 500)
        {
            _lastBlink         = steady_clock::now();
            _state->showCursor = !_state->showCursor;
            _state->dirty      = true;
        }
    }

    if (!_state->dirty || !_graphicsService || !_context)
        return;

    auto *em = _context->entityManager();
    if (!em)
        return;

    auto *textEditEntity = em->getEntityByAlias("TextEditEntity");
    if (!textEditEntity)
        return;

    float contentHeight = 0.0f;

    _graphicsService->bindEntity(textEditEntity);
    if (auto *rc = _graphicsService->renderComponent())
    {
        if (_state->textChanged)
        {
            // Modify the TextComponent in-place (avoids triple string copy).
            auto &text = rc->getText();
            text.setText(_state->text);
            text.setCursorBytePos(_state->cursorPos);
            text.setCursorVisible(_state->showCursor);

            if (_state->textContentChanged)
            {
                // Text content changed — incremental rebuild from edit position.
                static_cast<void>(rc->incrementalRebuildSdf(_state->editBytePos));
                _state->textContentChanged = false;
            } else
            {
                // Only cursor moved — re-emit with cached line layout.
                static_cast<void>(rc->refreshSdfGlyphQuads());
            }

            _cursorSlots        = rc->takeCursorSlots();
            contentHeight       = rc->sdfContentHeight();
            _state->textChanged = false;

            // Mark glyph dirty only when vertex data actually changed.
            if (_renderSystem)
                _renderSystem->markGlyphDirty();
        } else
        {
            // Only cursor visibility toggled (blink) — skip expensive rebuild.
            rc->updateCursorVisible(_state->showCursor);
        }
    }
    _graphicsService->unbindEntity();

    // Keep editor board height fixed. Only update text scroll/clipping for large content.
    if (contentHeight > 0.0f)
        _contentHeight = contentHeight;

    // Clamp scroll so text can't be scrolled past available content.
    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    if (_scrollOffsetY > maxScroll)
        applyScrollOffset(maxScroll);

    // Clip text to fixed panel bounds in shader (only if panel moved or content changed).
    // No need to update clip on every cursor blink.
    if (_state->textChanged)
    {
        const float panelBottom = _panelTopY - _panelHeight;
        if (_renderSystem)
            _renderSystem->setSdfClipY(panelBottom, _panelTopY);
    }

    updateScrollbarVisuals();

    _state->dirty = false;
}

void TextEditorSystem::onMouseWheel(int delta)
{
    if (!_graphicsService || !_context || delta == 0)
        return;

    // 3 visible lines per scroll notch (Windows WHEEL_DELTA = 120).
    constexpr float kPixelSize  = 28.0f;
    constexpr float kVoxelSize  = 0.0022f;
    const float lineHeightWorld = kPixelSize * 1.25f * kVoxelSize; // ~0.077 world units
    const float step      = lineHeightWorld * 3.0f * (static_cast<float>(std::abs(delta)) / 120.0f);

    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    const float prevScroll = _scrollOffsetY;

    if (delta < 0) // scroll down: show content below → text moves up
        applyScrollOffset((std::min)(_scrollOffsetY + step, maxScroll));
    else // scroll up: back toward top
        applyScrollOffset((std::max)(_scrollOffsetY - step, 0.0f));

    if (std::abs(_scrollOffsetY - prevScroll) > 1e-6f)
        updateScrollbarVisuals();
}

void TextEditorSystem::setFocused(bool focused)
{
    if (_focused == focused)
        return;

    _focused = focused;
    updateFocusFrameVisual(_focused);
}

bool TextEditorSystem::onMouseButtonDown(int x, int y, const vigine::Entity *pickedEntity)
{
    if (!_context || !_renderSystem || !isScrollbarEntity(pickedEntity))
        return false;

    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    if (maxScroll <= 0.0f)
        return true;

    float hitY = 0.0f;
    if (!screenYToWorldY(x, y, hitY))
        return true;

    _scrollbarDragging    = true;
    const float halfThumb = _scrollbarThumbHeight * 0.5f;
    if (hitY >= _scrollbarThumbCenterY - halfThumb && hitY <= _scrollbarThumbCenterY + halfThumb)
    {
        // Preserve grab offset inside thumb for natural drag.
        _scrollbarDragYOffset = hitY - _scrollbarThumbCenterY;
    } else
    {
        // Click on track: center thumb at click and start dragging from center.
        _scrollbarDragYOffset   = 0.0f;
        const float trackTop    = _panelTopY - kScrollbarInsetY;
        const float trackBottom = _panelTopY - _panelHeight + kScrollbarInsetY;
        const float minCenterY  = trackBottom + halfThumb;
        const float maxCenterY  = trackTop - halfThumb;
        const float newCenterY  = (std::max)(minCenterY, (std::min)(hitY, maxCenterY));
        const float travel      = (std::max)(0.0f, maxCenterY - minCenterY);
        const float norm        = (travel > 0.0f) ? (maxCenterY - newCenterY) / travel : 0.0f;
        applyScrollOffset(norm * maxScroll);
        updateScrollbarVisuals();
    }

    return true;
}

void TextEditorSystem::onMouseButtonUp()
{
    _scrollbarDragging    = false;
    _scrollbarDragYOffset = 0.0f;
}

void TextEditorSystem::onMouseMove(int x, int y)
{
    if (!_scrollbarDragging)
        return;

    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    if (maxScroll <= 0.0f)
        return;

    float hitY = 0.0f;
    if (!screenYToWorldY(x, y, hitY))
        return;

    const float trackTop       = _panelTopY - kScrollbarInsetY;
    const float trackBottom    = _panelTopY - _panelHeight + kScrollbarInsetY;
    const float halfThumb      = _scrollbarThumbHeight * 0.5f;
    const float minCenterY     = trackBottom + halfThumb;
    const float maxCenterY     = trackTop - halfThumb;
    const float draggedCenterY = hitY - _scrollbarDragYOffset;
    const float clampedCenterY = (std::max)(minCenterY, (std::min)(draggedCenterY, maxCenterY));

    const float travel         = (std::max)(0.0f, maxCenterY - minCenterY);
    const float norm           = (travel > 0.0f) ? (maxCenterY - clampedCenterY) / travel : 0.0f;

    applyScrollOffset(norm * maxScroll);
    updateScrollbarVisuals();
}

void TextEditorSystem::onEditorClick(int x, int y)
{
    if (!_state)
        return;

    if (_state->text.empty())
    {
        _state->cursorPos   = 0;
        _state->dirty       = true;
        _state->textChanged = true;
        return;
    }

    _state->cursorPos   = cursorFromHitPoint(x, y);
    _state->dirty       = true;
    _state->textChanged = true;
}

void TextEditorSystem::onKeyDown(unsigned int keyCode)
{
    if (!_state)
        return;

    constexpr unsigned int kKeyBackspace = 0x08;
    constexpr unsigned int kKeyDelete    = 0x2E;
    constexpr unsigned int kKeyLeft      = 0x25;
    constexpr unsigned int kKeyRight     = 0x27;
    constexpr unsigned int kKeyHome      = 0x24;
    constexpr unsigned int kKeyEnd       = 0x23;
    constexpr unsigned int kKeyEnter     = 0x0D;

    switch (keyCode)
    {
    case kKeyBackspace:
        _state->backspace();
        break;
    case kKeyDelete:
        _state->deleteChar();
        break;
    case kKeyLeft:
        _state->moveCursorLeft();
        break;
    case kKeyRight:
        _state->moveCursorRight();
        break;
    case kKeyHome:
        _state->cursorPos   = 0;
        _state->dirty       = true;
        _state->textChanged = true;
        break;
    case kKeyEnd:
        _state->cursorPos   = _state->text.size();
        _state->dirty       = true;
        _state->textChanged = true;
        break;
    case kKeyEnter:
        _state->insertCodepoint('\n');
        break;
    default:
        break;
    }
}

void TextEditorSystem::onChar(const vigine::platform::TextEvent &event, uint8_t movementKeyMask)
{
    if (!_state)
        return;

    const bool isCtrlAltCombo = (event.modifiers & vigine::platform::KeyModifierControl) != 0 ||
                                (event.modifiers & vigine::platform::KeyModifierAlt) != 0;
    if (isCtrlAltCombo || movementKeyMask != 0)
        return;

    _state->insertCodepoint(event.codePoint);
}

bool TextEditorSystem::isEditorEntity(const vigine::Entity *entity) const
{
    if (!entity || !_context)
        return false;

    auto *em = _context->entityManager();
    if (!em)
        return false;

    return entity == em->getEntityByAlias("TextEditEntity") ||
           entity == em->getEntityByAlias("TextEditBgEntity") ||
           entity == em->getEntityByAlias("TextEditFocusTopEntity") ||
           entity == em->getEntityByAlias("TextEditFocusBottomEntity") ||
           entity == em->getEntityByAlias("TextEditFocusLeftEntity") ||
           entity == em->getEntityByAlias("TextEditFocusRightEntity") || isScrollbarEntity(entity);
}

void TextEditorSystem::insertUtf8(const std::string &utf8)
{
    if (!_state)
        return;

    _state->insertUtf8(utf8);
}

void TextEditorSystem::clearText()
{
    if (!_state)
        return;

    _state->text.clear();
    _state->cursorPos   = 0;
    _state->dirty       = true;
    _state->textChanged = true;
}

const std::string &TextEditorSystem::text() const
{
    static const std::string kEmpty;
    if (!_state)
        return kEmpty;
    return _state->text;
}

void TextEditorSystem::applyScrollOffset(float newOffset)
{
    if (!_graphicsService || !_context)
        return;

    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    const float clamped   = (std::max)(0.0f, (std::min)(newOffset, maxScroll));
    const float delta     = clamped - _scrollOffsetY;
    if (std::abs(delta) < 1e-6f)
        return;

    auto *em = _context->entityManager();
    if (!em)
        return;

    auto *textEntity = em->getEntityByAlias("TextEditEntity");
    if (!textEntity)
        return;

    _scrollOffsetY = clamped;

    _graphicsService->bindEntity(textEntity);
    if (auto *rc = _graphicsService->renderComponent())
    {
        rc->scrollVertical(delta);
        if (_renderSystem)
            _renderSystem->markGlyphDirty();
    }
    _graphicsService->unbindEntity();
}

void TextEditorSystem::updateScrollbarVisuals()
{
    if (!_graphicsService || !_context)
        return;

    auto *em = _context->entityManager();
    if (!em)
        return;

    auto *trackEntity = em->getEntityByAlias("TextEditScrollbarTrackEntity");
    auto *thumbEntity = em->getEntityByAlias("TextEditScrollbarThumbEntity");
    if (!trackEntity || !thumbEntity)
        return;

    const float panelRight   = _panelCenterX + _panelWidth * 0.5f;
    const float trackHeight  = (std::max)(0.05f, _panelHeight - 2.0f * kScrollbarInsetY);
    const float trackCenterY = _panelTopY - _panelHeight * 0.5f;
    const float trackCenterX = panelRight - kScrollbarInsetX - kScrollbarWidth * 0.5f;

    _graphicsService->bindEntity(trackEntity);
    if (auto *trackRc = _graphicsService->renderComponent())
    {
        auto t = trackRc->getTransform();
        t.setPosition({trackCenterX, trackCenterY, kScrollbarPlaneZ + _panelZBase});
        t.setScale({kScrollbarWidth, trackHeight, 0.01f});
        trackRc->setTransform(t);
    }
    _graphicsService->unbindEntity();

    const float fullContent  = (std::max)(_contentHeight, _panelHeight);
    const float visibleRatio = _panelHeight / fullContent;
    _scrollbarThumbHeight =
        (std::max)(kScrollbarMinThumbHeight, (std::min)(trackHeight * visibleRatio, trackHeight));

    const float trackTop    = _panelTopY - kScrollbarInsetY;
    const float trackBottom = _panelTopY - _panelHeight + kScrollbarInsetY;
    const float minCenterY  = trackBottom + _scrollbarThumbHeight * 0.5f;
    const float maxCenterY  = trackTop - _scrollbarThumbHeight * 0.5f;

    const float maxScroll   = (std::max)(0.0f, _contentHeight - _panelHeight);
    const float norm        = (maxScroll > 0.0f) ? (_scrollOffsetY / maxScroll) : 0.0f;
    _scrollbarThumbCenterY  = maxCenterY - norm * (std::max)(0.0f, maxCenterY - minCenterY);

    _graphicsService->bindEntity(thumbEntity);
    if (auto *thumbRc = _graphicsService->renderComponent())
    {
        auto t = thumbRc->getTransform();
        t.setPosition(
            {trackCenterX, _scrollbarThumbCenterY, kScrollbarPlaneZ + _panelZBase + 0.001f});
        t.setScale({kScrollbarWidth * 0.82f, _scrollbarThumbHeight, 0.012f});
        thumbRc->setTransform(t);
    }
    _graphicsService->unbindEntity();
}

bool TextEditorSystem::screenYToWorldY(int x, int y, float &worldY) const
{
    if (!_renderSystem)
        return false;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return false;

    if (std::abs(rayDirection.z) < 1e-6f)
        return false;

    const float scrollbarZ = kScrollbarPlaneZ + _panelZBase;
    const float t          = (scrollbarZ - rayOrigin.z) / rayDirection.z;
    if (t <= 0.0f)
        return false;

    worldY = (rayOrigin + rayDirection * t).y;
    return true;
}

bool TextEditorSystem::isScrollbarEntity(const vigine::Entity *entity) const
{
    if (!entity || !_context)
        return false;

    auto *em = _context->entityManager();
    if (!em)
        return false;

    return entity == em->getEntityByAlias("TextEditScrollbarTrackEntity") ||
           entity == em->getEntityByAlias("TextEditScrollbarThumbEntity");
}

void TextEditorSystem::updateFocusFrameVisual(bool visible)
{
    if (!_graphicsService || !_context)
        return;

    auto *em = _context->entityManager();
    if (!em)
        return;

    auto *top    = em->getEntityByAlias("TextEditFocusTopEntity");
    auto *bottom = em->getEntityByAlias("TextEditFocusBottomEntity");
    auto *left   = em->getEntityByAlias("TextEditFocusLeftEntity");
    auto *right  = em->getEntityByAlias("TextEditFocusRightEntity");
    if (!top || !bottom || !left || !right)
        return;

    const float panelTop     = _panelTopY;
    const float panelBottom  = panelTop - _panelHeight;
    const float panelCenterY = (panelTop + panelBottom) * 0.5f;
    const float panelLeft    = _panelCenterX - _panelWidth * 0.5f;
    const float panelRight   = _panelCenterX + _panelWidth * 0.5f;

    const float frameTop     = panelTop + kFocusFramePadding;
    const float frameBottom  = panelBottom - kFocusFramePadding;
    const float frameLeft    = panelLeft - kFocusFramePadding;
    const float frameRight   = panelRight + kFocusFramePadding;

    const float hWidth       = frameRight - frameLeft;
    const float vHeight      = frameTop - frameBottom;

    auto place = [&](vigine::Entity *e, const glm::vec3 &pos, const glm::vec3 &scale) {
        _graphicsService->bindEntity(e);
        if (auto *rc = _graphicsService->renderComponent())
        {
            auto t = rc->getTransform();
            if (visible)
            {
                t.setPosition(pos);
                t.setScale(scale);
            } else
            {
                t.setPosition({0.0f, -100.0f, 0.0f});
                t.setScale({0.001f, 0.001f, 0.001f});
            }
            rc->setTransform(t);
        }
        _graphicsService->unbindEntity();
    };

    // Encode frame side id in scale.z for shader-side perimeter animation mapping.
    place(top, {_panelCenterX, frameTop, kFocusFrameZ + _panelZBase},
          {hWidth, kFocusFrameThickness, 0.021f});
    place(right, {frameRight, panelCenterY, kFocusFrameZ + _panelZBase},
          {kFocusFrameThickness, vHeight, 0.022f});
    place(bottom, {_panelCenterX, frameBottom, kFocusFrameZ + _panelZBase},
          {hWidth, kFocusFrameThickness, 0.023f});
    place(left, {frameLeft, panelCenterY, kFocusFrameZ + _panelZBase},
          {kFocusFrameThickness, vHeight, 0.024f});
}

std::size_t TextEditorSystem::cursorFromHitPoint(int x, int y) const
{
    if (!_renderSystem)
        return 0;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return 0;

    if (std::abs(rayDirection.z) < 1e-6f)
        return 0;

    const float t = (kEditorTextPlaneZ - rayOrigin.z) / rayDirection.z;
    if (t <= 0.0f)
        return 0;

    const glm::vec3 hit = rayOrigin + rayDirection * t;

    if (_cursorSlots.empty())
        return 0;

    // Find the line (row) whose worldY is closest to the hit point.
    constexpr float kLineEps = 1e-4f;
    float bestLineDist       = std::numeric_limits<float>::max();
    float bestLineY          = _cursorSlots.front().worldY;

    for (const auto &s : _cursorSlots)
    {
        const float d = std::abs(s.worldY - hit.y);
        if (d < bestLineDist)
        {
            bestLineDist = d;
            bestLineY    = s.worldY;
        }
    }

    // Within that line, find the slot whose worldX is closest to the hit point.
    std::size_t bestByte = 0;
    float bestXDist      = std::numeric_limits<float>::max();

    for (const auto &s : _cursorSlots)
    {
        if (std::abs(s.worldY - bestLineY) > kLineEps)
            continue;

        const float d = std::abs(s.worldX - hit.x);
        if (d < bestXDist)
        {
            bestXDist = d;
            bestByte  = s.byteOffset;
        }
    }

    return bestByte;
}

std::size_t TextEditorSystem::nextUtf8Pos(const std::string &text, std::size_t pos)
{
    if (pos >= text.size())
        return text.size();

    ++pos;
    while (pos < text.size() && isContinuationByte(static_cast<unsigned char>(text[pos])))
        ++pos;

    return pos;
}

bool TextEditorSystem::isContinuationByte(unsigned char byte) { return (byte & 0xC0u) == 0x80u; }
