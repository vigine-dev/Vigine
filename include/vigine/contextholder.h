#pragma once

namespace vigine
{

class Context;

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
