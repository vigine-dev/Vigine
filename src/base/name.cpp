#include "vigine/base/name.h"

vigine::Name::Name(const std::string &name) : _name(name) {}

vigine::Name::Name(std::string &&name) : _name(std::move(name)) {}

vigine::Name::Name(const char *name) : _name(name) {}

vigine::Name &vigine::Name::operator=(std::string &&name) noexcept
{
    _name = std::move(name);
    return *this;
}

bool vigine::Name::operator==(const std::string &other) const { return _name == other; }

bool vigine::Name::operator!=(const std::string &other) const { return _name != other; }

bool vigine::Name::operator<(const std::string &other) const { return _name < other; }

bool vigine::Name::operator<=(const std::string &other) const { return _name <= other; }

bool vigine::Name::operator>(const std::string &other) const { return _name > other; }

bool vigine::Name::operator>=(const std::string &other) const { return _name >= other; }

vigine::Name::operator const std::string &() const { return _name; }

vigine::Name::operator const char *() const { return _name.c_str(); }

const std::string &vigine::Name::str() const { return _name; }

bool vigine::Name::operator>=(const Name &other) const { return _name >= other._name; }

bool vigine::Name::operator>(const Name &other) const { return _name > other._name; }

bool vigine::Name::operator<=(const Name &other) const { return _name <= other._name; }

bool vigine::Name::operator<(const Name &other) const { return _name < other._name; }

bool vigine::Name::operator!=(const Name &other) const { return _name != other._name; }

bool vigine::Name::operator==(const Name &other) const { return _name == other._name; }

vigine::Name &vigine::Name::operator=(const std::string &name)
{
    _name = name;
    return *this;
}

std::string vigine::operator+(const std::string &lhs, const Name &rhs)
{
    return lhs + static_cast<const std::string &>(rhs);
}

std::string vigine::operator+(const char *lhs, const Name &rhs)
{
    return std::string(lhs) + static_cast<const std::string &>(rhs);
}

std::string vigine::operator+(const Name &lhs, const char *rhs)
{
    return static_cast<const std::string &>(lhs) + std::string(rhs);
}

std::string vigine::operator+(const Name &lhs, const std::string &rhs)
{
    return static_cast<const std::string &>(lhs) + rhs;
}
