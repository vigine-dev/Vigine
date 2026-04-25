#include "vigine/impl/engine/engine.h"

namespace vigine::engine
{

// Engine seals the AbstractEngine inheritance chain and carries
// no state of its own. The constructor forwards the config through to
// the abstract base, which does the real construction work (build the
// context aggregator, capture the run-mode hint, default-initialise
// the lifecycle flags). The destructor is defaulted; RAII on the
// AbstractEngine members handles the reverse-order teardown.

Engine::Engine(const EngineConfig &config) : AbstractEngine{config} {}

Engine::~Engine() = default;

} // namespace vigine::engine
