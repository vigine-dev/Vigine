#include "rendercubetask.h"

#include <vigine/context.h>
#include <vigine/ecs/entitymanager.h>

#include <iostream>

RenderCubeTask::RenderCubeTask() {}

void RenderCubeTask::contextChanged()
{
    // No specific services needed for cube rendering
}

vigine::Result RenderCubeTask::execute()
{
    // Update cube rotation
    _rotationAngle += 0.05f; // Rotate by 0.05 radians per frame
    if (_rotationAngle > 2 * 3.14159f)
    {
        _rotationAngle = 0.0f;
    }

    std::cout << "Rendering cube... (rotation: " << _rotationAngle << " radians)" << std::endl;

    return vigine::Result();
}
