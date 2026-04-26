#include "setupcubetask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <iostream>

SetupCubeTask::SetupCubeTask() = default;

void SetupCubeTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void SetupCubeTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result SetupCubeTask::run()
{
    std::cout << "Setting up cube geometry..." << std::endl;

    if (!_entityManager)
        return vigine::Result(vigine::Result::Code::Error, "EntityManager is unavailable");

    auto *token = api();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto graphicsResult = token->service(_graphicsServiceId);
    if (!graphicsResult.ok())
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");

    auto *graphicsService =
        dynamic_cast<vigine::ecs::graphics::GraphicsService *>(&graphicsResult.value());
    if (!graphicsService || !graphicsService->renderSystem())
        return vigine::Result(vigine::Result::Code::Error,
                              "Graphics service is unavailable");

    auto *renderSystem = graphicsService->renderSystem();

    auto *cubeEntity = _entityManager->createEntity();
    if (!cubeEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create cube entity");

    _entityManager->addAlias(cubeEntity, "CubeEntity");

    renderSystem->bindEntity(cubeEntity);

    auto *renderComponent = graphicsService->renderComponent();
    if (!renderComponent)
    {
        renderSystem->unbindEntity();
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

    renderSystem->unbindEntity();

    std::cout << "Cube created with " << cubeMesh.getVertexCount() << " vertices and "
              << cubeMesh.getIndexCount() << " indices" << std::endl;

    return vigine::Result();
}
