#pragma once

#include "vigine/base/macros.h"

#include <string>
#include <memory>

namespace vigine
{
class Result
{
  public:
    enum class Code
    {
        Success,
        Error
    };

    Result();
    Result(Code code, const std::string &message = "");
    virtual ~Result();

    Result(const Result &other);
    Result &operator=(const Result &other);

    Result(Result &&other) noexcept            = default;
    Result &operator=(Result &&other) noexcept = default;

    bool isSuccess() const;
    bool isError() const;
    const std::string &message() const;
    Code code();

  protected:
    void setMessage(const std::string &text);

  private:
    Code _code;
    std::string _message;
};

BUILD_SMART_PTR(Result);

} // namespace vigine
