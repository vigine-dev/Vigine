#include "vigine/api/requestbus/factory.h"

// createRequestBus is defined in src/impl/requestbus/requestbus.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
