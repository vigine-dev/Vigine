#pragma once

#include "meshcomponent.h"
#include "transformcomponent.h"

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

    void setMesh(const MeshComponent &mesh);
    void setTransform(const TransformComponent &transform);

    MeshComponent &getMesh() { return _mesh; }
    TransformComponent &getTransform() { return _transform; }

  private:
    MeshComponent _mesh;
    TransformComponent _transform;
};

BUILD_SMART_PTR(RenderComponent);

} // namespace graphics
} // namespace vigine
