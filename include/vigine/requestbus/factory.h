#pragma once

#include "vigine/requestbus/defaultrequestbus.h"

// factory.h is a convenience header that re-exports createRequestBus so
// callers can include a single predictable factory header rather than
// naming the concrete DefaultRequestBus type. The function is defined in
// src/requestbus/defaultrequestbus.cpp.
//
// Invariants:
//   - INV-9: createRequestBus returns std::unique_ptr<IRequestBus>.
//   - INV-11: no graph types appear here.
