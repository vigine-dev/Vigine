#include "vigine/api/topicbus/factory.h"

// createTopicBus is defined in src/impl/topicbus/topicbus.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
