#include "setupcubetask.h"

#include <vigine/context.h>
#include <vigine/ecs/entitymanager.h>
#include <vigine/ecs/render/meshcomponent.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/transformcomponent.h>

#include <iostream>


SetupCubeTask::SetupCubeTask() {}

void SetupCubeTask::contextChanged()
{
    // No specific services needed for cube setup
}

vigine::Result SetupCubeTask::execute()
{
    std::cout << "Setting up cube geometry..." << std::endl;

    auto *entityManager = context()->entityManager();
    auto *cubeEntity    = entityManager->createEntity();

    if (!cubeEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create cube entity");

    entityManager->addAlias(cubeEntity, "CubeEntity");

    // Create a cube mesh with colored faces
    auto cubeMesh = vigine::graphics::MeshComponent::createCube();

    // Create a render component and add the mesh
    vigine::graphics::RenderComponent renderComponent;
    renderComponent.setMesh(cubeMesh);

    // Set initial transform (center at origin, no rotation)
    vigine::graphics::TransformComponent transform;
    transform.setPosition({0.0f, 0.0f, 0.0f});
    transform.setScale({1.0f, 1.0f, 1.0f});
    renderComponent.setTransform(transform);

    std::cout << "Cube created with " << cubeMesh.getVertexCount() << " vertices and "
              << cubeMesh.getIndexCount() << " indices" << std::endl;

    return vigine::Result();
}
