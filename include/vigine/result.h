#pragma once

/**
 * @file result.h
 * @brief Status / error return type used across the engine API surface.
 */

#include <memory>
#include <string>

namespace vigine
{
/**
 * @brief Carries a status Code and an optional human-readable message.
 *
 * Produced by task / state / service / context operations that can
 * fail. Code values are append-only so callers that persist or
 * serialise a Result remain compatible across engine versions.
 */
class Result
{
  public:
    enum class Code
    {
        Success,
        Error,
        // Appended for the payload-id registry (R.1.3.1). Append-only:
        // existing values above keep their numeric positions so that
        // callers that persist or serialise Result codes are unaffected.
        DuplicatePayloadId,
        OutOfRange,
        // Appended for the messaging control-block additions (R.3.1.1).
        // Append-only; existing numeric values above are frozen.
        // SubscriptionExpired   -- reported when a caller hands the bus a
        //                          connection id whose slot has been
        //                          released (target-first destruction or
        //                          an explicit unregister).
        // InvalidMessageTarget  -- reported when the target pointer is
        //                          null or the registered target has
        //                          already been torn down.
        SubscriptionExpired,
        InvalidMessageTarget,
        // Appended for the context aggregator (R.4.5). Append-only;
        // existing numeric values above are frozen.
        // TopologyFrozen -- reported when a caller invokes a mutating
        //                   IContext method (createMessageBus,
        //                   registerService) after the engine has
        //                   frozen the context topology. Mutation
        //                   is blocked; reads remain available.
        TopologyFrozen
    };

    Result();
    Result(Code code, const std::string &message = "");
    virtual ~Result();

    Result(const Result &other);
    Result &operator=(const Result &other);

    Result(Result &&other) noexcept            = default;
    Result &operator=(Result &&other) noexcept = default;

    [[nodiscard]] bool isSuccess() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] const std::string &message() const;
    [[nodiscard]] Code code() const;

  protected:
    void setMessage(const std::string &text);

  private:
    Code _code;
    std::string _message;
};

using ResultUPtr = std::unique_ptr<Result>;
using ResultSPtr = std::shared_ptr<Result>;

} // namespace vigine
