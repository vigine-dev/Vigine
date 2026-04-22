#pragma once

#include "vigine/channelfactory/defaultchannelfactory.h"

// factory.h is a convenience header that re-exports createChannelFactory so
// callers can include a single predictable factory header rather than naming
// the concrete DefaultChannelFactory type. The function is defined in
// src/channelfactory/factory.cpp.
//
// Invariants:
//   - INV-9: createChannelFactory returns std::unique_ptr<IChannelFactory>.
//   - INV-11: no graph types appear here.
