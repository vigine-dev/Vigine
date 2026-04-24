#pragma once

/**
 * @file factory.h
 * @brief Convenience re-export header for @c createRequestBus.
 *
 * Lets callers include a single predictable header to obtain an
 * owning @c std::unique_ptr<IRequestBus> without naming the concrete
 * @c DefaultRequestBus type. The function itself is defined in
 * @c src/requestbus/defaultrequestbus.cpp.
 */

#include "vigine/requestbus/defaultrequestbus.h"

// factory.h is a convenience header that re-exports createRequestBus so
// callers can include a single predictable factory header rather than
// naming the concrete DefaultRequestBus type. The function is defined in
// src/requestbus/defaultrequestbus.cpp.
//
// Invariants:
//   - INV-9: createRequestBus returns std::unique_ptr<IRequestBus>.
//   - INV-11: no graph types appear here.
