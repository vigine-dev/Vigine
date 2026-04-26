#include "vigine/api/actorhost/factory.h"

// createActorHost is defined in src/impl/actorhost/actorhost.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
