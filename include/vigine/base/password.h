#pragma once

#include "macros.h"

#include <optional>
#include <regex>
#include <string>

namespace vigine
{
class Password
{
  public:
    // Constructors
    explicit Password(const std::string &pwd);
    explicit Password(std::string &&pwd);

    // Copy/move
    Password(const Password &)                = default;
    Password(Password &&) noexcept            = default;
    Password &operator=(const Password &)     = default;
    Password &operator=(Password &&) noexcept = default;

    // assign std::string
    Password &operator=(const std::string &pwd);

    Password &operator=(std::string &&pwd) noexcept;

    // cast to std::string
    explicit operator const std::string &() const;

    // equals operator
    bool operator==(const Password &other) const;
    bool operator!=(const Password &other) const;

    // other
    const std::string &str() const;

    void setValidationRule(const std::string &regexPattern);

    bool isValid() const;

  private:
    std::string _value;
    std::optional<std::regex> _validationRule;
};

BUILD_SMART_PTR(Password);

} // namespace vigine
