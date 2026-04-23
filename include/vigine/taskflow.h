#pragma once

#include "abstracttask.h"
#include "payload/payloadtypeid.h"
#include "result.h"
#include "threading/threadaffinity.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace vigine::messaging
{
class ISubscriber;
class ISubscriptionToken;
} // namespace vigine::messaging

namespace vigine::signalemitter
{
class ISignalEmitter;
} // namespace vigine::signalemitter

namespace vigine
{

enum class RouteType
{
    MouseEvent,
    KeyPressEvent,
    KeyRleaseEvent
};

class TaskFlow
{
    using TaskUPtr            = std::unique_ptr<AbstractTask>;
    using TaskContainer       = std::vector<TaskUPtr>;
    using Transition          = std::pair<Result::Code, AbstractTask *>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap       = std::unordered_map<AbstractTask *, TransitionContainer>;

    // Internal subscriber wrapper that re-posts an incoming message to the
    // thread selected by a caller-supplied ThreadAffinity. Defined in the
    // .cpp so the public header is not coupled to messaging / threading
    // implementation classes.
    class ScheduledDelivery;

    using ScheduledDeliveryUPtr = std::unique_ptr<ScheduledDelivery>;
    using SubscriptionTokenUPtr = std::unique_ptr<messaging::ISubscriptionToken>;
    using TokenContainer        = std::vector<SubscriptionTokenUPtr>;
    using ScheduledContainer    = std::vector<ScheduledDeliveryUPtr>;

  public:
    // Constructor and destructor are out-of-line because _scheduledDeliveries
    // holds unique_ptr<ScheduledDelivery> with ScheduledDelivery only
    // forward-declared in this header. The definition lives in
    // src/taskflow.cpp so every TU that instantiates TaskFlow (example/,
    // tests/, etc.) resolves the hidden destructor call through the
    // out-of-line defaulted dtor instead of requiring the full
    // ScheduledDelivery definition inline.
    TaskFlow();
    virtual ~TaskFlow();

    // Add a task and return pointer to it
    [[nodiscard]] AbstractTask *addTask(TaskUPtr task);

    // Remove a task
    void removeTask(AbstractTask *task);

    // Add a transition between tasks
    [[nodiscard]] Result route(AbstractTask *from, AbstractTask *to,
                               Result::Code resultCode = Result::Code::Success);

    /**
     * @brief Wires a signal edge from @p from to @p to.
     *
     * Subscribes @p to (after casting it to @ref messaging::ISubscriber) on
     * the currently installed @ref signalemitter::ISignalEmitter for messages
     * whose @ref messaging::MessageFilter matches
     * @ref messaging::MessageKind::Signal and the supplied @p signalType.
     * @p from is validated as registered in this flow but is not persisted
     * anywhere — the edge's producer identity is not retained. The actual
     * emission is driven by whichever code calls the emitter's @c emit
     * with a matching @c PayloadTypeId.
     *
     * Threading:
     *   - @p affinity defaults to @ref threading::ThreadAffinity::Any.
     *     Inside @c TaskFlow::signal, @c Any is re-interpreted as
     *     "run the handler synchronously on the emitter's thread, with no
     *     @ref threading::IThreadManager involvement". This is a local
     *     re-definition of the generic "engine picks a fast worker"
     *     meaning documented on @ref threading::ThreadAffinity::Any for
     *     @ref threading::IThreadManager::schedule; see that header for
     *     the dispatch-time semantics.
     *   - Any other @p affinity value wraps @p to in an internal
     *     subscriber adapter that, on delivery from the emitter, re-posts
     *     the handler call through
     *     @ref threading::IThreadManager::schedule. The emitter thread
     *     therefore does not block on the handler.
     *
     * Failures:
     *   - Null @p from or @p to, or @p from / @p to not registered with
     *     this flow, returns an error @ref Result.
     *   - A @ref threading::ThreadAffinity::Named value returns an error;
     *     @c Named affinities require a @ref threading::NamedThreadId and
     *     are routed through @ref threading::IThreadManager::scheduleOnNamed
     *     instead of @c schedule.
     *   - No @ref signalemitter::ISignalEmitter installed via
     *     @ref setSignalEmitter returns an error.
     *   - @p to not implementing @ref messaging::ISubscriber returns an
     *     error.
     *
     * Ownership:
     *   - The returned subscription token is stored inside this flow and
     *     kept alive for the flow's lifetime; destroying the flow tears
     *     every subscription down via the RAII contract of
     *     @ref messaging::ISubscriptionToken. The same applies to the
     *     internal adapter for non-@c Any affinities — it dies with the
     *     flow, and its destructor is reached only after the bus has
     *     drained any in-flight dispatch thanks to the token dtor-blocks
     *     contract.
     */
    [[nodiscard]] Result signal(AbstractTask            *from,
                                AbstractTask            *to,
                                payload::PayloadTypeId   signalType,
                                threading::ThreadAffinity affinity
                                = threading::ThreadAffinity::Any);

    // Change current task
    void changeCurrentTaskTo(AbstractTask *newTask);

    // Get current task
    [[nodiscard]] AbstractTask *currentTask() const;

    // Run current task
    void runCurrentTask();

    // Check if there are tasks to run
    [[nodiscard]] bool hasTasksToRun() const;

    // Run the task flow
    void operator()();

    void setContext(Context *context);

    /**
     * @brief Installs the signal emitter used by @ref signal to wire
     *        subscriptions.
     *
     * Mirrors the @ref setContext injection pattern: the caller owns the
     * emitter and must keep it alive for at least as long as this flow.
     * Passing @c nullptr disables signal wiring; subsequent @ref signal
     * calls return an error @ref Result until a non-null emitter is
     * installed.
     */
    void setSignalEmitter(signalemitter::ISignalEmitter *emitter);

  private:
    bool isTaskRegistered(AbstractTask *task) const;

  private:
    TaskContainer                  _tasks;
    TransitionMap                  _transitions;
    AbstractTask                  *_currTask{nullptr};
    Context                       *_context{nullptr};
    signalemitter::ISignalEmitter *_signalEmitter{nullptr};

    // Adapter instances live here so every scheduled-delivery wrapper
    // outlives its subscription slot. The emitter stores raw pointers
    // to them; the tokens below cancel in their dtor, and only then is
    // it safe to destroy the adapters.
    ScheduledContainer _scheduledDeliveries;

    // Token order matters for teardown: token destruction blocks until
    // any in-flight dispatch on that slot has drained (see
    // ISubscriptionToken dtor-blocks contract). Declaring the tokens
    // AFTER `_scheduledDeliveries` guarantees tokens destroy FIRST
    // (reverse member order), draining in-flight dispatch on a wrapper
    // BEFORE the wrapper itself is torn down.
    TokenContainer _subscriptionTokens;
};

} // namespace vigine
