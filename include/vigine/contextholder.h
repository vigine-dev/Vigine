#pragma once

/**
 * @file contextholder.h
 * @brief Mixin that stores a non-owning Context pointer for dependent classes.
 */

namespace vigine
{

class Context;

/**
 * @brief Carries a non-owning Context pointer for its derived classes.
 *
 * Derived classes obtain the Context via context() and may react to
 * binding changes by overriding contextChanged(). The pointer is set
 * externally via setContext(); this class never takes ownership.
 */
class ContextHolder
{
  public:
    ~ContextHolder();
    void setContext(Context *context);

  protected:
    ContextHolder();

    Context *context() const;
    virtual void contextChanged();

  private:
    Context *_context{nullptr};
};
} // namespace vigine
