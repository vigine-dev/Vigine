#pragma once

#include "vigine/topicbus/defaulttopicbus.h"

// factory.h is a convenience header that re-exports createTopicBus so callers
// can include a single predictable factory header rather than naming the
// concrete DefaultTopicBus type. The function is defined in
// src/topicbus/factory.cpp.
//
// Invariants:
//   - INV-9: createTopicBus returns std::unique_ptr<ITopicBus>.
//   - INV-11: no graph types appear here.
