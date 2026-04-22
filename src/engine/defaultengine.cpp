#include "vigine/engine/defaultengine.h"

namespace vigine::engine
{

// DefaultEngine seals the AbstractEngine inheritance chain and carries
// no state of its own. The constructor forwards the config through to
// the abstract base, which does the real construction work (build the
// context aggregator, capture the run-mode hint, default-initialise
// the lifecycle flags). The destructor is defaulted; RAII on the
// AbstractEngine members handles the reverse-order teardown.

DefaultEngine::DefaultEngine(const EngineConfig &config) : AbstractEngine{config} {}

DefaultEngine::~DefaultEngine() = default;

} // namespace vigine::engine
