#pragma once

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/service/abstractservice.h>

#include <memory>

namespace vigine
{
class Entity;
class IContext;
} // namespace vigine

struct TextEditState;
class TextEditorSystem;

/**
 * @brief App-scope service that owns the example's text-editor state
 *        and system pair AND fronts every window-event hook the editor
 *        window needs.
 *
 * The service wraps @c TextEditState and @c TextEditorSystem (the
 * example's UTF-8 buffer + input router) so every editor-touching task
 * resolves them through @c apiToken()->service(...) instead of through
 * a chain of setters threaded through the task wiring. The window
 * event router methods (@ref onMouseButtonDown, @ref onMouseMove,
 * @ref onKeyDown, ...) forward verbatim to the underlying
 * @c TextEditorSystem so @c RunWindowTask can stay state-free and
 * push every callback through this facade.
 *
 * Registration:
 * @code
 *   context.registerService(std::make_shared<TextEditorService>(),
 *                           example::services::wellknown::textEditor);
 * @endcode
 *
 * Resolution from a task:
 * @code
 *   auto svc = apiToken()->service(example::services::wellknown::textEditor);
 *   if (!svc.ok()) return Error("text editor service unavailable");
 *   auto* tes = dynamic_cast<TextEditorService*>(&svc.value());
 *   tes->ensureWired(apiToken()->engine().context());
 *   tes->onMouseMove(x, y);  // → routed to TextEditorSystem::routeMouseMove
 * @endcode
 *
 * Wiring: the underlying @c TextEditorSystem needs a one-time binding
 * to the engine's entity manager + graphics service / render system.
 * @ref ensureWired performs that binding the first time it is called
 * and is idempotent so callers can invoke it unconditionally.
 *
 * Ownership: the service owns the @c TextEditState and
 * @c TextEditorSystem instances through @c std::shared_ptr so callers
 * that want to keep a handle past the service's own lifetime may copy
 * the pointer aside. Both objects live for the service's lifetime
 * inside the registry.
 */
class TextEditorService final : public vigine::service::AbstractService
{
  public:
    TextEditorService();
    ~TextEditorService() override;

    /**
     * @brief Idempotent one-shot wiring of the underlying
     *        @c TextEditorSystem to the engine's entity manager and
     *        graphics service / render system.
     */
    void ensureWired(vigine::IContext &context);

    /**
     * @brief Registers a fresh @ref TextEditorComponent against
     *        @p entity through @c TextEditorSystem::bindInteractionEntity.
     *
     * The example calls this once from @c SetupTextEditTask for the
     * @c MainWindow entity right after the editor visuals are built;
     * subsequent calls re-bind (or refresh) the component for the
     * given entity. Forwarded verbatim so callers do not need to
     * reach into the system.
     */
    void bindInteractionEntity(vigine::Entity *entity);

    [[nodiscard]] std::shared_ptr<TextEditState>    state() const noexcept;
    [[nodiscard]] std::shared_ptr<TextEditorSystem> textEditorSystem() const noexcept;

    // ---- Window-event router (forwards to TextEditorSystem::route*) ----

    void onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(const vigine::ecs::platform::KeyEvent &event);
    void onKeyUp(const vigine::ecs::platform::KeyEvent &event);
    void onChar(const vigine::ecs::platform::TextEvent &event);
    void onWindowResized(int width, int height);

    /**
     * @brief Per-frame editor tick.
     *
     * Forwards to @c TextEditorSystem::onFrame which drives the cursor
     * blink, applies any pending swapchain resize debounced by 80 ms,
     * and rebuilds the editor mesh when the buffer is dirty.
     */
    void onFrame();

  private:
    std::shared_ptr<TextEditState>    _state;
    std::shared_ptr<TextEditorSystem> _system;
    bool                              _wired{false};
};
