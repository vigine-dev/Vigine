#pragma once

#include "vigine/topicbus/defaulttopicbus.h"

// factory.h is a convenience header that re-exports createTopicBus so
// callers can include a single predictable factory header rather than
// naming the concrete DefaultTopicBus type. The function definition
// lives in src/topicbus/defaulttopicbus.cpp alongside the concrete
// class; src/topicbus/factory.cpp holds only a pointer-comment
// noting that location.
//
// Invariants:
//   - createTopicBus returns std::unique_ptr<ITopicBus>.
//   - No graph types appear here.
