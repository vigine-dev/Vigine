#include "vigine/base/password.h"

vigine::Password::Password(const std::string &pwd) : _value(pwd) {}

vigine::Password::Password(std::string &&pwd) : _value(std::move(pwd)) {}

vigine::Password &vigine::Password::operator=(std::string &&pwd) noexcept
{
    _value = std::move(pwd);
    return *this;
}

vigine::Password::operator const std::string &() const { return _value; }

bool vigine::Password::operator==(const Password &other) const { return _value == other._value; }

bool vigine::Password::operator!=(const Password &other) const { return _value != other._value; }

const std::string &vigine::Password::str() const { return _value; }

void vigine::Password::setValidationRule(const std::string &regexPattern)
{
    _validationRule = std::regex(regexPattern);
}

bool vigine::Password::isValid() const
{
    if (_validationRule.has_value() && !std::regex_match(_value, *_validationRule))
        return false;

    return true;
}

vigine::Password &vigine::Password::operator=(const std::string &pwd)
{
    _value = pwd;
    return *this;
}
