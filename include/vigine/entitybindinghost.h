#pragma once

/**
 * @file entitybindinghost.h
 * @brief Mixin that binds a class to a single Entity at a time.
 */

namespace vigine
{
class Entity;

/**
 * @brief Tracks a non-owning Entity pointer with bind / unbind hooks.
 *
 * Derived classes are notified via entityBound() / entityUnbound() when
 * the associated Entity changes. Used by services and systems that
 * operate against one Entity at a time.
 */
class EntityBindingHost
{
  public:
    virtual ~EntityBindingHost();

    void bindEntity(Entity *entity);
    void unbindEntity();
    Entity *getBoundEntity() const;

  protected:
    EntityBindingHost();

    virtual void entityBound();
    virtual void entityUnbound();

  private:
    Entity *_entity{nullptr};
};
} // namespace vigine
