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
namespace graphics
{

class GraphicsBackend;
class ShaderComponent;

/**
 * @brief Caches created pipelines to avoid redundant backend creation.
 *
 * getOrCreate() returns the pipeline for the given ShaderComponent,
 * allocating a new one from the GraphicsBackend and caching it on
 * first miss. Keyed on a hash computed from the ShaderComponent's
 * pipeline-relevant state. invalidate() drops every cached pipeline
 * (e.g. after device loss).
 */
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
