#include "windoweventpayload.h"

namespace example::payloads
{
// Default-initialised to the invalid sentinel (PayloadTypeId{} == 0,
// which is engine-bundled "Control" range — emit() against it would
// be rejected by the registry's validation if @c main.cpp forgets to
// fill these in. main.cpp overwrites them with the broker-allocated
// ids before the engine pump starts.
vigine::payload::PayloadTypeId mouseButtonDown{};
vigine::payload::PayloadTypeId keyDown{};
} // namespace example::payloads
