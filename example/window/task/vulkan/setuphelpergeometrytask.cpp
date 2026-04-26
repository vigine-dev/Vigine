#include "setuphelpergeometrytask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <iostream>

SetupHelperGeometryTask::SetupHelperGeometryTask() = default;

void SetupHelperGeometryTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void SetupHelperGeometryTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result SetupHelperGeometryTask::run()
{
    std::cout << "Setting up helper geometry (pyramid, grid, sun)..." << std::endl;

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
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");

    auto *renderSystem = graphicsService->renderSystem();

    // --- Pyramid Entity ---
    auto *pyramidEntity = _entityManager->createEntity();
    if (!pyramidEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create pyramid entity");

    _entityManager->addAlias(pyramidEntity, "PyramidEntity");

    renderSystem->createComponents(pyramidEntity);
    renderSystem->bindEntity(pyramidEntity);
    auto *pyramidRC = graphicsService->renderComponent();
    if (!pyramidRC)
    {
        renderSystem->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Render component is unavailable for PyramidEntity");
    }

    auto pyramidMesh = vigine::ecs::graphics::MeshComponent::createCube();
    pyramidMesh.setProceduralInShader(true, 18); // Pyramid shader generates 18 vertices
    pyramidRC->setMesh(pyramidMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("pyramid.vert.spv", "pyramid.frag.spv");
        pyramidRC->setShader(shader);
    }

    vigine::ecs::graphics::TransformComponent pyramidTransform;
    pyramidTransform.setPosition({0.0f, 0.0f, 0.0f});
    pyramidTransform.setScale({1.0f, 1.0f, 1.0f});
    pyramidRC->setTransform(pyramidTransform);
    pyramidRC->setPickable(false);

    renderSystem->unbindEntity();

    // --- Grid Entity ---
    auto *gridEntity = _entityManager->createEntity();
    if (!gridEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create grid entity");

    _entityManager->addAlias(gridEntity, "GridEntity");

    renderSystem->createComponents(gridEntity);
    renderSystem->bindEntity(gridEntity);
    auto *gridRC = graphicsService->renderComponent();
    if (!gridRC)
    {
        renderSystem->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Render component is unavailable for GridEntity");
    }

    auto gridMesh = vigine::ecs::graphics::MeshComponent::createCube();
    gridMesh.setProceduralInShader(true, 6); // Grid shader generates 6 vertices
    gridRC->setMesh(gridMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("grid.vert.spv", "grid.frag.spv");
        gridRC->setShader(shader);
    }

    vigine::ecs::graphics::TransformComponent gridTransform;
    gridTransform.setPosition({0.0f, 0.0f, 0.0f});
    gridTransform.setScale({1.0f, 1.0f, 1.0f});
    gridRC->setTransform(gridTransform);
    gridRC->setPickable(false);

    renderSystem->unbindEntity();

    // --- Sun Entity ---
    auto *sunEntity = _entityManager->createEntity();
    if (!sunEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create sun entity");

    _entityManager->addAlias(sunEntity, "SunEntity");

    renderSystem->createComponents(sunEntity);
    renderSystem->bindEntity(sunEntity);
    auto *sunRC = graphicsService->renderComponent();
    if (!sunRC)
    {
        renderSystem->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Render component is unavailable for SunEntity");
    }

    auto sunMesh = vigine::ecs::graphics::MeshComponent::createCube();
    sunMesh.setProceduralInShader(true, 768); // UV-sphere: 16 lon x 8 lat x 2 tri x 3 vert
    sunRC->setMesh(sunMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("sun.vert.spv", "sun.frag.spv");
        sunRC->setShader(shader);
    }

    vigine::ecs::graphics::TransformComponent sunTransform;
    sunTransform.setPosition({0.0f, 0.0f, 0.0f});
    sunTransform.setScale({1.0f, 1.0f, 1.0f});
    sunRC->setTransform(sunTransform);
    sunRC->setPickable(false);

    renderSystem->unbindEntity();

    std::cout << "Helper geometry created: Pyramid (18 verts), Grid (6 verts), Sun (768 verts)"
              << std::endl;

    return vigine::Result();
}
