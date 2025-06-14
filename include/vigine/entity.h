#pragma once

#include "ecs/abstractentity.h"

namespace vigine {

class Entity : public AbstractEntity {
public:
    virtual ~Entity() = default;
    virtual void update();
};

} // namespace vigine
