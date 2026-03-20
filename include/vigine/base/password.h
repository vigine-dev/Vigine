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
    [[nodiscard]] explicit operator const std::string &() const;

    // equals operator
    [[nodiscard]] bool operator==(const Password &other) const;
    [[nodiscard]] bool operator!=(const Password &other) const;

    // other
    [[nodiscard]] const std::string &str() const;

    void setValidationRule(const std::string &regexPattern);

    [[nodiscard]] bool isValid() const;

  private:
    std::string _value;
    std::optional<std::regex> _validationRule;
};

BUILD_SMART_PTR(Password);

} // namespace vigine
