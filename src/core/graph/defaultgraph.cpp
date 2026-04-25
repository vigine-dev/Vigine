#include "core/graph/defaultgraph.h"

// DefaultGraph is a thin final subclass of AbstractGraph. All storage and
// lifecycle behaviour lives on AbstractGraph; this translation unit exists
// only so that the build has an explicit home for the class' out-of-line
// linkage in case the implementation needs to migrate back out of the
// header without churning every consumer.

namespace vigine::core::graph
{
} // namespace vigine::core::graph
