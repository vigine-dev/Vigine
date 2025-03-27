#include "vigine/result.h"
#include <iostream>

namespace vigine {

Result::Result() : _code(Code::Success) {}

Result::Result(Code code, const std::string& message)
    : _code(code), _message(message) {}

bool Result::isSuccess() const {
    return _code == Code::Success;
}

Result::Code Result::code() { return _code; }

bool Result::isError() const {
    return _code == Code::Error;
}

const std::string& Result::message() const {
    return _message;
}
}
