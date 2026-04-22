#pragma once

#include <memory>

#include "vigine/graph/igraph.h"

namespace vigine::graph
{
/**
 * @brief Constructs the default in-memory @ref IGraph implementation.
 *
 * Returns a new graph instance that the caller owns uniquely. Promoting
 * to `std::shared_ptr<IGraph>` is a single move when shared lifetime is
 * genuinely required (`std::shared_ptr<IGraph>(std::move(g))`); the
 * reverse promotion is not possible, so the factory hands back the
 * tighter ownership by default. The factory is deliberately
 * non-templated: every implementation is selected at build time by the
 * engine library.
 */
[[nodiscard]] std::unique_ptr<IGraph> createGraph();

} // namespace vigine::graph
