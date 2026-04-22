#pragma once

#include <memory>

#include "vigine/reactivestream/abstractreactivestream.h"
#include "vigine/reactivestream/ireactivesubscription.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::reactivestream
{

/**
 * @brief Concrete final reactive-stream facade.
 *
 * @ref DefaultReactiveStream is Level-5 of the five-layer wrapper recipe.
 * It provides the full @ref IReactiveStream implementation on top of
 * @ref AbstractReactiveStream:
 *
 *   - Cold publisher model: @ref subscribe creates a fresh, independent
 *     @ref IReactiveSubscription (and publisher) for each caller. Two
 *     subscribers on the same stream never share delivery state.
 *   - Backpressure: demand is tracked per subscription; the publisher
 *     calls @c onNext only while demand > 0.
 *   - @ref IReactiveSubscription::cancel is idempotent; the subscriber's
 *     @c onNext stops immediately after cancel returns.
 *   - Terminal signals (@c onComplete / @c onError) mark the subscription
 *     closed; the delivery path is a no-op afterwards.
 *   - @ref shutdown drains in-flight delivery, cancels every subscription
 *     with @c onComplete, and rejects new @ref subscribe calls. Idempotent.
 *
 * Callers obtain instances exclusively through @ref createReactiveStream;
 * they never construct this type by name.
 *
 * Thread-safety: @ref subscribe and @ref shutdown are safe to call from
 * any thread concurrently. The subscription registry is guarded by a
 * @c std::mutex.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref createReactiveStream returns @c std::unique_ptr<IReactiveStream>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultReactiveStream final : public AbstractReactiveStream
{
  public:
    /**
     * @brief Constructs the reactive-stream facade over @p bus.
     *
     * @p bus and @p threadManager must outlive this facade instance.
     */
    DefaultReactiveStream(vigine::messaging::IMessageBus    &bus,
                          vigine::threading::IThreadManager &threadManager);

    ~DefaultReactiveStream() override;

    // IReactiveStream
    [[nodiscard]] std::unique_ptr<IReactiveSubscription>
        subscribe(IReactiveSubscriber *subscriber) override;

    vigine::Result shutdown() override;

    /**
     * @brief Pushes @p payload to the FIRST active subscriber with
     *        outstanding demand — single-consumer / cold-publisher
     *        semantics.
     *
     * The facade is single-consumer by design: the payload is a
     * `std::unique_ptr<IMessagePayload>` and cannot be fanned out
     * without cloning. The driver walks the subscriber snapshot in
     * registration order, skips cancelled / terminal subscriptions
     * and subscribers whose demand is zero (backpressure), and
     * transfers ownership to the first live subscriber with non-zero
     * demand. When multiple subscribers share the stream, only that
     * first match receives the payload.
     *
     * Engine code that needs fan-out to many subscribers must post
     * one payload per subscriber (or use `IMessageBus` directly,
     * which is the correct seam for multi-consumer delivery).
     *
     * Returns @c Result::Code::Success when the payload landed on a
     * subscriber or was dropped because no subscriber had demand
     * (backpressure case — intentionally non-fatal). Returns an
     * error `Result` only on a null payload or a shut-down stream.
     */
    vigine::Result publish(std::unique_ptr<vigine::messaging::IMessagePayload> payload);

    /**
     * @brief Signals @c onComplete to every active subscriber.
     *
     * Marks each subscription terminal. Idempotent per subscription.
     */
    vigine::Result complete();

    /**
     * @brief Signals @c onError(@p error) to every active subscriber.
     *
     * Marks each subscription terminal. Idempotent per subscription.
     */
    vigine::Result fail(vigine::Result error);

    DefaultReactiveStream(const DefaultReactiveStream &)            = delete;
    DefaultReactiveStream &operator=(const DefaultReactiveStream &) = delete;
    DefaultReactiveStream(DefaultReactiveStream &&)                 = delete;
    DefaultReactiveStream &operator=(DefaultReactiveStream &&)      = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::reactivestream

// The factory declaration lives in a dedicated
// `include/vigine/reactivestream/factory.h` header, matching the
// convention used by every other facade: a factory-only public
// entry point that does NOT pull the concrete class body into
// callers' translation units. Callers that want just the factory
// include `factory.h`; callers that need to name the concrete
// `DefaultReactiveStream` (tests, engineering) continue to
// include this header.
#include "vigine/reactivestream/factory.h"
