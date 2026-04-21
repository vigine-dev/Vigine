#include "vigine/result.h"

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

Result::Code Result::code() const { return _code; }

// Any non-Success code is treated as an error, so that codes appended
// after the initial `Success` / `Error` pair (e.g. DuplicatePayloadId,
// OutOfRange introduced with the payload-id registry) are reported as
// errors by isError() without requiring every caller to know the full
// code list.
bool Result::isError() const { return _code != Code::Success; }

const std::string &Result::message() const { return _message; }
} // namespace vigine
