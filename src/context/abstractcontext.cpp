#include "vigine/context/abstractcontext.h"

#include <mutex>
#include <utility>

#include "vigine/ecs/factory.h"
#include "vigine/messaging/factory.h"
#include "vigine/service/abstractservice.h"
#include "vigine/statemachine/factory.h"
#include "vigine/taskflow/factory.h"
#include "vigine/threading/factory.h"

namespace vigine::context
{

AbstractContext::AbstractContext(const ContextConfig &config)
    // Step 1: thread manager first. Built before the initialiser list
    // touches any other wrapper so every downstream step has a live
    // thread manager reference to bind to.
    : _threadManager{threading::createThreadManager(config.threading)}
    // Step 2: system bus next. Takes the thread manager by reference so
    // its dispatch worker can schedule on the engine pool. The factory
    // returns a unique_ptr; we lift it into a shared_ptr so facades
    // and services can keep an independent handle.
    , _systemBus{std::shared_ptr<messaging::IMessageBus>{
          messaging::createMessageBus(config.systemBus, *_threadManager).release()}}
    // Steps 3--5: Level-1 wrappers from their default factories. Each
    // is self-contained; the default factories auto-provision the
    // minimum state callers need (e.g. the state machine registers one
    // default state per UD-3).
    , _ecs{ecs::createECS()}
    , _stateMachine{statemachine::createStateMachine()}
    , _taskFlow{taskflow::createTaskFlow()}
// Steps 6--8: empty registries + cleared freeze flag are covered by
// the member default initialisers declared on @c AbstractContext.
{
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

    std::shared_ptr<messaging::IMessageBus> shared{bus.release()};
    _userBuses.emplace(shared->id().value, shared);
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

threading::IThreadManager &AbstractContext::threadManager()
{
    return *_threadManager;
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
