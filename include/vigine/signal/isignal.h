#pragma once

namespace vigine
{
/**
 * @brief Classifies signals by their delivery semantics.
 */
enum class SignalType
{
    System,
    Event,
    Custom
};

/**
 * @brief Base interface for all signals exchanged between tasks.
 *
 * A concrete signal type is passed from an emitter task to the task flow
 * dispatch layer and may be validated by an associated @ref ISignalBinder.
 * Implementations typically carry payload data and report their category
 * through @ref type().
 *
 * @see ISignalEmiter
 * @see ISignalBinder
 */
class ISignal
{
  public:
    virtual ~ISignal() = default;

    /**
     * @brief Returns the category of this signal.
     *
     * The returned value can be used by the engine or by custom wiring logic
     * to distinguish framework-level, event-based, and user-defined signals.
     *
     * @return Signal category for the concrete signal instance.
     */
    [[nodiscard]] virtual SignalType type() const = 0;
};
} // namespace vigine
