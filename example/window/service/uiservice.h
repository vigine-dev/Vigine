#pragma once

#include <vigine/abstractservice.h>

class UISystem;

class UIService : public vigine::AbstractService
{
  public:
    explicit UIService(const vigine::Name& name);
    ~UIService() override;

    [[nodiscard]] vigine::ServiceId id() const override { return "UI"; }

    UISystem* uiSystem() const;

  protected:
    void contextChanged() override;

  private:
    UISystem* _uiSystem{nullptr};
};
