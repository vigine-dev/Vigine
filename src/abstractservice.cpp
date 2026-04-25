#include "vigine/abstractservice.h"

#include "vigine/context.h"

vigine::AbstractService::~AbstractService() {}

vigine::Name vigine::AbstractService::name() { return _name; }

vigine::AbstractService::AbstractService(const Name &name) : _name{name} {}

void vigine::AbstractService::setContext(Context *context)
{
    _context = context;
    contextChanged();
}

vigine::Context *vigine::AbstractService::context() const { return _context; }

void vigine::AbstractService::contextChanged() {}

void vigine::AbstractService::bindEntity(Entity *entity)
{
    _entity = entity;
    entityBound();
}

void vigine::AbstractService::unbindEntity()
{
    _entity = nullptr;
    entityUnbound();
}

vigine::Entity *vigine::AbstractService::getBoundEntity() const { return _entity; }

void vigine::AbstractService::entityBound() {}

void vigine::AbstractService::entityUnbound() {}
