#pragma once

#include <memory>

#include "vigine/graph/igraph.h"

namespace vigine::graph
{
/**
 * @brief Constructs the default in-memory @ref IGraph implementation.
 *
 * Returns a new graph instance wrapped in a `std::shared_ptr` so that
 * multiple wrappers can share the same substrate when required. The
 * factory is deliberately non-templated: every implementation is
 * selected at build time by the engine library.
 */
[[nodiscard]] std::shared_ptr<IGraph> createGraph();

} // namespace vigine::graph
