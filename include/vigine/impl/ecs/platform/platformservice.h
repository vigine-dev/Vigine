#pragma once

/**
 * @file platformservice.h
 * @brief Concrete service that exposes window-platform operations
 *        (window creation, visibility, native handle access, event
 *        handler binding) through the modern service container.
 */

#include "vigine/api/ecs/platform/iwindoweventhandler.h"
#include "vigine/api/service/abstractservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/base/name.h"
#include "vigine/result.h"

#include <memory>
#include <vector>

namespace vigine
{
class Entity;

namespace ecs
{
namespace platform
{
class WindowSystem;
class IWindowEventHandlerComponent;
class WindowComponent;

/**
 * @brief Platform service: owns a @c WindowSystem and mediates
 *        window lifecycle and event-handler binding for clients.
 *
 * Created by the engine and registered on the modern
 * @ref vigine::context::AbstractContext via @c registerService. The
 * service exposes a window-centric API (create, show, query native
 * handle, bind an @c IWindowEventHandlerComponent) built on top of
 * the ECS-side @c WindowSystem.
 *
 * Wrapper base: the service derives from
 * @ref vigine::service::AbstractService. Ownership of the underlying
 * @c WindowSystem is taken at construction time through a
 * @c std::unique_ptr — there is no post-construction setter. The
 * service tears the system down through its private member when the
 * service itself is destroyed.
 *
 * Entity binding: the service holds no per-entity state of its own.
 * Callers pass the target entity to the per-call methods that need
 * it (@ref windowComponents, @ref windowEventHandlers,
 * @ref bindWindowEventHandler).
 */
class PlatformService : public vigine::service::AbstractService
{
  public:
    /**
     * @brief Constructs the service taking ownership of @p windowSystem.
     *
     * The @ref AbstractContext default registration creates the service
     * with a default @c WindowSystem internally; applications that
     * need a different concrete window system pass a custom
     * @c std::unique_ptr<WindowSystem> here and register the resulting
     * service through @c IContext::registerService(svc, knownId) which
     * replaces the default at the slot.
     */
    PlatformService(const Name &name, std::unique_ptr<WindowSystem> windowSystem);
    ~PlatformService() override;

    /**
     * @brief Returns the instance name supplied at construction.
     */
    [[nodiscard]] const Name &name() const noexcept;

    /**
     * @brief Returns the owned @c WindowSystem.
     *
     * The pointer is valid for the service's lifetime; the service
     * owns the system through a @c std::unique_ptr and the accessor
     * never returns @c nullptr because the constructor refuses a null
     * argument (asserts in Debug, treats null as a constructor
     * pre-condition violation in Release).
     */
    [[nodiscard]] WindowSystem *windowSystem() const noexcept;

    [[nodiscard]] WindowComponent *createWindow();
    [[nodiscard]] vigine::Result showWindow(WindowComponent *window);

    /**
     * @brief Binds @p handler to @p window for events delivered to
     *        the entity addressed by @p entity.
     *
     * Callers pass the target entity explicitly; the service holds
     * no bound-entity slot of its own.
     */
    [[nodiscard]] vigine::Result bindWindowEventHandler(Entity *entity, WindowComponent *window,
                                                        IWindowEventHandlerComponent *handler);

    [[nodiscard]] void *nativeWindowHandle(WindowComponent *window) const;

    /**
     * @brief Lists the @c WindowComponent instances attached to
     *        @p entity.
     */
    [[nodiscard]] std::vector<WindowComponent *> windowComponents(Entity *entity) const;

    /**
     * @brief Lists every @c IWindowEventHandlerComponent bound to
     *        @p entity (across every window).
     */
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *> windowEventHandlers(Entity *entity) const;

    /**
     * @brief Lists @c IWindowEventHandlerComponent instances bound
     *        to @p entity for the specified @p window.
     */
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *>
    windowEventHandlers(Entity *entity, WindowComponent *window) const;

    /**
     * @brief Modern lifecycle entry point.
     *
     * Chains to @ref vigine::service::AbstractService::onInit so the
     * @ref isInitialised flag flips to @c true.
     */
    [[nodiscard]] vigine::Result onInit(vigine::IContext &context) override;

    /**
     * @brief Modern teardown entry point.
     *
     * The owned @c WindowSystem is torn down with the service itself
     * via the private @c unique_ptr; @ref onShutdown only chains to
     * @ref vigine::service::AbstractService::onShutdown so the
     * @ref isInitialised flag flips back to @c false.
     */
    [[nodiscard]] vigine::Result onShutdown(vigine::IContext &context) override;

  private:
    Name                          _name;
    std::unique_ptr<WindowSystem> _windowSystem;
};

using PlatformServiceUPtr = std::unique_ptr<PlatformService>;
using PlatformServiceSPtr = std::shared_ptr<PlatformService>;

} // namespace platform
} // namespace ecs
} // namespace vigine
