#pragma once

#include <string>
#include <utility>

namespace vigine
{
class Name
{
  public:
    // Constructors
    Name() = default;
    Name(const std::string &name);
    Name(std::string &&name);
    Name(const char *name);

    // Copy/move
    Name(const Name &)                = default;
    Name(Name &&) noexcept            = default;
    Name &operator=(const Name &)     = default;
    Name &operator=(Name &&) noexcept = default;

    // assign std::string
    Name &operator=(const std::string &name);

    Name &operator=(std::string &&name) noexcept;

    // equals operator
    bool operator==(const Name &other) const;
    bool operator!=(const Name &other) const;
    bool operator<(const Name &other) const;
    bool operator<=(const Name &other) const;
    bool operator>(const Name &other) const;
    bool operator>=(const Name &other) const;

    // equal with std::string
    bool operator==(const std::string &other) const;
    bool operator!=(const std::string &other) const;
    bool operator<(const std::string &other) const;
    bool operator<=(const std::string &other) const;
    bool operator>(const std::string &other) const;
    bool operator>=(const std::string &other) const;

    // cast to std::string
    operator const std::string &() const;
    operator const char *() const;

    // other
    const std::string &str() const;

  private:
    std::string _name;
};

std::string operator+(const std::string &lhs, const Name &rhs);
std::string operator+(const Name &lhs, const std::string &rhs);
std::string operator+(const char *lhs, const Name &rhs);
std::string operator+(const Name &lhs, const char *rhs);

} // namespace vigine
