#include "vigine/graph/factory.h"

#include <memory>

#include "graph/defaultgraph.h"

namespace vigine::graph
{
// Factory returns the default adjacency-list implementation behind the
// pure-virtual IGraph interface. shared_ptr so that several wrappers can
// share the same substrate when that is the right architectural choice.

std::shared_ptr<IGraph> createGraph()
{
    return std::make_shared<DefaultGraph>();
}

} // namespace vigine::graph
