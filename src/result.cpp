#include "vigine/result.h"

#include <iostream>

namespace vigine
{

Result::Result() : _code(Code::Success) {}

Result::Result(Code code, const std::string &message) : _code(code), _message(message) {}

Result::~Result() {}

Result::Result(const Result &other) : _code(other._code), _message(other._message) {}

Result &Result::operator=(const Result &other)
{
    if (this != &other)
        {
            _code    = other._code;
            _message = other._message;
        }

    return *this;
}

bool Result::isSuccess() const { return _code == Code::Success; }

Result::Code Result::code() { return _code; }

bool Result::isError() const { return _code == Code::Error; }

const std::string &Result::message() const { return _message; }
} // namespace vigine
