#include "vigine/ecs/factory.h"

#include <memory>

#include "ecs/defaultecs.h"

namespace vigine::ecs
{

std::unique_ptr<IECS> createECS()
{
    // The factory constructs the default concrete closer over
    // AbstractECS. The internal entity world is allocated eagerly by
    // the base class constructor, so the returned ECS is immediately
    // ready to accept entities and components.
    return std::make_unique<DefaultECS>();
}

} // namespace vigine::ecs
