#include "imageloader.h"
#include "loadtexturestask.h"

#include <vigine/context.h>
#include <vigine/ecs/entitymanager.h>
#include <vigine/ecs/render/rendersystem.h>
#include <vigine/ecs/render/texturecomponent.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

void LoadTexturesTask::contextChanged()
{
    if (!context())
    {
        _graphicsService = nullptr;
        return;
    }

    _graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));

    if (!_graphicsService)
    {
        _graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
            context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::New));
    }
}

vigine::Result LoadTexturesTask::execute()
{
    if (!_graphicsService)
    {
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");
    }

    auto *entityManager = context()->entityManager();

    // Try several candidate cwd-relative paths so the example works whether
    // it is launched from the engine root, the exe directory (the build
    // system post-build-copies `resource/` next to the exe), `build/bin/`,
    // or `build/bin/Release/`. The first candidate that resolves to an
    // existing directory wins. Matches the candidate-list pattern already
    // used by SetupTextTask and SetupTextEditTask for `assets/fonts/`.
    static constexpr const char *kImgCandidates[] = {
        "resource/img",          // engine root, or exe dir (post-build copy)
        "../resource/img",       // build/bin
        "../../resource/img",    // build/bin/Release
        "../../../resource/img", // deeper config layouts
    };
    std::filesystem::path imgDir;
    for (const auto *candidate : kImgCandidates)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec))
        {
            imgDir = candidate;
            break;
        }
    }
    if (imgDir.empty())
    {
        return vigine::Result(vigine::Result::Code::Error,
                              "Image directory not found: resource/img");
    }

    std::cout << "Loading textures from " << imgDir.string() << "..." << std::endl;

    std::vector<std::string> imagePaths;
    for (const auto &entry : std::filesystem::directory_iterator(imgDir))
    {
        if (!entry.is_regular_file())
            continue;
        auto ext = entry.path().extension().string();
        // Case-insensitive extension check
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png")
            imagePaths.push_back(entry.path().string());
    }
    std::sort(imagePaths.begin(), imagePaths.end());

    if (imagePaths.empty())
    {
        return vigine::Result(vigine::Result::Code::Error, "No image files found in resource/img");
    }

    size_t loadedIndex = 0;
    for (size_t i = 0; i < imagePaths.size(); ++i)
    {
        const auto &imagePath = imagePaths[i];

        // Check if file exists
        if (!std::filesystem::exists(imagePath))
        {
            std::cerr << "Image file not found: " << imagePath << std::endl;
            continue;
        }

        // Load image data
        ImageData imageData = ImageLoader::loadImage(imagePath, 4); // Force RGBA

        if (!imageData.isValid())
        {
            std::cerr << "Failed to load texture: " << imagePath << std::endl;
            continue;
        }

        // Create entity for this texture
        auto *textureEntity = entityManager->createEntity();
        if (!textureEntity)
        {
            std::cerr << "Failed to create entity for texture: " << imagePath << std::endl;
            continue;
        }

        // Use sequential index based on successful loads so entity names are always 0,1,2...
        std::string entityName = "TextureEntity_" + std::to_string(loadedIndex++);
        entityManager->addAlias(textureEntity, entityName);

        // Create texture component for this entity
        auto *renderSystem = _graphicsService->renderSystem();
        if (renderSystem)
        {
            renderSystem->createTextureComponent(textureEntity);
        }

        _graphicsService->bindEntity(textureEntity);

        // Get texture component (RenderSystem should manage this via GraphicsService)
        auto *textureComponent = _graphicsService->textureComponent();
        if (!textureComponent)
        {
            _graphicsService->unbindEntity();
            std::cerr << "Texture component is unavailable for " << entityName << std::endl;
            continue;
        }

        // Set texture data
        textureComponent->setDimensions(imageData.width, imageData.height);
        textureComponent->setFormat(vigine::graphics::TextureFormat::RGBA8_SRGB);
        textureComponent->setPixelData(imageData.pixels);

        // Set filtering and wrapping
        textureComponent->setFilterMode(vigine::graphics::TextureFilter::Linear,
                                        vigine::graphics::TextureFilter::Linear);
        textureComponent->setWrapMode(vigine::graphics::TextureWrapMode::Repeat,
                                      vigine::graphics::TextureWrapMode::Repeat);

        _graphicsService->unbindEntity();

        // Upload texture to GPU
        renderSystem->uploadTextureToGpu(textureEntity);

        std::cout << "Loaded and uploaded texture " << (loadedIndex - 1) << ": " << imagePath
                  << " (" << imageData.width << "x" << imageData.height << ")" << std::endl;
    }

    std::cout << "Texture loading complete." << std::endl;
    return vigine::Result();
}
