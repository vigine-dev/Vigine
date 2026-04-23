#include <vigine/context.h>
#include <vigine/context/icontext.h>
#include <vigine/messaging/imessage.h>
#include <vigine/messaging/isubscriber.h>
#include <vigine/messaging/isubscriptiontoken.h>
#include <vigine/messaging/messagefilter.h>
#include <vigine/messaging/messagekind.h>
#include <vigine/messaging/routemode.h>
#include <vigine/payload/payloadtypeid.h>
#include <vigine/signalemitter/isignalemitter.h>
#include <vigine/taskflow.h>
#include <vigine/threading/irunnable.h>
#include <vigine/threading/ithreadmanager.h>
#include <vigine/threading/threadaffinity.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

namespace vigine {

// ---------------------------------------------------------------------
// ScheduledEnvelope — minimal IMessage that TaskFlow::signal synthesises
// when it re-posts an incoming signal onto another thread.
//
// The originating envelope (posted by the emitter through
// IMessageBus::post) is owned by the bus; its lifetime ends when the
// bus's dispatch frame returns. A ScheduledDelivery wrapper receives
// that envelope by const reference on the bus worker and must hand off
// everything the target subscriber needs BEFORE returning — otherwise
// the target would dereference dangling memory on the worker thread.
//
// IMessagePayload does not expose a generic clone contract, so a
// wrapper that only sees IMessagePayload* cannot deep-copy the
// polymorphic payload. ScheduledEnvelope therefore preserves only the
// routing / identity metadata (kind, payloadTypeId, routeMode,
// correlationId, target, scheduledFor) and reports a null payload on
// the delivered envelope. Any target that needs payload data on a
// deferred edge must rely on out-of-band state (for example a queue
// filled by the sending task) or on a future extension of
// ISignalPayload that adds a shared-ownership clone contract.
// ---------------------------------------------------------------------

namespace {

class ScheduledEnvelope final : public messaging::IMessage {
public:
  ScheduledEnvelope(messaging::MessageKind kind,
                    payload::PayloadTypeId payloadTypeId,
                    messaging::RouteMode routeMode,
                    messaging::CorrelationId correlationId,
                    const messaging::AbstractMessageTarget *target,
                    std::chrono::steady_clock::time_point scheduledFor,
                    std::unique_ptr<signalemitter::ISignalPayload> payload)
      : _kind(kind), _payloadTypeId(payloadTypeId), _routeMode(routeMode),
        _correlationId(correlationId), _target(target),
        _scheduledFor(scheduledFor), _payload(std::move(payload)) {}

  [[nodiscard]] messaging::MessageKind kind() const noexcept override {
    return _kind;
  }

  [[nodiscard]] payload::PayloadTypeId payloadTypeId() const noexcept override {
    return _payloadTypeId;
  }

  [[nodiscard]] const messaging::IMessagePayload *
  payload() const noexcept override {
    return _payload.get();
  }

  [[nodiscard]] const messaging::AbstractMessageTarget *
  target() const noexcept override {
    return _target;
  }

  [[nodiscard]] messaging::RouteMode routeMode() const noexcept override {
    return _routeMode;
  }

  [[nodiscard]] messaging::CorrelationId
  correlationId() const noexcept override {
    return _correlationId;
  }

  [[nodiscard]] std::chrono::steady_clock::time_point
  scheduledFor() const noexcept override {
    return _scheduledFor;
  }

private:
  messaging::MessageKind _kind;
  payload::PayloadTypeId _payloadTypeId;
  messaging::RouteMode _routeMode;
  messaging::CorrelationId _correlationId;
  const messaging::AbstractMessageTarget *_target;
  std::chrono::steady_clock::time_point _scheduledFor;
  // Heap-owned payload clone. ISignalPayload::clone() is called inside
  // ScheduledDelivery::onMessage before the source IMessage's dispatch
  // frame unwinds, so by the time the worker thread invokes the target
  // the envelope owns everything the subscriber needs to read.
  std::unique_ptr<signalemitter::ISignalPayload> _payload;
};

// ---------------------------------------------------------------------
// DeliverRunnable — IRunnable that re-invokes a target ISubscriber's
// onMessage with a pre-captured ScheduledEnvelope. Holds the envelope
// by value so the runnable owns everything the target will read.
//
// Teardown race protection:
//   The runnable captures a shared alive-flag that is created by the
//   owning ScheduledDelivery adapter and flipped to false in the
//   adapter's destructor. The TaskFlow class orders its members so that
//   the subscription tokens (which block on in-flight bus dispatch)
//   destruct BEFORE the ScheduledDelivery containers (which flip the
//   flags), and before both comes any still-queued runnable on the
//   thread manager. Once TaskFlow is gone, the flag has been flipped
//   to false, the runnable's early-out fires, and the target pointer
//   is never dereferenced. A residual race exists: a runnable already
//   past the check but not yet inside onMessage can still dereference
//   the target — full protection would require a second sync barrier
//   (e.g. a live-runnable counter joined at adapter destruction); see
//   the comment on the check in run() for the rationale for deferring
//   that work.
//
// Exception isolation:
//   onMessage is invoked inside a try / catch so that a throwing
//   subscriber does not escape the thread manager worker. The dispatch
//   boundary semantics mirror AbstractMessageBus::deliver — catch
//   std::exception + ..., log a diagnostic on stderr, convert to a
//   failing Result so the worker loop observes the failure and the
//   process is not terminated.
// ---------------------------------------------------------------------

class DeliverRunnable final : public threading::IRunnable {
public:
  DeliverRunnable(messaging::ISubscriber *target,
                  std::unique_ptr<ScheduledEnvelope> envelope,
                  std::shared_ptr<std::atomic<bool>> alive)
      : _target(target), _envelope(std::move(envelope)),
        _alive(std::move(alive)) {}

  [[nodiscard]] Result run() override {
    if (_target == nullptr)
      return Result(Result::Code::Error,
                    "DeliverRunnable has no target subscriber");
    if (_envelope == nullptr)
      return Result(Result::Code::Error,
                    "DeliverRunnable has no envelope to deliver");

    // Teardown check. If the adapter has been destroyed (and with it
    // the owning TaskFlow), the flag flipped to false and the target
    // pointer is no longer valid. Skip the dispatch. The load uses
    // acquire so the flipping store in the adapter destructor happens
    // before this load from the runnable's point of view. A narrow
    // window remains between this check passing and the onMessage
    // call entering the target; closing that window requires an
    // explicit live-runnable counter on the adapter and is out of
    // scope for this fix.
    if (_alive == nullptr
        || !_alive->load(std::memory_order_acquire))
      return Result();

    try {
      static_cast<void>(_target->onMessage(*_envelope));
    } catch (const std::exception &ex) {
      std::fprintf(stderr,
                   "[vigine::TaskFlow] scheduled onMessage threw: %s\n",
                   ex.what());
      return Result(Result::Code::Error,
                    "scheduled onMessage threw std::exception");
    } catch (...) {
      std::fprintf(stderr,
                   "[vigine::TaskFlow] scheduled onMessage threw a "
                   "non-std::exception object\n");
      return Result(Result::Code::Error,
                    "scheduled onMessage threw non-std::exception");
    }
    return Result();
  }

private:
  messaging::ISubscriber *_target;
  // Held by unique_ptr because IMessage's copy / move constructors are
  // deleted, so ScheduledEnvelope is neither copyable nor movable.
  // Heap allocation keeps the runnable itself movable (IThreadManager
  // takes ownership via unique_ptr<IRunnable>).
  std::unique_ptr<ScheduledEnvelope> _envelope;
  // Shared alive-flag with the owning ScheduledDelivery adapter. The
  // adapter creates this flag at construction, captures it in every
  // runnable it schedules, and flips it to false in its destructor.
  // shared_ptr keeps the atomic alive even if the adapter has been
  // torn down before the runnable runs.
  std::shared_ptr<std::atomic<bool>> _alive;
};

} // namespace

// ---------------------------------------------------------------------
// TaskFlow::ScheduledDelivery — private subscriber adapter installed on
// non-Any signal edges. The emitter's bus calls onMessage on this
// adapter inline on the emitter's thread (the signal bus is
// InlineOnly). The adapter captures a metadata-only envelope and hands
// the re-delivery off to IThreadManager::schedule so the emitter
// thread returns immediately.
// ---------------------------------------------------------------------

class TaskFlow::ScheduledDelivery final : public messaging::ISubscriber {
public:
  ScheduledDelivery(messaging::ISubscriber *target,
                    threading::IThreadManager &threadManager,
                    threading::ThreadAffinity affinity)
      : _target(target), _threadManager(threadManager), _affinity(affinity),
        _alive(std::make_shared<std::atomic<bool>>(true)) {}

  ~ScheduledDelivery() override {
    // Flip the shared alive-flag so any DeliverRunnable still queued
    // on the thread manager (or mid-way through its own run()) sees
    // false and early-exits without touching the now-dead target.
    // release-order matches the acquire-load on the runnable side.
    if (_alive != nullptr)
      _alive->store(false, std::memory_order_release);
  }

  [[nodiscard]] messaging::DispatchResult
  onMessage(const messaging::IMessage &message) override {
    // Snapshot everything the target will read. The source message dies
    // when the bus's dispatch frame unwinds; the scheduled runnable can
    // only safely touch memory it owns outright. IMessage deletes copy
    // / move so the envelope lives on the heap; the payload is cloned
    // through ISignalPayload::clone() so the worker thread sees its
    // own copy of the data, independent of the bus's lifetime.
    std::unique_ptr<signalemitter::ISignalPayload> clonedPayload;
    if (const auto *raw = message.payload()) {
      if (const auto *signalPayload =
              dynamic_cast<const signalemitter::ISignalPayload *>(raw)) {
        clonedPayload = signalPayload->clone();
      }
    }

    auto envelope = std::make_unique<ScheduledEnvelope>(
        message.kind(), message.payloadTypeId(), message.routeMode(),
        message.correlationId(), message.target(), message.scheduledFor(),
        std::move(clonedPayload));

    auto runnable = std::make_unique<DeliverRunnable>(
        _target, std::move(envelope), _alive);
    static_cast<void>(
        _threadManager.schedule(std::move(runnable), _affinity));

    // The adapter has accepted the message and will deliver it on the
    // worker thread. The bus treats the edge as handled so FanOut
    // walks the remaining subscribers as normal.
    return messaging::DispatchResult::Handled;
  }

private:
  messaging::ISubscriber *_target;
  threading::IThreadManager &_threadManager;
  threading::ThreadAffinity _affinity;
  // Shared alive-flag passed into every runnable this adapter schedules.
  // The flag is flipped to false in the adapter destructor; runnables
  // still pending on the thread manager observe the flip through an
  // acquire-load before dereferencing the (possibly already dead)
  // target subscriber. See DeliverRunnable for the residual-race note.
  std::shared_ptr<std::atomic<bool>> _alive;
};

// Out-of-line TaskFlow constructor and destructor. Declared here (not
// in the header) because the `_scheduledDeliveries` container holds
// `unique_ptr<ScheduledDelivery>` with ScheduledDelivery only
// forward-declared in taskflow.h. The defaulted member functions only
// compile in a TU that has the full ScheduledDelivery definition in
// scope, which is exactly this file.
TaskFlow::TaskFlow()  = default;
TaskFlow::~TaskFlow() = default;

AbstractTask *TaskFlow::addTask(TaskUPtr task) {
  if (!task)
    return nullptr;

  task->setContext(_context);

  // Store the task
  _tasks.push_back(std::move(task));
  return _tasks.back().get();
}

void TaskFlow::removeTask(AbstractTask *task) {
  if (!task || !isTaskRegistered(task))
    return;

  // Remove task from current task if it's the one being removed
  if (_currTask == task)
    _currTask = nullptr;

  // Remove all transitions involving this task
  _transitions.erase(task);
  for (auto &[_, transitions] : _transitions) {
    transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
                                     [task](const auto &transition) {
                                       return transition.second == task;
                                     }),
                      transitions.end());
  }

  // Remove the task itself
  _tasks.erase(
      std::remove_if(_tasks.begin(), _tasks.end(),
                     [task](const auto &t) { return t.get() == task; }),
      _tasks.end());
}

bool TaskFlow::isTaskRegistered(AbstractTask *task) const {
  if (!task)
    return false;

  return std::find_if(_tasks.begin(), _tasks.end(), [task](const auto &t) {
           return t.get() == task;
         }) != _tasks.end();
}

Result TaskFlow::route(AbstractTask *from, AbstractTask *to,
                       Result::Code resultCode) {
  if (!from || !to)
    return Result(Result::Code::Error,
                  "Invalid pointer provided for transition");

  if (!isTaskRegistered(from))
    return Result(Result::Code::Error, "From task is not registered");

  if (!isTaskRegistered(to))
    return Result(Result::Code::Error, "To task is not registered");

  _transitions[from].emplace_back(resultCode, to);

  return Result();
}

Result TaskFlow::signal(AbstractTask *from, AbstractTask *to,
                        payload::PayloadTypeId signalType,
                        threading::ThreadAffinity affinity) {
  if (!from || !to)
    return Result(Result::Code::Error,
                  "Invalid pointer provided for signal transition");

  if (!isTaskRegistered(from))
    return Result(Result::Code::Error, "From task is not registered");

  if (!isTaskRegistered(to))
    return Result(Result::Code::Error, "To task is not registered");

  // Named affinities carry a generational id that ThreadAffinity alone
  // cannot represent; IThreadManager::schedule rejects Named on
  // purpose and the caller must use scheduleOnNamed instead. TaskFlow
  // does not yet carry a NamedThreadId parameter on signal(), so the
  // edge is rejected outright.
  if (affinity == threading::ThreadAffinity::Named)
    return Result(Result::Code::Error,
                  "ThreadAffinity::Named is not supported on signal edges");

  if (_signalEmitter == nullptr)
    return Result(Result::Code::Error,
                  "No ISignalEmitter installed; call setSignalEmitter first");

  auto *subscriber = dynamic_cast<messaging::ISubscriber *>(to);
  if (subscriber == nullptr)
    return Result(Result::Code::Error,
                  "Target task does not implement messaging::ISubscriber");

  // Filter is scoped to Signal traffic carrying exactly signalType.
  // The emitter forces kind to Signal on forward anyway (see the doc
  // on ISignalEmitter::subscribeSignal), but setting it explicitly
  // here matches the intent and keeps the filter shape readable.
  messaging::MessageFilter filter{};
  filter.kind = messaging::MessageKind::Signal;
  filter.typeId = signalType;

  if (affinity == threading::ThreadAffinity::Any) {
    // Local re-interpretation of ThreadAffinity::Any: run the handler
    // synchronously on the emitter's thread. No scheduler involvement.
    // The subscriber is wired directly to the bus; the bus's
    // InlineOnly policy delivers inside the emit call.
    auto token = _signalEmitter->subscribeSignal(filter, subscriber);
    if (token == nullptr)
      return Result(Result::Code::Error,
                    "Signal emitter rejected the subscription");
    _subscriptionTokens.push_back(std::move(token));
    return Result();
  }

  // Non-Any path: the handler must run on a worker selected by the
  // thread manager, not on the emitter thread. IThreadManager lives
  // on the context installed through setContext; without it the edge
  // cannot be wired.
  if (_context == nullptr)
    return Result(Result::Code::Error,
                  "No context installed; threaded signal edges require "
                  "IThreadManager from IContext::threadManager");

  threading::IThreadManager &threadManager = _context->threadManager();

  auto delivery = std::make_unique<ScheduledDelivery>(subscriber, threadManager,
                                                      affinity);
  messaging::ISubscriber *wrapperRaw = delivery.get();

  auto token = _signalEmitter->subscribeSignal(filter, wrapperRaw);
  if (token == nullptr)
    return Result(Result::Code::Error,
                  "Signal emitter rejected the subscription");

  // Adapter must outlive the subscription slot; the token's dtor
  // blocks until any in-flight dispatch on the slot has drained, so
  // declaring the adapter vector BEFORE the token vector in the class
  // body guarantees the correct reverse-order destruction.
  _scheduledDeliveries.push_back(std::move(delivery));
  _subscriptionTokens.push_back(std::move(token));

  return Result();
}

// COPILOT_TODO: Перевіряти, що newTask зареєстрований у цьому TaskFlow, інакше
// сюди можна передати зовнішній або вже невалідний вказівник.
void TaskFlow::changeCurrentTaskTo(AbstractTask *newTask) {
  if (!newTask)
    return;

  // Update current task
  _currTask = newTask;
}

AbstractTask *TaskFlow::currentTask() const { return _currTask; }

void TaskFlow::runCurrentTask() {
  if (!_currTask)
    return;

  // Execute current task and get result
  auto currStatus = _currTask->execute();

  // Check possible transitions
  auto transitions = _transitions.find(_currTask);
  _currTask = nullptr;

  if (transitions == _transitions.end())
    return;

  for (const auto &[relStatus, relTask] : transitions->second) {
    if (relStatus != currStatus.code())
      continue;

    // Found matching transition
    changeCurrentTaskTo(relTask);
    break;
  }
}

bool TaskFlow::hasTasksToRun() const { return _currTask != nullptr; }

void TaskFlow::operator()() {
  while (hasTasksToRun()) {
    runCurrentTask();
  }
}

void TaskFlow::setContext(Context *context) {
  _context = context;

  std::ranges::for_each(_tasks, [&context](const TaskUPtr &taskUptr) {
    taskUptr->setContext(context);
  });
}

void TaskFlow::setSignalEmitter(signalemitter::ISignalEmitter *emitter) {
  _signalEmitter = emitter;
}

} // namespace vigine
