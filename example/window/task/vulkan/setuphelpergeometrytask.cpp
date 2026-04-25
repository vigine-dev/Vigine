#include "setuphelpergeometrytask.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/property.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <iostream>

SetupHelperGeometryTask::SetupHelperGeometryTask() {}

void SetupHelperGeometryTask::contextChanged()
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

vigine::Result SetupHelperGeometryTask::execute()
{
    std::cout << "Setting up helper geometry (pyramid, grid, sun)..." << std::endl;

    if (!_graphicsService)
    {
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");
    }

    auto *entityManager = context()->entityManager();

    // --- Pyramid Entity ---
    auto *pyramidEntity = entityManager->createEntity();
    if (!pyramidEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create pyramid entity");

    entityManager->addAlias(pyramidEntity, "PyramidEntity");

    _graphicsService->bindEntity(pyramidEntity);
    auto *pyramidRC = _graphicsService->renderComponent();
    if (!pyramidRC)
    {
        _graphicsService->unbindEntity();
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

    _graphicsService->unbindEntity();

    // --- Grid Entity ---
    auto *gridEntity = entityManager->createEntity();
    if (!gridEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create grid entity");

    entityManager->addAlias(gridEntity, "GridEntity");

    _graphicsService->bindEntity(gridEntity);
    auto *gridRC = _graphicsService->renderComponent();
    if (!gridRC)
    {
        _graphicsService->unbindEntity();
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

    _graphicsService->unbindEntity();

    // --- Sun Entity ---
    auto *sunEntity = entityManager->createEntity();
    if (!sunEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create sun entity");

    entityManager->addAlias(sunEntity, "SunEntity");

    _graphicsService->bindEntity(sunEntity);
    auto *sunRC = _graphicsService->renderComponent();
    if (!sunRC)
    {
        _graphicsService->unbindEntity();
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

    _graphicsService->unbindEntity();

    std::cout << "Helper geometry created: Pyramid (18 verts), Grid (6 verts), Sun (768 verts)"
              << std::endl;

    return vigine::Result();
}
