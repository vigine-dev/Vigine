#pragma once

#include "graphicshandles.h"

#include "vigine/base/macros.h"

#include <cstdint>
#include <memory>
#include <unordered_map>


namespace vigine
{
namespace graphics
{

class GraphicsBackend;
class ShaderComponent;

class PipelineCache
{
  public:
    PipelineCache() = default;

    PipelineHandle getOrCreate(GraphicsBackend &backend, ShaderComponent &shader);
    void invalidate(GraphicsBackend &backend);

  private:
    static uint64_t computeKey(const ShaderComponent &shader);

    struct CacheEntry
    {
        PipelineHandle pipeline;
        ShaderModuleHandle vertModule;
        ShaderModuleHandle fragModule;
    };

    std::unordered_map<uint64_t, CacheEntry> _cache;
};

BUILD_SMART_PTR(PipelineCache);

} // namespace graphics
} // namespace vigine
