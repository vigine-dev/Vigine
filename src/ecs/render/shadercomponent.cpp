#include "vigine/ecs/render/shadercomponent.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
std::vector<char> loadBinaryFile(const std::vector<std::string> &candidates)
{
    for (const auto &path : candidates)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            continue;

        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
            continue;

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

    return {};
}

std::vector<std::string> shaderCandidates(const std::string &name)
{
    return {"build/bin/shaders/" + name, "bin/shaders/" + name, "shaders/" + name};
}
} // namespace

namespace vigine
{
namespace graphics
{

bool ShaderComponent::loadSpirv()
{
    if (_vertexShaderPath.empty() || _fragmentShaderPath.empty())
        return false;

    _vertexSpirv = loadBinaryFile(shaderCandidates(_vertexShaderPath));
    if (_vertexSpirv.empty())
    {
        std::cerr << "ShaderComponent: cannot load vertex shader " << _vertexShaderPath
                  << std::endl;
        return false;
    }

    _fragmentSpirv = loadBinaryFile(shaderCandidates(_fragmentShaderPath));
    if (_fragmentSpirv.empty())
    {
        std::cerr << "ShaderComponent: cannot load fragment shader " << _fragmentShaderPath
                  << std::endl;
        _vertexSpirv.clear();
        return false;
    }

    return true;
}

} // namespace graphics
} // namespace vigine
