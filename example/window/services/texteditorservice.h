#pragma once

#include <vigine/api/service/abstractservice.h>

#include <memory>

namespace vigine
{
class IContext;
} // namespace vigine

struct TextEditState;
class TextEditorSystem;

/**
 * @brief App-scope service that owns the example's text-editor state
 *        and system pair.
 *
 * @ref TextEditorService wraps @c TextEditState and @c TextEditorSystem
 * (the example's UTF-8 buffer + input router) into a single
 * @ref vigine::service::IService so every editor-touching task
 * resolves them through @c apiToken()->service(...) instead of through
 * a chain of setters threaded through the task wiring.
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
 *   auto editorSystem = tes->textEditorSystem();
 * @endcode
 *
 * Wiring: the underlying @c TextEditorSystem needs a one-time
 * binding to the engine's entity manager + graphics service /
 * render system. @ref ensureWired performs that binding the first
 * time it is called and is idempotent on every subsequent call so
 * tasks can invoke it unconditionally without bookkeeping.
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
     *
     * The first call performs the lookup through @p context and
     * binds the system. Subsequent calls are no-ops. A bind failure
     * (entity manager has unexpected type, graphics service is
     * unavailable, render system pointer is null) leaves the wired
     * flag clear so a later call retries; the contract is "best
     * effort, idempotent on success".
     */
    void ensureWired(vigine::IContext &context);

    [[nodiscard]] std::shared_ptr<TextEditState>    state() const noexcept;
    [[nodiscard]] std::shared_ptr<TextEditorSystem> textEditorSystem() const noexcept;

  private:
    std::shared_ptr<TextEditState>    _state;
    std::shared_ptr<TextEditorSystem> _system;
    bool                              _wired{false};
};
