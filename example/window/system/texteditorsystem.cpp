#include "texteditorsystem.h"

#include <vigine/impl/ecs/entity.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

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

void TextEditorSystem::bind(vigine::EntityManager *entityManager,
                            vigine::ecs::graphics::GraphicsService *graphicsService,
                            vigine::ecs::graphics::RenderSystem *renderSystem)
{
    _entityManager   = entityManager;
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
    // Apply any pending swapchain resize first so the render pass that
    // follows this tick draws against the new size. Debounced 80 ms to
    // avoid thrashing the swapchain while the user actively drags the
    // window edge.
    if (auto *i = interaction())
        applyPendingResizeIfPaused(*i);

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

    if (!_state->dirty || !_graphicsService || !_entityManager)
        return;

    auto *em = _entityManager;
    if (!em)
        return;

    auto *textEditEntity = em->getEntityByAlias("TextEditEntity");
    if (!textEditEntity)
        return;

    float contentHeight = 0.0f;

    _renderSystem->bindEntity(textEditEntity);
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
    _renderSystem->unbindEntity();

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
    if (!_graphicsService || !_entityManager || delta == 0)
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
    if (!_entityManager || !_renderSystem || !isScrollbarEntity(pickedEntity))
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

void TextEditorSystem::onChar(const vigine::ecs::platform::TextEvent &event, uint8_t movementKeyMask)
{
    if (!_state)
        return;

    const bool isCtrlAltCombo = (event.modifiers & vigine::ecs::platform::KeyModifierControl) != 0 ||
                                (event.modifiers & vigine::ecs::platform::KeyModifierAlt) != 0;
    if (isCtrlAltCombo || movementKeyMask != 0)
        return;

    _state->insertCodepoint(event.codePoint);
}

bool TextEditorSystem::isEditorEntity(const vigine::Entity *entity) const
{
    if (!entity || !_entityManager)
        return false;

    auto *em = _entityManager;
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
    if (!_graphicsService || !_entityManager)
        return;

    const float maxScroll = (std::max)(0.0f, _contentHeight - _panelHeight);
    const float clamped   = (std::max)(0.0f, (std::min)(newOffset, maxScroll));
    const float delta     = clamped - _scrollOffsetY;
    if (std::abs(delta) < 1e-6f)
        return;

    auto *em = _entityManager;
    if (!em)
        return;

    auto *textEntity = em->getEntityByAlias("TextEditEntity");
    if (!textEntity)
        return;

    _scrollOffsetY = clamped;

    _renderSystem->bindEntity(textEntity);
    if (auto *rc = _graphicsService->renderComponent())
    {
        rc->scrollVertical(delta);
        if (_renderSystem)
            _renderSystem->markGlyphDirty();
    }
    _renderSystem->unbindEntity();
}

void TextEditorSystem::updateScrollbarVisuals()
{
    if (!_graphicsService || !_entityManager)
        return;

    auto *em = _entityManager;
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

    _renderSystem->bindEntity(trackEntity);
    if (auto *trackRc = _graphicsService->renderComponent())
    {
        auto t = trackRc->getTransform();
        t.setPosition({trackCenterX, trackCenterY, kScrollbarPlaneZ + _panelZBase});
        t.setScale({kScrollbarWidth, trackHeight, 0.01f});
        trackRc->setTransform(t);
    }
    _renderSystem->unbindEntity();

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

    _renderSystem->bindEntity(thumbEntity);
    if (auto *thumbRc = _graphicsService->renderComponent())
    {
        auto t = thumbRc->getTransform();
        t.setPosition(
            {trackCenterX, _scrollbarThumbCenterY, kScrollbarPlaneZ + _panelZBase + 0.001f});
        t.setScale({kScrollbarWidth * 0.82f, _scrollbarThumbHeight, 0.012f});
        thumbRc->setTransform(t);
    }
    _renderSystem->unbindEntity();
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
    if (!entity || !_entityManager)
        return false;

    auto *em = _entityManager;
    if (!em)
        return false;

    return entity == em->getEntityByAlias("TextEditScrollbarTrackEntity") ||
           entity == em->getEntityByAlias("TextEditScrollbarThumbEntity");
}

void TextEditorSystem::updateFocusFrameVisual(bool visible)
{
    if (!_graphicsService || !_entityManager)
        return;

    auto *em = _entityManager;
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
        _renderSystem->bindEntity(e);
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
        _renderSystem->unbindEntity();
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

// ===========================================================================
//  Window-interaction surface
//
//  The block below absorbs the focus / object-drag / mouse-ray / Ctrl /
//  movement-key / debounced-resize logic @c RunWindowTask used to carry
//  directly on its own object. The state lives on the bound
//  @ref TextEditorComponent record; the @c route* methods are the entry
//  points the @c TextEditorService facade forwards from
//  @c RunWindowTask's event router.
// ===========================================================================

namespace
{
constexpr unsigned int kKeyW               = 'W';
constexpr unsigned int kKeyA               = 'A';
constexpr unsigned int kKeyS               = 'S';
constexpr unsigned int kKeyD               = 'D';
constexpr unsigned int kKeyQ               = 'Q';
constexpr unsigned int kKeyE               = 'E';
constexpr unsigned int kKeyShift           = 0x10;
constexpr unsigned int kKeyLeftShift       = 0xA0;
constexpr unsigned int kKeyRightShift      = 0xA1;
constexpr unsigned int kKeyControl         = 0x11;
constexpr unsigned int kKeyLeftControl     = 0xA2;
constexpr unsigned int kKeyRightControl    = 0xA3;
constexpr unsigned int kKeyAlt             = 0x12;
constexpr unsigned int kKeyLeftAlt         = 0xA4;
constexpr unsigned int kKeyRightAlt        = 0xA5;
constexpr unsigned int kKeyRayToggle       = 'R';
constexpr unsigned int kKeyBillboardToggle = 'B';
constexpr unsigned int kKeyEscape          = 0x1B;

#ifdef _WIN32
std::string utf8FromWide(const wchar_t *wide)
{
    if (!wide)
        return {};

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
        return {};

    std::string result(static_cast<size_t>(needed - 1), '\0');
    static_cast<void>(
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), needed, nullptr, nullptr));
    return result;
}

std::wstring wideFromUtf8(const std::string &utf8)
{
    if (utf8.empty())
        return {};

    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (needed <= 1)
        return {};

    std::wstring result(static_cast<size_t>(needed - 1), L'\0');
    static_cast<void>(MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), needed));
    return result;
}
#endif
} // namespace

TextEditorComponent &
TextEditorSystem::bindInteractionEntity(vigine::Entity *entity)
{
    return _interactions[entity];
}

TextEditorComponent *TextEditorSystem::interaction(vigine::Entity *entity)
{
    auto it = _interactions.find(entity);
    return it != _interactions.end() ? &it->second : nullptr;
}

const TextEditorComponent *
TextEditorSystem::interaction(const vigine::Entity *entity) const
{
    auto it = _interactions.find(const_cast<vigine::Entity *>(entity));
    return it != _interactions.end() ? &it->second : nullptr;
}

TextEditorComponent *TextEditorSystem::interaction()
{
    return _interactions.empty() ? nullptr : &_interactions.begin()->second;
}

const TextEditorComponent *TextEditorSystem::interaction() const
{
    return _interactions.empty() ? nullptr : &_interactions.begin()->second;
}

vigine::Entity *TextEditorSystem::focusedEntity() const
{
    if (auto *i = interaction())
        return i->focusedEntity;
    return nullptr;
}

uint8_t TextEditorSystem::movementKeyMask() const
{
    if (auto *i = interaction())
        return i->movementKeyMask;
    return 0u;
}

// ---- Window-event router methods -----------------------------------------

void TextEditorSystem::routeMouseButtonDown(vigine::ecs::platform::MouseButton button,
                                            int x, int y)
{
    auto *i = interaction();
    if (!i)
        return;

    if (button == vigine::ecs::platform::MouseButton::Left)
    {
        vigine::Entity *picked =
            _renderSystem ? _renderSystem->pickFirstIntersectedEntity(x, y) : nullptr;

        i->lastMouseRayX     = x;
        i->lastMouseRayY     = y;
        i->hasMouseRaySample = true;

        const bool pickedTextEditor = isEditorEntity(picked);

        // Freeze click marker first, then build ray from the same click point.
        updateMouseClickSphereVisualization(*i, x, y);
        updateMouseRayVisualization(*i, x, y);

        bool consumedByScrollbar = false;
        if (pickedTextEditor)
            consumedByScrollbar = onMouseButtonDown(x, y, picked);

        if (pickedTextEditor && !consumedByScrollbar)
            onEditorClick(x, y);

        // In Ctrl camera-unlock mode keep current focus unchanged.
        if (!i->ctrlHeld)
        {
            setFocusedEntityImpl(*i, picked);

            if (i->focusedEntity)
            {
                i->movementKeyMask = 0;
                if (_renderSystem)
                {
                    _renderSystem->setMoveForwardActive(false);
                    _renderSystem->setMoveBackwardActive(false);
                    _renderSystem->setMoveLeftActive(false);
                    _renderSystem->setMoveRightActive(false);
                    _renderSystem->setMoveUpActive(false);
                    _renderSystem->setMoveDownActive(false);
                    _renderSystem->setSprintActive(false);
                }
            }
        }
    }

    if (_renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!i->focusedEntity || i->ctrlHeld || i->objectDragActive))
        _renderSystem->beginCameraDrag(x, y);
}

void TextEditorSystem::routeMouseButtonUp(vigine::ecs::platform::MouseButton button,
                                          int /*x*/, int /*y*/)
{
    auto *i = interaction();
    if (!i)
        return;

    if (button == vigine::ecs::platform::MouseButton::Left)
        onMouseButtonUp();

    if (_renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!i->focusedEntity || i->ctrlHeld || i->objectDragActive))
        _renderSystem->endCameraDrag();
}

void TextEditorSystem::routeMouseMove(int x, int y)
{
    auto *i = interaction();
    if (!i)
        return;

    i->lastMouseRayX     = x;
    i->lastMouseRayY     = y;
    i->hasMouseRaySample = true;

    if (i->objectDragActive)
        updateObjectDrag(*i, x, y, /*suppressZDelta=*/true);

    onMouseMove(x, y);

    if (_renderSystem &&
        (!i->focusedEntity || i->ctrlHeld || i->objectDragActive))
        _renderSystem->updateCameraDrag(x, y);
}

void TextEditorSystem::routeMouseWheel(int delta, int x, int y)
{
    auto *i = interaction();
    if (!i)
        return;

    // In object-drag mode: wheel adjusts object distance from camera.
    if (i->objectDragActive)
    {
        const float wheelSteps             = static_cast<float>(delta) / 120.0f;
        const float factor                 = std::pow(0.90f, -wheelSteps);
        i->objectDragDistanceFromCamera    = (std::max)(0.15f,
                                                          i->objectDragDistanceFromCamera * factor);
        // Allow Z movement so the object actually moves in depth, not just XY.
        updateObjectDrag(*i, x, y, /*suppressZDelta=*/false);
        return;
    }

    // When text editor is focused (and Ctrl not held): scroll text, not camera.
    if (i->focusedEntity && !i->ctrlHeld)
    {
        onMouseWheel(delta);
        return;
    }

    if (_renderSystem && (!i->focusedEntity || i->ctrlHeld))
        _renderSystem->zoomCamera(delta);
}

void TextEditorSystem::routeKeyDown(const vigine::ecs::platform::KeyEvent &event)
{
    auto *i = interaction();
    if (!i)
        return;

    if (event.keyCode == kKeyControl || event.keyCode == kKeyLeftControl ||
        event.keyCode == kKeyRightControl)
        i->ctrlHeld = true;

    if (!event.isRepeat &&
        (event.keyCode == kKeyAlt || event.keyCode == kKeyLeftAlt ||
         event.keyCode == kKeyRightAlt))
    {
        if (i->objectDragActive)
        {
            endObjectDrag(*i);
        } else if (i->focusedEntity)
        {
            const int mx = i->hasMouseRaySample ? i->lastMouseRayX : 0;
            const int my = i->hasMouseRaySample ? i->lastMouseRayY : 0;
            static_cast<void>(beginObjectDrag(*i, i->focusedEntity, mx, my));
        }
        return;
    }

    if (event.keyCode == kKeyRayToggle && !event.isRepeat)
    {
        i->mouseRayVisible = !i->mouseRayVisible;

        if (i->mouseRayVisible)
        {
            if (i->hasMouseRaySample)
                updateMouseRayVisualization(*i, i->lastMouseRayX, i->lastMouseRayY);
        } else if (ensureMouseRayEntity(*i) && _renderSystem)
        {
            _renderSystem->bindEntity(i->mouseRayEntity);
            if (auto *rc = _renderSystem->boundRenderComponent())
            {
                auto transform = rc->getTransform();
                transform.setPosition({0.0f, -100.0f, 0.0f});
                transform.setScale({0.01f, 0.01f, 0.01f});
                rc->setTransform(transform);
            }
            _renderSystem->unbindEntity();
        }
    }

    if (event.keyCode == kKeyBillboardToggle && !event.isRepeat)
    {
        if (_renderSystem)
            _renderSystem->toggleBillboard();
    }

    if (!i->focusedEntity || i->ctrlHeld || i->objectDragActive)
        updateCameraMovementKey(*i, event.keyCode, true);

    if (event.keyCode == kKeyEscape)
    {
        setFocusedEntityImpl(*i, nullptr);
        return;
    }

    if (handleClipboardShortcut(event))
        return;

    if (i->focusedEntity && isEditorEntity(i->focusedEntity))
        onKeyDown(event.keyCode);
}

void TextEditorSystem::routeKeyUp(const vigine::ecs::platform::KeyEvent &event)
{
    auto *i = interaction();
    if (!i)
        return;

    if (event.keyCode == kKeyControl || event.keyCode == kKeyLeftControl ||
        event.keyCode == kKeyRightControl)
        i->ctrlHeld = false;

    if (!i->focusedEntity || i->ctrlHeld || i->objectDragActive)
        updateCameraMovementKey(*i, event.keyCode, false);
}

void TextEditorSystem::routeChar(const vigine::ecs::platform::TextEvent &event)
{
    auto *i = interaction();
    if (!i)
        return;
    if (i->focusedEntity && isEditorEntity(i->focusedEntity))
        onChar(event, i->movementKeyMask);
}

void TextEditorSystem::routeWindowResized(int width, int height)
{
    auto *i = interaction();
    if (!i || !_renderSystem)
        return;
    if (width <= 0 || height <= 0)
        return;

    i->pendingResizeWidth  = static_cast<uint32_t>(width);
    i->pendingResizeHeight = static_cast<uint32_t>(height);
    i->resizePending       = true;
    i->lastResizeEvent     = std::chrono::steady_clock::now();
}

// ---- Implementation helpers (private) ------------------------------------

void TextEditorSystem::setFocusedEntityImpl(TextEditorComponent &i, vigine::Entity *entity)
{
    if (entity == i.focusedEntity)
        return;

    // Any focus change exits move mode.
    if (i.objectDragActive)
        endObjectDrag(i);

    if (i.focusedEntity && _renderSystem && i.hasFocusedOriginalScale)
    {
        _renderSystem->bindEntity(i.focusedEntity);
        if (auto *rc = _renderSystem->boundRenderComponent())
        {
            auto transform = rc->getTransform();
            transform.setScale(i.focusedOriginalScale);
            rc->setTransform(transform);
        }
        _renderSystem->unbindEntity();
    }

    i.focusedEntity           = entity;
    i.hasFocusedOriginalScale = false;

    // Editor entities receive focus for input but should not get the
    // scale-up visual effect.
    const bool isEditor = isEditorEntity(entity);

    setFocused(i.focusedEntity != nullptr && isEditor);

    if (i.focusedEntity && _renderSystem && !isEditor)
    {
        _renderSystem->bindEntity(i.focusedEntity);
        if (auto *rc = _renderSystem->boundRenderComponent())
        {
            auto transform              = rc->getTransform();
            i.focusedOriginalScale      = transform.getScale();
            transform.setScale(i.focusedOriginalScale * 1.08f);
            rc->setTransform(transform);
            i.hasFocusedOriginalScale   = true;
        }
        _renderSystem->unbindEntity();
    }
}

bool TextEditorSystem::ensureMouseRayEntity(TextEditorComponent &i)
{
    if (i.mouseRayEntity)
        return true;

    if (!_entityManager || !_renderSystem)
        return false;

    auto *existing = _entityManager->getEntityByAlias("MouseRayEntity");
    i.mouseRayEntity = existing ? static_cast<vigine::Entity *>(existing) : nullptr;
    if (!i.mouseRayEntity)
    {
        i.mouseRayEntity = _entityManager->createEntity();
        if (!i.mouseRayEntity)
            return false;

        _entityManager->addAlias(i.mouseRayEntity, "MouseRayEntity");
    }

    _renderSystem->createComponents(i.mouseRayEntity);
    _renderSystem->bindEntity(i.mouseRayEntity);
    auto *rc = _renderSystem->boundRenderComponent();
    if (!rc)
    {
        _renderSystem->unbindEntity();
        return false;
    }

    auto rayMesh = vigine::ecs::graphics::MeshComponent::createCube();
    rayMesh.setProceduralInShader(true, 36);
    rc->setMesh(rayMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("cube.vert.spv", "cube.frag.spv");
        rc->setShader(shader);
    }
    rc->setPickable(false);

    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, -100.0f, 0.0f});
    transform.setScale({0.01f, 0.01f, 0.01f});
    rc->setTransform(transform);

    _renderSystem->unbindEntity();
    return true;
}

bool TextEditorSystem::ensureMouseClickSphereEntity(TextEditorComponent &i)
{
    if (i.mouseClickSphereEntity)
        return true;

    if (!_entityManager || !_renderSystem)
        return false;

    auto *existing = _entityManager->getEntityByAlias("MouseClickSphereEntity");
    i.mouseClickSphereEntity = existing ? static_cast<vigine::Entity *>(existing) : nullptr;
    if (!i.mouseClickSphereEntity)
    {
        i.mouseClickSphereEntity = _entityManager->createEntity();
        if (!i.mouseClickSphereEntity)
            return false;

        _entityManager->addAlias(i.mouseClickSphereEntity, "MouseClickSphereEntity");
    }

    _renderSystem->createComponents(i.mouseClickSphereEntity);
    _renderSystem->bindEntity(i.mouseClickSphereEntity);
    auto *rc = _renderSystem->boundRenderComponent();
    if (!rc)
    {
        _renderSystem->unbindEntity();
        return false;
    }

    auto sphereMesh = vigine::ecs::graphics::MeshComponent::createCube();
    sphereMesh.setProceduralInShader(true, 768);
    rc->setMesh(sphereMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("sphere.vert.spv", "sphere.frag.spv");
        rc->setShader(shader);
    }
    rc->setPickable(false);

    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, -100.0f, 0.0f});
    transform.setScale({0.06f, 0.06f, 0.06f});
    rc->setTransform(transform);

    _renderSystem->unbindEntity();
    return true;
}

void TextEditorSystem::updateMouseRayVisualization(TextEditorComponent &i, int x, int y)
{
    if (!_renderSystem)
        return;

    if (!ensureMouseRayEntity(i))
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
        return;

    const glm::vec3 rayDirection  = glm::normalize(clickRayDirection);

    constexpr float kRayLength    = 60.0f;
    constexpr float kRayThickness = 0.012f;
    constexpr float kStartOffset  = 0.03f;

    const glm::vec3 rayStart      = clickRayOrigin + clickRayDirection * kStartOffset;

    const glm::vec3 center        = rayStart + rayDirection * (kRayLength * 0.5f);
    const glm::quat orientation   = glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), rayDirection);
    const glm::vec3 rotationEuler = glm::eulerAngles(orientation);

    _renderSystem->bindEntity(i.mouseRayEntity);
    if (auto *rc = _renderSystem->boundRenderComponent())
    {
        auto transform = rc->getTransform();
        if (i.mouseRayVisible)
        {
            transform.setPosition(center);
            transform.setRotation(rotationEuler);
            transform.setScale({kRayThickness, kRayThickness, kRayLength});
        } else
        {
            transform.setPosition({0.0f, -100.0f, 0.0f});
            transform.setScale({0.01f, 0.01f, 0.01f});
        }
        rc->setTransform(transform);
    }
    _renderSystem->unbindEntity();
}

void TextEditorSystem::updateMouseClickSphereVisualization(TextEditorComponent &i, int x, int y)
{
    if (!_renderSystem)
        return;

    if (!ensureMouseClickSphereEntity(i))
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
        return;

    constexpr float kStartOffset = 0.03f;
    const glm::vec3 sphereCenter = clickRayOrigin + clickRayDirection * kStartOffset;

    _renderSystem->bindEntity(i.mouseClickSphereEntity);
    if (auto *rc = _renderSystem->boundRenderComponent())
    {
        auto transform = rc->getTransform();
        transform.setPosition(sphereCenter);
        transform.setScale({0.05f, 0.05f, 0.05f});
        rc->setTransform(transform);
    }
    _renderSystem->unbindEntity();
}

bool TextEditorSystem::beginObjectDrag(TextEditorComponent &i, vigine::Entity *entity,
                                       int x, int y)
{
    if (!entity || !_renderSystem)
        return false;

    _renderSystem->bindEntity(entity);
    auto *rc = _renderSystem->boundRenderComponent();
    if (!rc)
    {
        _renderSystem->unbindEntity();
        return false;
    }

    const auto transform = rc->getTransform();
    _renderSystem->unbindEntity();

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return false;

    const float dirLen = glm::length(rayDirection);
    if (dirLen < 1e-6f)
        return false;
    rayDirection                          /= dirLen;

    i.objectDragDistanceFromCamera         = glm::dot(transform.getPosition() - rayOrigin,
                                                       rayDirection);
    if (i.objectDragDistanceFromCamera <= 0.0f)
        return false;

    const glm::vec3 hit = rayOrigin + rayDirection * i.objectDragDistanceFromCamera;

    i.objectDragActive       = true;
    i.objectDragEditorGroup  = isEditorEntity(entity);
    i.objectDragEntity       = entity;
    i.objectDragGrabOffset   = transform.getPosition() - hit;
    return true;
}

void TextEditorSystem::updateObjectDrag(TextEditorComponent &i, int x, int y, bool suppressZDelta)
{
    if (!i.objectDragActive || !i.objectDragEntity || !_renderSystem)
        return;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!_renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return;

    const float dirLen = glm::length(rayDirection);
    if (dirLen < 1e-6f)
        return;
    rayDirection           /= dirLen;

    const glm::vec3 hit     = rayOrigin + rayDirection * i.objectDragDistanceFromCamera;
    const glm::vec3 newPos  = hit + i.objectDragGrabOffset;

    _renderSystem->bindEntity(i.objectDragEntity);
    auto *dragRc = _renderSystem->boundRenderComponent();
    if (!dragRc)
    {
        _renderSystem->unbindEntity();
        return;
    }

    auto dragTr     = dragRc->getTransform();
    glm::vec3 delta = newPos - dragTr.getPosition();
    if (i.objectDragEditorGroup)
    {
        if (suppressZDelta)
            delta.z = 0.0f;
        else
        {
            delta.x = 0.0f;
            delta.y = 0.0f;
        }
    }

    dragTr.setPosition(dragTr.getPosition() + delta);
    dragRc->setTransform(dragTr);

    if (i.objectDragEditorGroup)
        dragRc->translateGlyphVertices(delta);

    _renderSystem->unbindEntity();

    if (i.objectDragEditorGroup && _entityManager)
    {
        const char *editorAliases[] = {
            "TextEditBgEntity",
            "TextEditEntity",
            "TextEditScrollbarTrackEntity",
            "TextEditScrollbarThumbEntity",
            "TextEditFocusTopEntity",
            "TextEditFocusBottomEntity",
            "TextEditFocusLeftEntity",
            "TextEditFocusRightEntity",
        };

        for (const char *alias : editorAliases)
        {
            auto *e = _entityManager->getEntityByAlias(alias);
            if (!e || e == i.objectDragEntity)
                continue;

            _renderSystem->bindEntity(e);
            if (auto *rc = _renderSystem->boundRenderComponent())
            {
                auto tr = rc->getTransform();
                tr.setPosition(tr.getPosition() + delta);
                rc->setTransform(tr);

                if (std::strcmp(alias, "TextEditEntity") == 0)
                    rc->translateGlyphVertices(delta);
            }
            _renderSystem->unbindEntity();
        }

        offsetEditorFrame(delta.x, delta.y, delta.z);
        refreshEditorLayout();
        _renderSystem->markGlyphDirty();
    }
}

void TextEditorSystem::endObjectDrag(TextEditorComponent &i)
{
    i.objectDragActive             = false;
    i.objectDragEditorGroup        = false;
    i.objectDragEntity             = nullptr;
    i.objectDragDistanceFromCamera = 0.0f;
    i.objectDragGrabOffset         = {0.0f, 0.0f, 0.0f};
}

void TextEditorSystem::updateCameraMovementKey(TextEditorComponent &i, unsigned int keyCode,
                                               bool pressed)
{
    if (!_renderSystem)
        return;

    auto setMoveMaskBit = [&i, pressed](uint8_t bit) {
        if (pressed)
            i.movementKeyMask = static_cast<uint8_t>(i.movementKeyMask | bit);
        else
            i.movementKeyMask = static_cast<uint8_t>(i.movementKeyMask & ~bit);
    };

    switch (keyCode)
    {
    case kKeyW:
        _renderSystem->setMoveForwardActive(pressed);
        setMoveMaskBit(1u << 0);
        break;
    case kKeyS:
        _renderSystem->setMoveBackwardActive(pressed);
        setMoveMaskBit(1u << 2);
        break;
    case kKeyA:
        _renderSystem->setMoveLeftActive(pressed);
        setMoveMaskBit(1u << 1);
        break;
    case kKeyD:
        _renderSystem->setMoveRightActive(pressed);
        setMoveMaskBit(1u << 3);
        break;
    case kKeyQ:
        _renderSystem->setMoveDownActive(pressed);
        setMoveMaskBit(1u << 4);
        break;
    case kKeyE:
        _renderSystem->setMoveUpActive(pressed);
        setMoveMaskBit(1u << 5);
        break;
    case kKeyShift:
    case kKeyLeftShift:
    case kKeyRightShift:
        _renderSystem->setSprintActive(pressed);
        break;
    default:
        break;
    }
}

void TextEditorSystem::applyPendingResizeIfPaused(TextEditorComponent &i)
{
    if (!i.resizePending || !_renderSystem)
        return;

    const auto now    = std::chrono::steady_clock::now();
    const bool paused = now - i.lastResizeEvent >= std::chrono::milliseconds(80);
    if (!paused)
        return;

    if (i.pendingResizeWidth == i.appliedResizeWidth &&
        i.pendingResizeHeight == i.appliedResizeHeight)
    {
        i.resizePending = false;
        return;
    }

    const bool resized = _renderSystem->resize(i.pendingResizeWidth, i.pendingResizeHeight);
    if (resized)
    {
        i.appliedResizeWidth  = i.pendingResizeWidth;
        i.appliedResizeHeight = i.pendingResizeHeight;
        i.lastResizeApply     = now;
    }
    i.resizePending = false;
}

bool TextEditorSystem::handleClipboardShortcut(const vigine::ecs::platform::KeyEvent &event)
{
    auto *i = interaction();
    if (!i || !i->focusedEntity)
        return false;
    if (!isEditorEntity(i->focusedEntity))
        return false;

    const bool ctrlPressed = (event.modifiers & vigine::ecs::platform::KeyModifierControl) != 0;
    if (!ctrlPressed)
        return false;

    if (event.keyCode == 'C' || event.keyCode == 'X')
    {
#ifdef _WIN32
        const std::wstring wide = wideFromUtf8(text());
        if (!wide.empty() && OpenClipboard(nullptr))
        {
            static_cast<void>(EmptyClipboard());
            const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
            HGLOBAL memory     = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (memory)
            {
                void *ptr = GlobalLock(memory);
                if (ptr)
                {
                    std::memcpy(ptr, wide.c_str(), bytes);
                    GlobalUnlock(memory);
                    static_cast<void>(SetClipboardData(CF_UNICODETEXT, memory));
                    memory = nullptr;
                }
            }
            if (memory)
                GlobalFree(memory);
            CloseClipboard();
        }
#endif

        if (event.keyCode == 'X')
            clearText();
        return true;
    }

    if (event.keyCode == 'V')
    {
#ifdef _WIN32
        if (OpenClipboard(nullptr))
        {
            HANDLE data = GetClipboardData(CF_UNICODETEXT);
            if (data)
            {
                auto *wide = static_cast<const wchar_t *>(GlobalLock(data));
                if (wide)
                {
                    insertUtf8(utf8FromWide(wide));
                    GlobalUnlock(data);
                }
            }
            CloseClipboard();
        }
#endif
        return true;
    }

    return false;
}
