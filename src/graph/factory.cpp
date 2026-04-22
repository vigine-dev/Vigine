#include "vigine/graph/factory.h"

#include <memory>

#include "graph/defaultgraph.h"

namespace vigine::graph
{
// Factory returns the default adjacency-list implementation behind the
// pure-virtual IGraph interface. unique_ptr: the default hand-off is sole
// ownership; a wrapper that legitimately needs to share the substrate
// can promote with `std::shared_ptr<IGraph>(std::move(g))`. The reverse
// direction (shared -> unique) is impossible, which is why the factory
// returns the tighter type by default.

std::unique_ptr<IGraph> createGraph()
{
    return std::make_unique<DefaultGraph>();
}

} // namespace vigine::graph
