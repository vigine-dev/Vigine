#include "vigine/entitybindinghost.h"

vigine::EntityBindingHost::~EntityBindingHost() {}

void vigine::EntityBindingHost::bindEntity(Entity *entity)
{
    _entity = entity;
    entityBound();
}

void vigine::EntityBindingHost::unbindEntity()
{
    _entity = nullptr;
    entityUnbound();
}

vigine::EntityBindingHost::EntityBindingHost() {}

void vigine::EntityBindingHost::entityBound() {}

void vigine::EntityBindingHost::entityUnbound() {}

vigine::Entity *vigine::EntityBindingHost::getBoundEntity() const { return _entity; }
