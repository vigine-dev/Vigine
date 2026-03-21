#pragma once

#include "vigine/base/macros.h"

#include <memory>
#include <string>

namespace vigine
{
namespace graphics
{

class ShaderComponent
{
  public:
    ShaderComponent() = default;

    void setVertexShaderPath(const std::string &path) { _vertexShaderPath = path; }
    void setFragmentShaderPath(const std::string &path) { _fragmentShaderPath = path; }

    const std::string &getVertexShaderPath() const { return _vertexShaderPath; }
    const std::string &getFragmentShaderPath() const { return _fragmentShaderPath; }

  private:
    std::string _vertexShaderPath;
    std::string _fragmentShaderPath;
};

BUILD_SMART_PTR(ShaderComponent);

} // namespace graphics
} // namespace vigine
