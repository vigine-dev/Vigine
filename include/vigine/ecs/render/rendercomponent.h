#pragma once

#include "vigine/base/macros.h"
#include <memory>

namespace vigine
{
namespace graphics
{
class RenderComponent
{
  public:
    RenderComponent();
    ~RenderComponent();
    void render();

    //#
};

BUILD_SMART_PTR(RenderComponent);

} // namespace graphics
} // namespace vigine
