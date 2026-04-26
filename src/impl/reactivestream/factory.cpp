#include "vigine/api/reactivestream/factory.h"

// createReactiveStream is defined in src/impl/reactivestream/reactivestream.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
