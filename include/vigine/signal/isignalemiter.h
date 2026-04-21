#pragma once

#include "vigine/signal/isignal.h"

#include <functional>

namespace vigine
{

/**
 * @brief Interface for a task that can emit signals.
 *
 * A task that wants to send signals to other tasks must inherit this
 * interface and store the @ref SignalEmiterProxy injected by the engine
 * via @ref setProxyEmiter().
 *
 * Typical usage:
 * @code
 * // The engine sets the proxy before running the TaskFlow
 * emiterTask->setProxyEmiter([](ISignal *signal) {
 *     // deliver the signal to subscribed tasks
 * });
 *
 * // The emitter task invokes the proxy inside execute()
 * if (auto proxy = proxyEmiter())
 *     proxy(mySignal.get());
 * @endcode
 *
 * @see ISignal
 * @see ISignalBinder
 */
class ISignalEmiter // INV-10 EXEMPTION: predates convention; carries proxy state; rename tracked separately
{
  public:
    /**
     * @brief Alias for the proxy function through which signals are dispatched.
     *
     * The parameter is a non-owning pointer to the dispatched @ref ISignal.
     * Ownership stays with the emitter; the proxy must not store the pointer
     * beyond the call.
     */
    using SignalEmiterProxy  = std::function<void(ISignal *)>;

    virtual ~ISignalEmiter() = default;

    /**
     * @brief Sets the proxy function used to dispatch signals.
     *
     * Called by the engine or @c TaskFlow before the task is executed.
     * After this call the task may emit signals through the stored proxy.
     *
     * @param proxyEmiter A callable that accepts a pointer to @ref ISignal.
     *                    Passed by move.
     */
    void setProxyEmiter(SignalEmiterProxy proxyEmiter);

  protected:
    /**
     * @brief Returns a reference to the current proxy function.
     *
     * Intended to be called by a subclass inside @c execute() to dispatch a signal.
     * The returned @c std::function may be empty if the proxy has not been set yet —
     * always check before invoking: @code if (auto p = proxyEmiter()) p(sig); @endcode
     *
     * @return A const reference to the stored @ref SignalEmiterProxy.
     */
    [[nodiscard]] const SignalEmiterProxy &proxyEmiter() const;

  private:
    SignalEmiterProxy _proxyEmiter; ///< Stored proxy function
};

} // namespace vigine
