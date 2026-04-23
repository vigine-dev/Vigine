#pragma once

#include "vigine/ecs/platform/inputmap.h"

namespace vigine
{
namespace platform
{

class InputProfileComponent
{
  public:
    virtual ~InputProfileComponent() = default;

    virtual const char* name() const = 0;
    virtual const char* description() const = 0;

    const InputMap& inputMap() const { return _map; }
    InputMap& inputMap() { return _map; }

  protected:
    InputProfileComponent();

    virtual void populateBindings() = 0;
    void addCommonBindings();

    InputMap _map;
};

} // namespace platform
} // namespace vigine
