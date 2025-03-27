#pragma once

#include <string>

namespace vigine {

class Result {
public:

    enum class Code
    {
        Success,
        Error
    };

    Result();
    Result(Code code, const std::string& message = "");

    bool isSuccess() const;
    bool isError() const;
    const std::string& message() const;
    Code code();

  private:
    Code _code;
    std::string _message;
};

} // namespace vigine 
