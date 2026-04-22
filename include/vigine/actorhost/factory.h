#pragma once

#include "vigine/actorhost/defaultactorhost.h"

// factory.h is a convenience header that re-exports createActorHost so callers
// can include a single predictable factory header rather than naming the
// concrete DefaultActorHost type.  The function is defined in
// src/actorhost/defaultactorhost.cpp.
//
// Invariants:
//   - INV-9:  createActorHost returns std::unique_ptr<IActorHost>.
//   - INV-11: no graph types appear here.
