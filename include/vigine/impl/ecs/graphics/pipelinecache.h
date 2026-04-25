#pragma once

/**
 * @file pipelinecache.h
 * @brief Content-addressed cache of graphics pipelines keyed by shader state.
 */

#include "graphicshandles.h"

#include "vigine/base/macros.h"

#include <cstdint>
#include <memory>
#include <unordered_map>


namespace vigine
{
namespace ecs
{
namespace graphics
{

class IGraphicsBackend;
class ShaderComponent;

/**
 * @brief Caches created pipelines to avoid redundant backend creation.
 *
 * getOrCreate() returns the pipeline for the given ShaderComponent,
 * allocating a new one from the IGraphicsBackend and caching it on
 * first miss. Keyed on a hash computed from the ShaderComponent's
 * pipeline-relevant state. invalidate() drops every cached pipeline
 * (e.g. after device loss).
 */
class PipelineCache
{
  public:
    PipelineCache() = default;

    PipelineHandle getOrCreate(IGraphicsBackend &backend, ShaderComponent &shader);
    void invalidate(IGraphicsBackend &backend);

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

using PipelineCacheUPtr = std::unique_ptr<PipelineCache>;
using PipelineCacheSPtr = std::shared_ptr<PipelineCache>;

} // namespace graphics
} // namespace ecs
} // namespace vigine
