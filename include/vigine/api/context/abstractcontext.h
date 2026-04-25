#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "vigine/api/context/contextconfig.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/service/iservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/result.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/stateid.h"
#include "vigine/taskflow/itaskflow.h"
#include "vigine/core/threading/ithreadmanager.h"

namespace vigine::context
{
/**
 * @brief Stateful abstract base that owns every engine-wide resource
 *        exposed through @ref IContext.
 *
 * @ref AbstractContext is level 4 of the wrapper recipe: it supplies
 * the @ref IContext accessors, owns the thread manager, the system
 * bus, the Level-1 wrappers, the user-bus registry, and the service
 * registry, and encodes the strict construction and destruction order
 * required by UD-9 + AD-5 C8. A concrete closer (see
 * @c Context in @c include/vigine/impl/context) seals the chain so the
 * factory can hand out @c std::unique_ptr<IContext>.
 *
 * The class carries state, so it uses the project's @c Abstract
 * naming convention. All data members are @c private per the strict
 * encapsulation rule; derived classes that need to inspect the
 * substrate go through @c protected accessors.
 *
 * Construction order (encoded by member declaration order in the
 * private block below):
 *   1. @ref core::threading::IThreadManager -- created first so every
 *      downstream component can depend on it.
 *   2. @ref messaging::IMessageBus (system) -- created second; takes
 *      a reference to the thread manager.
 *   3. @ref ecs::IECS -- Level-1 wrapper; default-constructed.
 *   4. @ref statemachine::IStateMachine -- Level-1 wrapper;
 *      default-constructed.
 *   5. @ref taskflow::ITaskFlow -- Level-1 wrapper;
 *      default-constructed.
 *   6. user-bus registry (empty at construction).
 *   7. service registry (empty at construction).
 *   8. freeze flag (cleared at construction).
 *
 * Destruction is the reverse of the above because C++ destructs
 * members in the reverse order of declaration; the freeze flag dies
 * first, then the registries, then the Level-1 wrappers, then the
 * system bus, then the thread manager last. Services registered
 * through @ref registerService are destroyed along with the service
 * registry before the Level-1 wrappers tear down.
 *
 * Freeze semantics (UD-9):
 *   - The freeze flag is an @c std::atomic<bool>. @ref freeze flips
 *     it to @c true; @ref isFrozen reads it; every mutator takes the
 *     registry mutex and returns @ref Result::Code::TopologyFrozen
 *     when the flag is set. A winning @ref freeze call and an
 *     in-progress mutator serialise through the mutex: either the
 *     mutator sees the flag clear and completes, or it sees it set
 *     and reports the error; there is no partial update.
 *
 * INV-11 compliance: the header imports Level-1 wrapper interfaces
 * (`IMessageBus`, `IECS`, `IStateMachine`, `ITaskFlow`,
 * `IThreadManager`) and POD handles (`BusConfig`, `BusId`,
 * `ServiceId`). It does not include any graph primitive header and
 * does not mention `NodeId`, `EdgeId`, `IGraph`, or any graph visitor
 * type.
 */
class AbstractContext : public IContext
{
  public:
    ~AbstractContext() override;

    // ------ IContext: messaging ------

    [[nodiscard]] messaging::IMessageBus &systemBus() override;

    [[nodiscard]] std::shared_ptr<messaging::IMessageBus>
        createMessageBus(const messaging::BusConfig &config) override;

    [[nodiscard]] std::shared_ptr<messaging::IMessageBus>
        messageBus(messaging::BusId id) const override;

    // ------ IContext: Level-1 wrappers ------

    [[nodiscard]] ecs::IECS                     &ecs() override;
    [[nodiscard]] statemachine::IStateMachine   &stateMachine() override;
    [[nodiscard]] taskflow::ITaskFlow           &taskFlow() override;

    // ------ IContext: threading ------

    [[nodiscard]] core::threading::IThreadManager &threadManager() override;

    // ------ IContext: service registry ------

    [[nodiscard]] std::shared_ptr<service::IService>
        service(service::ServiceId id) const override;

    [[nodiscard]] Result
        registerService(std::shared_ptr<service::IService> service) override;

    // ------ IContext: engine-token factory ------

    [[nodiscard]] std::unique_ptr<vigine::engine::IEngineToken>
        makeEngineToken(vigine::statemachine::StateId boundState) override;

    // ------ Engine-internal scope-bypass accessors -------------------------
    //
    // The accessors below mirror the gated domain accessors on
    // @ref IContext but stay OFF the public @ref IContext interface --
    // they exist so engine-internal code (the concrete EngineToken,
    // diagnostics, lifecycle wiring) can reach the underlying
    // resources without going through a state-scoped token. The
    // public surface keeps the gated semantics; the engine reaches
    // the substrate directly through the @c *Internal trio. This
    // matches the R-StateScope architecture rule: tasks see a gated
    // view; the engine itself sees a direct view.
    //
    // Naming: the @c *Internal suffix flags the engine-internal scope.
    // Public on @ref AbstractContext (concrete closers consume them
    // verbatim) but never lifted to @ref IContext.
    // ----------------------------------------------------------------------

    /**
     * @brief Engine-internal direct accessor for @ref service.
     *
     * Mirrors @ref IContext::service: looks up the service registered
     * under @p id without taking the gated detour through an
     * @ref vigine::engine::IEngineToken. Returns the raw shared
     * pointer so callers can branch on null without juggling a
     * @ref vigine::engine::Result wrapper. Used by
     * @ref vigine::engine::EngineToken's gated accessor body and by
     * any future engine-side diagnostics that want to inspect the
     * service registry without minting a token first.
     *
     * The thread-safety contract matches @ref IContext::service: the
     * registry mutex serialises the lookup against concurrent
     * @ref registerService calls.
     */
    [[nodiscard]] std::shared_ptr<service::IService>
        serviceInternal(service::ServiceId id) const;

    /**
     * @brief Engine-internal direct accessor for @ref ecs.
     *
     * Mirrors @ref IContext::ecs and returns the same reference;
     * exposed under the @c *Internal name so engine-side code paths
     * that resolve domain accessors uniformly through the
     * @c *Internal surface (not through the gated token) have a
     * consistent entry point. The reference stays live for the
     * lifetime of this @ref AbstractContext.
     */
    [[nodiscard]] ecs::IECS &ecsInternal() noexcept;

    // ------ IContext: lifecycle ------

    void              freeze() noexcept override;
    [[nodiscard]] bool isFrozen() const noexcept override;

    AbstractContext(const AbstractContext &)            = delete;
    AbstractContext &operator=(const AbstractContext &) = delete;
    AbstractContext(AbstractContext &&)                 = delete;
    AbstractContext &operator=(AbstractContext &&)      = delete;

  protected:
    /**
     * @brief Constructs the aggregator with the strict ctor order
     *        described on the class docstring.
     *
     * Step 1 builds the thread manager from @p config.threading. Step
     * 2 builds the system bus from @p config.systemBus, passing the
     * thread manager in. Steps 3--5 build the three Level-1 wrappers
     * from their default factories. Step 6 opens the user-bus
     * registry. Step 7 opens the service registry. Step 8 clears the
     * freeze flag.
     *
     * If any construction step throws, RAII unwinds the steps that
     * already completed in reverse order: partial state never escapes
     * the constructor.
     */
    explicit AbstractContext(const ContextConfig &config);

    /**
     * @brief Returns the stable index the next service registration
     *        will take in the service registry.
     *
     * Exposed as @c protected so derived closers can use it for their
     * own bookkeeping (e.g. diagnostics); the public API does not
     * surface the registry's internal layout.
     */
    [[nodiscard]] std::size_t nextServiceIndex() const noexcept;

  private:
    /**
     * @brief Allocates a fresh @ref service::ServiceId for the service
     *        about to be stored in the registry.
     *
     * Called from @ref registerService while holding @ref _registryMutex.
     * Increments @ref _serviceGeneration so the issued id carries a
     * non-zero generation per the @ref service::ServiceId::valid
     * contract.
     */
    service::ServiceId allocateServiceId() noexcept;

    // ------ Wrappers + buses (declaration order == construction order) ------

    /**
     * @brief First member: the thread manager.
     *
     * Declared first because every downstream component (system bus,
     * Level-1 wrappers, services) depends on it at construction time.
     * Destructed last because C++ tears down members in reverse
     * declaration order. The engine relies on this ordering to drain
     * bus workers and sync primitives before the thread manager joins
     * its pool.
     */
    std::unique_ptr<core::threading::IThreadManager> _threadManager;

    /**
     * @brief Second member: the engine-wide system bus.
     *
     * Held as @c std::shared_ptr so that facades and services can
     * keep a handle independent of the registry; the context retains
     * its own reference for the context's lifetime.
     */
    std::shared_ptr<messaging::IMessageBus> _systemBus;

    /**
     * @brief Level-1 wrapper: ECS.
     */
    std::unique_ptr<ecs::IECS> _ecs;

    /**
     * @brief Level-1 wrapper: state machine.
     */
    std::unique_ptr<statemachine::IStateMachine> _stateMachine;

    /**
     * @brief Level-1 wrapper: task flow.
     */
    std::unique_ptr<taskflow::ITaskFlow> _taskFlow;

    // ------ Registries (mutable state guarded by _registryMutex) ------

    /**
     * @brief User buses created through @ref createMessageBus, keyed
     *        by their @ref messaging::BusId for @ref messageBus
     *        lookups.
     */
    std::unordered_map<std::uint32_t, std::shared_ptr<messaging::IMessageBus>> _userBuses;

    /**
     * @brief Services registered through @ref registerService, keyed
     *        by the index field of their stamped @ref service::ServiceId.
     */
    std::unordered_map<std::uint32_t, std::shared_ptr<service::IService>> _services;

    /**
     * @brief Monotonic counter for the index field of issued service ids.
     *
     * Incremented once per successful @ref registerService call. Never
     * reused: the aggregator does not recycle slots, so every live id
     * maps to exactly one registered service for the context's
     * lifetime. Atomic because @ref nextServiceIndex reads this value
     * without holding @ref _registryMutex — a plain uint32_t there
     * would be a data race with the mutator path.
     */
    std::atomic<std::uint32_t> _nextServiceIndex{1};

    /**
     * @brief Monotonic counter for the generation field of issued
     *        service ids.
     *
     * Always non-zero so that issued ids pass
     * @ref service::ServiceId::valid. Stored separately from the index
     * so that future generational recycling can use it without
     * disturbing the lookup map. Atomic for symmetry with
     * @ref _nextServiceIndex; the mutator path bumps them together.
     */
    std::atomic<std::uint32_t> _serviceGeneration{1};

    /**
     * @brief Mutex serialising mutators against each other, against
     *        @ref freeze, and against read accessors.
     *
     * Every registry access — including the read-side
     * @ref messageBus and @ref service paths — acquires this mutex
     * via `std::scoped_lock`. Previous revisions of this comment
     * claimed "Read accessors take no lock"; the shipped impl
     * always did, and callers acted on the lock-free claim could
     * see shared_ptr copies race the mutator side. The wording
     * now matches the shipped behaviour. A future change may
     * promote this to a `std::shared_mutex` so concurrent reads
     * don't serialise on each other, but that is an optimisation,
     * not a required correctness step under the "mutate only
     * before freeze" contract.
     */
    mutable std::mutex _registryMutex;

    /**
     * @brief Freeze flag.
     *
     * Atomic so callers can observe its state without taking the
     * mutex. A successful @ref freeze flips it to @c true; once true,
     * the flag never flips back. Mutators read it under the mutex so
     * the freeze transition is atomic with respect to in-flight
     * registrations.
     */
    std::atomic<bool> _frozen{false};
};

} // namespace vigine::context
