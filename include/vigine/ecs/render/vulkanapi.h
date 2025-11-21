#pragma once

#include "vigine/base/macros.h"
#include <memory>

namespace vigine
{
namespace graphics
{

class VulkanAPI
{
  public:
    VulkanAPI();
    ~VulkanAPI();
};

BUILD_SMART_PTR(VulkanAPI);

} // namespace graphics
} // namespace vigine
