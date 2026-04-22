#pragma once

#include "vigine/service/abstractservice.h"

namespace vigine::service
{
/**
 * @brief Minimal concrete service that seals the wrapper recipe.
 *
 * @ref DefaultService exists so @ref createService can return a
 * real owning @c std::unique_ptr<IService>. It carries no
 * domain-specific behaviour; its lifecycle methods fall through to
 * @ref AbstractService and its @ref id reports the default-
 * constructed sentinel until the container stamps it during
 * registration.
 *
 * The class is @c final to close the inheritance chain for this
 * leaf; follow-up leaves that ship real services (graphics, platform,
 * network, database, timer) define their own concrete classes and
 * their own factory entry points and do not derive from
 * @ref DefaultService.
 */
class DefaultService final : public AbstractService
{
  public:
    DefaultService();
    ~DefaultService() override;

    DefaultService(const DefaultService &)            = delete;
    DefaultService &operator=(const DefaultService &) = delete;
    DefaultService(DefaultService &&)                 = delete;
    DefaultService &operator=(DefaultService &&)      = delete;
};

} // namespace vigine::service
