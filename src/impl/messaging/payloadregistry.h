#pragma once

#include "api/messaging/payload/abstractpayloadregistry.h"

namespace vigine::payload
{
/**
 * @brief Default concrete implementation of @ref IPayloadRegistry.
 *
 * Closes the inheritance chain on @ref AbstractPayloadRegistry and
 * contributes no additional storage or behaviour of its own; every
 * mechanic lives on the base. The @c final keyword prevents further
 * subclassing so that @ref createPayloadRegistry has one well-defined
 * concrete return type.
 *
 * The constructor calls @ref AbstractPayloadRegistry::bootstrapEngineRanges
 * so that the four engine-bundled ranges are pre-registered by the time
 * the instance is returned to the caller.
 */
class PayloadRegistry final : public AbstractPayloadRegistry
{
  public:
    PayloadRegistry();
    ~PayloadRegistry() override = default;
};

} // namespace vigine::payload
