#pragma once

#include "vigine/base/macros.h"

#include <memory>
#include <string>

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

    [[nodiscard]] bool isSuccess() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] const std::string &message() const;
    [[nodiscard]] Code code() const;

  protected:
    void setMessage(const std::string &text);

  private:
    Code _code;
    std::string _message;
};

BUILD_SMART_PTR(Result);

} // namespace vigine
