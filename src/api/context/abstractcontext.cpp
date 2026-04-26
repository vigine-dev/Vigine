#include "vigine/api/context/abstractcontext.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <utility>

#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/factory.h"
#include "vigine/api/messaging/isignalemitter.h"
#include "vigine/api/service/abstractservice.h"
#include "vigine/api/service/wellknown.h"
#include "vigine/impl/ecs/entitymanager.h"
#include "vigine/impl/ecs/factory.h"
#include "vigine/impl/ecs/graphics/graphicsservice.h"
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/impl/ecs/platform/platformservice.h"
#include "vigine/impl/ecs/platform/windowsystem.h"
#include "vigine/impl/engine/enginetoken.h"
#include "vigine/impl/messaging/signalemitter.h"
#include "vigine/api/statemachine/factory.h"
#include "vigine/api/taskflow/factory.h"
#include "vigine/core/threading/factory.h"

namespace vigine::engine
{
class IEngine;
} // namespace vigine::engine

namespace vigine::context
{

AbstractContext::AbstractContext(const ContextConfig &config)
    // Step 1: thread manager first. Built before the initialiser list
    // touches any other wrapper so every downstream step has a live
    // thread manager reference to bind to.
    : _threadManager{core::threading::createThreadManager(config.threading)}
    // Step 2: system bus next. Takes the thread manager by reference so
    // its dispatch worker can schedule on the engine pool. The factory
    // returns a unique_ptr; we lift it into a shared_ptr so facades
    // and services can keep an independent handle.
    //
    // Use the `shared_ptr(unique_ptr&&)` conversion rather than
    // `shared_ptr(unique_ptr.release())`. If the shared_ptr control-
    // block allocation throws (bad_alloc), the conversion overload
    // destroys the moved-in unique_ptr and releases the underlying
    // bus; the `.release()` form hands over a naked raw pointer and
    // leaks if the same allocation throws.
    , _systemBus{
          std::shared_ptr<messaging::IMessageBus>{
              messaging::createMessageBus(config.systemBus, *_threadManager)}}
    // Steps 3--5: Level-1 wrappers from their default factories. Each
    // is self-contained; the default factories auto-provision the
    // minimum state callers need (e.g. the state machine registers one
    // default state per UD-3).
    , _ecs{ecs::createECS()}
    , _stateMachine{statemachine::createStateMachine()}
    , _taskFlow{taskflow::createTaskFlow()}
    // Step 6: engine-environment defaults. The aggregator owns a
    // default EntityManager and SignalEmitter so every task observes a
    // live IContext::entityManager() / signalEmitter() reference
    // through @c apiToken() without anyone wiring them up explicitly.
    // Applications override either default through @ref setEntityManager
    // / @ref setSignalEmitter and the prior owner is destroyed via the
    // unique_ptr slot's RAII chain.
    , _entityManager{std::make_unique<vigine::EntityManager>()}
    , _signalEmitter{messaging::createSignalEmitter(*_threadManager,
                                                    messaging::sharedBusConfig())}
// Steps 7--9: empty registries + cleared freeze flag + null engine
// back-ref are covered by the member default initialisers declared
// on @c AbstractContext. The default Platform/Graphics services are
// built and auto-registered in the constructor body below because
// @ref registerService takes the registry mutex and the ctor body
// is the canonical place for that.
{
    // Step 10: default Platform / Graphics services. The aggregator
    // builds them with default WindowSystem / RenderSystem instances
    // owned through their service unique_ptr so every task observes a
    // live, working pair through @c apiToken()->service(...) without
    // anyone wiring them up explicitly. Applications that need a
    // different concrete platform / graphics implementation register
    // theirs via @ref registerService(svc, wellknown::*) which
    // replaces the default at the well-known slot.
    auto defaultPlatform =
        std::make_shared<ecs::platform::PlatformService>(
            Name("DefaultPlatform"),
            std::make_unique<ecs::platform::WindowSystem>("DefaultWindow"));
    if (auto r = registerService(defaultPlatform,
                                 service::wellknown::platformService);
        r.isError())
    {
        // Default registration failure is a programming error in the
        // engine bootstrap path: the well-known slot is empty at ctor
        // entry, the freeze flag is clear, and the service we just
        // built is non-null. Surface the violation in Debug; Release
        // continues without the default — application-side code will
        // still detect a missing service when it tries to look up the
        // well-known id.
        assert(false && "AbstractContext: default PlatformService registration failed");
    }

    auto defaultGraphics =
        std::make_shared<ecs::graphics::GraphicsService>(
            Name("DefaultGraphics"),
            std::make_unique<ecs::graphics::RenderSystem>("DefaultRender"));
    if (auto r = registerService(defaultGraphics,
                                 service::wellknown::graphicsService);
        r.isError())
    {
        assert(false && "AbstractContext: default GraphicsService registration failed");
    }
}

AbstractContext::~AbstractContext() = default;

// ---------------------------------------------------------------------
// Messaging
// ---------------------------------------------------------------------

messaging::IMessageBus &AbstractContext::systemBus()
{
    return *_systemBus;
}

std::shared_ptr<messaging::IMessageBus>
AbstractContext::createMessageBus(const messaging::BusConfig &config)
{
    // The freeze guard is checked under the registry mutex so a
    // concurrent freeze() call either blocks until this registration
    // completes (and then freezes the topology for future callers) or
    // arrives first and forces this call to fail fast. There is no
    // window where a bus gets registered after the flag is set.
    std::scoped_lock lock{_registryMutex};
    if (_frozen.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    auto bus = messaging::createMessageBus(config, *_threadManager);
    if (!bus)
    {
        return nullptr;
    }

    // Convert via `unique_ptr&&` so the control-block-alloc failure
    // path unwinds cleanly (the unique_ptr destroys the bus during
    // stack unwind); the prior `.release()` form leaked on bad_alloc.
    std::shared_ptr<messaging::IMessageBus> shared{std::move(bus)};
    // `emplace` returns `{iterator, bool}`; the `bool` is `false`
    // when the key already exists, which means two buses landed on
    // the same id — a programming error on the messaging factory
    // side. Silent ignore would let the first registration stay
    // live and silently dropped the new one; fail-fast instead.
    const auto [it, inserted] = _userBuses.emplace(shared->id().value, shared);
    if (!inserted)
    {
        return nullptr;
    }
    return shared;
}

std::shared_ptr<messaging::IMessageBus>
AbstractContext::messageBus(messaging::BusId id) const
{
    if (!id.valid())
    {
        return nullptr;
    }

    // The system bus answers to its own id first; fall through to the
    // user-bus registry when the id does not match the system bus so
    // callers can look up both kinds through one entry point.
    if (_systemBus && _systemBus->id() == id)
    {
        return _systemBus;
    }

    std::scoped_lock lock{_registryMutex};
    auto it = _userBuses.find(id.value);
    if (it == _userBuses.end())
    {
        return nullptr;
    }
    return it->second;
}

// ---------------------------------------------------------------------
// Level-1 wrappers
// ---------------------------------------------------------------------

ecs::IECS &AbstractContext::ecs()
{
    return *_ecs;
}

statemachine::IStateMachine &AbstractContext::stateMachine()
{
    return *_stateMachine;
}

taskflow::ITaskFlow &AbstractContext::taskFlow()
{
    return *_taskFlow;
}

// ---------------------------------------------------------------------
// Threading
// ---------------------------------------------------------------------

core::threading::IThreadManager &AbstractContext::threadManager()
{
    return *_threadManager;
}

// ---------------------------------------------------------------------
// Engine environment (default-built; replaceable via setter)
// ---------------------------------------------------------------------

IEntityManager &AbstractContext::entityManager()
{
    // The unique_ptr slot is filled in the ctor and never observed
    // null through public mutation paths (the setter asserts on null
    // in Debug and treats a null argument as a no-op in Release), so
    // the dereference is always safe between construction and
    // destruction.
    assert(_entityManager && "AbstractContext::entityManager: slot is null");
    return *_entityManager;
}

void AbstractContext::setEntityManager(std::unique_ptr<IEntityManager> entityManager)
{
    // The "default exists" invariant of @ref entityManager requires the
    // slot to stay non-null. A null replacement is treated as a no-op
    // in Release (and asserted in Debug) so callers cannot accidentally
    // strip the default by passing a null argument; replacing means
    // building a non-null replacement first.
    assert(entityManager && "AbstractContext::setEntityManager: replacement is null");
    if (!entityManager)
        return;
    _entityManager = std::move(entityManager);
}

messaging::ISignalEmitter &AbstractContext::signalEmitter()
{
    assert(_signalEmitter && "AbstractContext::signalEmitter: slot is null");
    return *_signalEmitter;
}

void AbstractContext::setSignalEmitter(std::unique_ptr<messaging::ISignalEmitter> signalEmitter)
{
    assert(signalEmitter && "AbstractContext::setSignalEmitter: replacement is null");
    if (!signalEmitter)
        return;
    _signalEmitter = std::move(signalEmitter);
}

engine::IEngine &AbstractContext::engine()
{
    // The engine back-pointer is wired in by
    // @ref engine::AbstractEngine's constructor body shortly after this
    // context is built; tasks reaching this accessor through their
    // engine token observe the live engine for the engine's entire
    // lifetime. A null read here would mean either the engine
    // bootstrap path skipped @ref setEngineBackRef (programming
    // error) or the context was constructed standalone outside any
    // engine (test fixture). Surface the misuse loudly in Debug;
    // Release crashes on the dereference, which is also acceptable
    // because the contract requires a non-null engine reference.
    assert(_engine && "AbstractContext::engine: engine back-ref is null");
    return *_engine;
}

void AbstractContext::setEngineBackRef(engine::IEngine *engine) noexcept
{
    _engine = engine;
}

// ---------------------------------------------------------------------
// Service registry
// ---------------------------------------------------------------------

std::shared_ptr<service::IService>
AbstractContext::service(service::ServiceId id) const
{
    if (!id.valid())
    {
        return nullptr;
    }

    // The registry is keyed on @c ServiceId::index; the lookup has to
    // also honour generation so a stale handle returned from a
    // previous registration (or typed-over from an unrelated
    // context) cannot alias a live slot. Generational slot recycling
    // itself is not yet on the surface (no @c unregisterService),
    // but generation-bump-on-every-registration already runs inside
    // @ref allocateServiceId, so two successive registrations issue
    // two different ids even when the index happens to line up.
    std::scoped_lock lock{_registryMutex};
    auto it = _services.find(id.index);
    if (it == _services.end())
    {
        return nullptr;
    }
    // If the registered service carries a stamped non-sentinel id
    // (the common case — AbstractService-derived services get
    // stamped during `registerService`), require an exact match on
    // the full `(index, generation)` pair. When the service does
    // not report a stamped id (`IService`-only implementations that
    // bypass the stamping hook), fall back to index-only match so
    // this path stays backward-compatible with custom service types
    // that do not plug into the `AbstractService::setId` surface.
    if (it->second)
    {
        const auto storedId = it->second->id();
        if (storedId.valid() && storedId != id)
        {
            return nullptr;
        }
    }
    return it->second;
}

Result AbstractContext::registerService(std::shared_ptr<service::IService> service)
{
    if (!service)
    {
        return Result{Result::Code::Error, "registerService: service is null"};
    }

    std::scoped_lock lock{_registryMutex};
    if (_frozen.load(std::memory_order_acquire))
    {
        return Result{
            Result::Code::TopologyFrozen,
            "registerService: context is frozen after Engine::run()"};
    }

    // The aggregator stamps a fresh id on every registration; concrete
    // services expose the stamped id through their own id() accessor
    // after registration. When the registered service derives from
    // `AbstractService` (the common case — the base carries the id
    // storage), we call the public `setId` so subsequent `id()`
    // reads return what we stamped. When the service is only an
    // `IService` (no AbstractService base), the aggregator keeps
    // the registry key working as the lookup and the caller-visible
    // id() falls back to whatever the concrete implementation
    // returns. The registry uses the locally-allocated index as the
    // lookup key so lookup does not depend on id() agreeing with
    // what we stamped.
    const service::ServiceId stamped = allocateServiceId();
    if (auto *abstractService = dynamic_cast<service::AbstractService *>(service.get()))
    {
        abstractService->setId(stamped);
    }
    _services.emplace(stamped.index, std::move(service));
    return Result{};
}

Result AbstractContext::registerService(std::shared_ptr<service::IService> service,
                                        service::ServiceId                 knownId)
{
    if (!service)
    {
        return Result{Result::Code::Error, "registerService: service is null"};
    }
    if (!knownId.valid())
    {
        return Result{Result::Code::Error,
                      "registerService: knownId is the invalid sentinel"};
    }

    // The well-known path requires the service to expose
    // @ref AbstractService::setId so the registry can stamp the
    // caller-provided id. IService-only implementations have no setId
    // hook; reject them rather than registering with a divergent id
    // that would fail the @ref service lookup's exact-pair match.
    auto *abstractService = dynamic_cast<service::AbstractService *>(service.get());
    if (abstractService == nullptr)
    {
        return Result{Result::Code::Error,
                      "registerService: knownId requires AbstractService base"};
    }

    std::scoped_lock lock{_registryMutex};
    if (_frozen.load(std::memory_order_acquire))
    {
        return Result{
            Result::Code::TopologyFrozen,
            "registerService: context is frozen after Engine::run()"};
    }

    // Replace the slot keyed by @p knownId.index. The previous
    // occupant (engine-built default or a prior caller-side
    // registration) is dropped from the @c _services map; if no
    // external caller held a @c shared_ptr to it, its destructor
    // runs as the map shrinks. Callers who intend to keep the prior
    // occupant alive should hold their own @c shared_ptr before
    // invoking this overload.
    abstractService->setId(knownId);
    _services[knownId.index] = std::move(service);

    return Result{};
}

// ---------------------------------------------------------------------
// Engine-token factory
// ---------------------------------------------------------------------

std::unique_ptr<vigine::engine::IEngineToken>
AbstractContext::makeEngineToken(vigine::statemachine::StateId boundState)
{
    // Mint a fresh @ref engine::EngineToken bound to @p boundState.
    // The token captures:
    //   - this aggregator by reference (so its gated accessors can
    //     delegate to @ref serviceInternal / @ref ecsInternal etc.),
    //   - the engine's @ref IStateMachine wrapper (so the token can
    //     register an invalidation listener via the
    //     @ref AbstractStateMachine subclass back-end and observe
    //     transitions out of @p boundState),
    //   - the aggregator's owned @ref ISignalEmitter façade so the
    //     ungated @c signalEmitter accessor returns the live wrapper
    //     directly (the file-private @c NullSignalEmitter stub the
    //     token carries as a fallback stays uninstantiated when this
    //     pointer is non-null, which it always is post-construction).
    //
    // The token does NOT inspect this aggregator's freeze flag: a task
    // can request a fresh token after @ref freeze (e.g. when a state
    // transition mints one for an onEnter handler) just like every
    // read-only accessor stays available post-freeze. The freeze
    // boundary blocks topology mutation; minting a state-scoped DI
    // handle is not a topology mutation.
    return std::make_unique<vigine::engine::EngineToken>(
        boundState, *this, *_stateMachine, _signalEmitter.get());
}

// ---------------------------------------------------------------------
// Engine-internal scope-bypass accessors
// ---------------------------------------------------------------------

std::shared_ptr<service::IService>
AbstractContext::serviceInternal(service::ServiceId id) const
{
    // Engine-internal direct lookup. Mirrors @ref service exactly --
    // the engine-side scope-bypass surface intentionally shares the
    // same generational guarantees as the public surface so callers
    // who switch between gated (token) and ungated (internal) code
    // paths observe identical results for the same @p id. The
    // implementation reuses @ref service to avoid drift; the only
    // reason this is a separate symbol is the R-StateScope split
    // between the public @ref IContext surface (gated through tokens
    // in the engine-token wrapper) and the engine-internal direct
    // surface that exists on @ref AbstractContext but not on
    // @ref IContext.
    return service(id);
}

ecs::IECS &AbstractContext::ecsInternal() noexcept
{
    // Engine-internal direct alias for @ref ecs. The underlying
    // wrapper lives for the aggregator's lifetime (it is owned
    // through @ref _ecs, the unique_ptr declared in this class) so
    // returning a reference is safe regardless of any state-machine
    // transition or freeze event.
    return *_ecs;
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

void AbstractContext::freeze() noexcept
{
    // Serialise against in-flight mutators so the topology is either
    // fully mutated pre-freeze or fully rejected post-freeze. The
    // scoped_lock makes the freeze point a strict boundary.
    std::scoped_lock lock{_registryMutex};
    _frozen.store(true, std::memory_order_release);
}

bool AbstractContext::isFrozen() const noexcept
{
    return _frozen.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------
// Protected helpers
// ---------------------------------------------------------------------

std::size_t AbstractContext::nextServiceIndex() const noexcept
{
    // Atomic read (no lock). Callers who need a monotonically-stable
    // reading relative to a registration should hold their own mutex
    // alongside; the atomic `load` here just closes the data race
    // with the mutator path writing via `allocateServiceId`.
    return _nextServiceIndex.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------

service::ServiceId AbstractContext::allocateServiceId() noexcept
{
    // Called under _registryMutex by @ref registerService. Non-zero
    // generation guarantees @ref ServiceId::valid returns true on the
    // issued handle. The atomic members are updated with relaxed
    // ordering — the mutex provides the happens-before between the
    // bump and the registry emplace; the atomics only exist so that
    // lock-free readers (`nextServiceIndex()`) do not race.
    const auto index      = _nextServiceIndex.load(std::memory_order_relaxed);
    const auto generation = _serviceGeneration.load(std::memory_order_relaxed);
    service::ServiceId id{index, generation};
    _nextServiceIndex.store(index + 1, std::memory_order_release);
    const auto nextGen = generation + 1;
    // Wrap-around guard: skip the invalid sentinel generation so
    // no future id accidentally compares equal to the default
    // @c ServiceId{}.
    _serviceGeneration.store(nextGen == 0 ? std::uint32_t{1} : nextGen,
                             std::memory_order_release);
    return id;
}

} // namespace vigine::context
