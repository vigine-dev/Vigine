#include "setuptexturedplanestask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/texturecomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <cmath>
#include <iostream>

void SetupTexturedPlanesTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void SetupTexturedPlanesTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result SetupTexturedPlanesTask::run()
{
    std::cout << "Setting up textured planes..." << std::endl;

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

    struct PlaneConfig
    {
        std::string entityName;
        std::string textureEntityName;
        glm::vec3 position;
    };

    // Discover all loaded texture entities (TextureEntity_0, TextureEntity_1, ...)
    size_t textureCount = 0;
    while (_entityManager->getEntityByAlias("TextureEntity_" + std::to_string(textureCount)))
        ++textureCount;

    std::cout << "Found " << textureCount << " texture entities" << std::endl;

    if (textureCount == 0)
    {
        std::cerr << "No texture entities found - skipping plane setup." << std::endl;
        return vigine::Result();
    }

    // Compute gallery layout: semicircle arc, radius=8, centered at far end XZ=(0,-12)
    constexpr float galleryY   = 2.2f;
    constexpr float centerX    = 0.0f;
    constexpr float centerZ    = -12.0f;
    constexpr float radius     = 8.0f;
    constexpr float halfArcRad = glm::radians(90.0f);

    std::vector<PlaneConfig> planes;
    planes.reserve(textureCount);
    for (size_t i = 0; i < textureCount; ++i)
    {
        const float t     = (textureCount > 1)
                                ? static_cast<float>(i) / static_cast<float>(textureCount - 1)
                                : 0.5f;
        const float angle = -halfArcRad + t * 2.0f * halfArcRad;
        const float x     = centerX + radius * std::sin(angle);
        const float z     = centerZ - radius * std::cos(angle);
        planes.push_back({
            "TexturedPlane" + std::to_string(i),
            "TextureEntity_" + std::to_string(i),
            {x, galleryY, z}
        });
    }

    for (const auto &config : planes)
    {
        // Find texture entity
        auto *textureEntity = _entityManager->getEntityByAlias(config.textureEntityName);
        if (!textureEntity)
        {
            std::cerr << "Texture entity not found: " << config.textureEntityName << std::endl;
            continue;
        }

        // Create plane entity
        auto *planeEntity = _entityManager->createEntity();
        if (!planeEntity)
        {
            std::cerr << "Failed to create plane entity: " << config.entityName << std::endl;
            continue;
        }

        _entityManager->addAlias(planeEntity, config.entityName);
        renderSystem->bindEntity(planeEntity);

        auto *renderComponent = graphicsService->renderComponent();
        if (!renderComponent)
        {
            renderSystem->unbindEntity();
            std::cerr << "Render component is unavailable for " << config.entityName << std::endl;
            continue;
        }

        // Create plane mesh (procedural in shader - 6 vertices for 2 triangles)
        auto planeMesh = vigine::ecs::graphics::MeshComponent();
        planeMesh.setProceduralInShader(true, 6);

        renderComponent->setMesh(planeMesh);

        // Set textured plane shader
        vigine::ecs::graphics::ShaderComponent shader("textured_plane.vert.spv",
                                                 "textured_plane.frag.spv");
        shader.setHasTextureBinding(true);
        renderComponent->setShader(shader);

        // Link texture to render component; read dimensions for aspect-correct scale.
        renderSystem->bindEntity(textureEntity);
        auto *textureComponent = graphicsService->textureComponent();
        glm::vec2 planeScale{2.0f, 2.0f};
        if (textureComponent && textureComponent->hasGpuTexture())
        {
            // Associate texture with this render component
            renderComponent->setTextureHandle(textureComponent->textureHandle());
            std::cout << "Linked texture to plane: " << config.entityName << std::endl;

            // Compute aspect-correct scale: longest side = 2 world units.
            const float w = static_cast<float>(textureComponent->width());
            const float h = static_cast<float>(textureComponent->height());
            if (w > 0.0f && h > 0.0f)
            {
                if (w >= h)
                    planeScale = glm::vec2{2.0f, 2.0f * (h / w)};
                else
                    planeScale = glm::vec2{2.0f * (w / h), 2.0f};
            }
        } else
        {
            std::cerr << "Texture not ready for " << config.entityName << std::endl;
        }
        renderSystem->unbindEntity();

        // Set back to plane entity
        renderSystem->bindEntity(planeEntity);

        // Set transform
        vigine::ecs::graphics::TransformComponent transform;
        transform.setPosition(config.position);
        transform.setScale({planeScale.x, planeScale.y, 1.0f});
        transform.setBillboard(true);

        renderComponent->setTransform(transform);

        renderSystem->unbindEntity();

        std::cout << "Created textured plane: " << config.entityName << " at position ("
                  << config.position.x << ", " << config.position.y << ", " << config.position.z
                  << ")" << std::endl;
    }

    std::cout << "Textured planes setup complete." << std::endl;
    return vigine::Result();
}
