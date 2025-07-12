#include "vigine/ecs/abstractsystem.h"

vigine::SystemName vigine::AbstractSystem::name() { return _name; }

vigine::AbstractSystem::~AbstractSystem() {}

vigine::AbstractSystem::AbstractSystem(const SystemName &name) : EntityBindingHost(), _name{name} {}
