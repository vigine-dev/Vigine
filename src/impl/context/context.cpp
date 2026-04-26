#include "vigine/impl/context/context.h"

namespace vigine::context
{

Context::Context(const ContextConfig &config)
    : AbstractContext{config}
{
}

Context::~Context() = default;

} // namespace vigine::context
