#include "setupcubetask.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/property.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <iostream>

SetupCubeTask::SetupCubeTask() {}

void SetupCubeTask::contextChanged()
{
    if (!context())
    {
        _graphicsService = nullptr;
        return;
    }

    _graphicsService = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));

    if (!_graphicsService)
    {
        _graphicsService = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
            context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::New));
    }
}

vigine::Result SetupCubeTask::execute()
{
    std::cout << "Setting up cube geometry..." << std::endl;

    if (!_graphicsService)
    {
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");
    }

    auto *entityManager = context()->entityManager();
    auto *cubeEntity    = entityManager->createEntity();

    if (!cubeEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create cube entity");

    entityManager->addAlias(cubeEntity, "CubeEntity");

    _graphicsService->bindEntity(cubeEntity);

    auto *renderComponent = _graphicsService->renderComponent();
    if (!renderComponent)
    {
        _graphicsService->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Render component is unavailable for CubeEntity");
    }

    // Create a cube mesh with colored faces
    auto cubeMesh = vigine::ecs::graphics::MeshComponent::createCube();
    // Cube shader generates geometry procedurally (36 vertices: 6 faces × 2 triangles × 3 vertices)
    cubeMesh.setProceduralInShader(true, 36);

    // Configure render component managed by RenderSystem
    renderComponent->setMesh(cubeMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("cube.vert.spv", "cube.frag.spv");
        renderComponent->setShader(shader);
    }

    // Set initial transform (center at origin, no rotation)
    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, 0.0f, 0.0f});
    transform.setScale({1.0f, 1.0f, 1.0f});
    renderComponent->setTransform(transform);

    _graphicsService->unbindEntity();

    std::cout << "Cube created with " << cubeMesh.getVertexCount() << " vertices and "
              << cubeMesh.getIndexCount() << " indices" << std::endl;

    return vigine::Result();
}
