#include "vigine/impl/ecs/graphics/pipelinecache.h"

#include "vigine/api/ecs/graphics/igraphicsbackend.h"
#include "vigine/impl/ecs/graphics/shadercomponent.h"

#include <functional>
#include <iostream>

namespace vigine
{
namespace ecs
{
namespace graphics
{

uint64_t PipelineCache::computeKey(const ShaderComponent &shader)
{
    std::hash<std::string> hasher;
    uint64_t h = hasher(shader.getVertexShaderPath());
    h ^= hasher(shader.getFragmentShaderPath()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(static_cast<int>(shader.blendMode())) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
    h ^= std::hash<bool>()(shader.depthWrite()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= std::hash<bool>()(shader.depthTest()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= std::hash<int>()(static_cast<int>(shader.topology())) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
    h ^= std::hash<bool>()(shader.instancedRendering()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
    h ^=
        std::hash<bool>()(shader.hasTextureBinding()) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

PipelineHandle PipelineCache::getOrCreate(IGraphicsBackend &backend, ShaderComponent &shader)
{
    const uint64_t key = computeKey(shader);

    auto it            = _cache.find(key);
    if (it != _cache.end())
        return it->second.pipeline;

    // Ensure SPIR-V is loaded
    if (!shader.spirvLoaded())
    {
        if (!shader.loadSpirv())
        {
            std::cerr << "PipelineCache: failed to load SPIR-V for " << shader.getVertexShaderPath()
                      << std::endl;
            return PipelineHandle{0};
        }
    }

    CacheEntry entry;
    entry.vertModule = backend.createShaderModule(shader.vertexSpirv());
    entry.fragModule = backend.createShaderModule(shader.fragmentSpirv());

    PipelineDesc desc;
    desc.vertexShader      = entry.vertModule;
    desc.fragmentShader    = entry.fragModule;
    desc.vertexLayout      = shader.vertexLayout();
    desc.blendMode         = shader.blendMode();
    desc.depthWrite        = shader.depthWrite();
    desc.depthTest         = shader.depthTest();
    desc.topology          = shader.topology();
    desc.hasTextureBinding = shader.hasTextureBinding();

    entry.pipeline         = backend.createPipeline(desc);

    if (!entry.pipeline.isValid())
    {
        std::cerr << "PipelineCache: failed to create pipeline for " << shader.getVertexShaderPath()
                  << std::endl;
        backend.destroyShaderModule(entry.vertModule);
        backend.destroyShaderModule(entry.fragModule);
        return PipelineHandle{0};
    }

    _cache[key] = entry;
    return entry.pipeline;
}

void PipelineCache::invalidate(IGraphicsBackend &backend)
{
    for (auto &pair : _cache)
    {
        backend.destroyPipeline(pair.second.pipeline);
        backend.destroyShaderModule(pair.second.vertModule);
        backend.destroyShaderModule(pair.second.fragModule);
    }
    _cache.clear();
}

} // namespace graphics
} // namespace ecs
} // namespace vigine
