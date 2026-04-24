#pragma once

/**
 * @file factory.h
 * @brief Convenience re-export header for @c createChannelFactory.
 *
 * Lets callers include a single predictable header to obtain an
 * owning @c std::unique_ptr<IChannelFactory> without naming the
 * concrete @c DefaultChannelFactory type. The function itself is
 * defined in @c src/channelfactory/factory.cpp.
 */

#include "vigine/channelfactory/defaultchannelfactory.h"

// factory.h is a convenience header that re-exports createChannelFactory so
// callers can include a single predictable factory header rather than naming
// the concrete DefaultChannelFactory type. The function is defined in
// src/channelfactory/factory.cpp.
//
// Invariants:
//   - INV-9: createChannelFactory returns std::unique_ptr<IChannelFactory>.
//   - INV-11: no graph types appear here.
