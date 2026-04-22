#include "context/defaultcontext.h"

namespace vigine::context
{

DefaultContext::DefaultContext(const ContextConfig &config)
    : AbstractContext{config}
{
}

DefaultContext::~DefaultContext() = default;

} // namespace vigine::context
