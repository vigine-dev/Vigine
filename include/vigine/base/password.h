#pragma once

/**
 * @file password.h
 * @brief Strongly-typed wrapper around a password string with an optional
 *        regex-based validation rule.
 */

#include <memory>
#include <optional>
#include <regex>
#include <string>

namespace vigine
{
/**
 * @brief Value type representing a user-supplied password.
 *
 * @ref Password carries a @c std::string payload with explicit-only
 * construction so that arbitrary strings cannot be silently promoted to a
 * password. An optional @c std::regex validation rule can be attached via
 * @ref setValidationRule; @ref isValid then returns whether the current
 * value matches the rule (or @c true when no rule has been set).
 *
 * Smart-pointer aliases @c PasswordUPtr and @c PasswordSPtr are declared
 * alongside the class so call sites can use them without redeclaring.
 */
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

using PasswordUPtr = std::unique_ptr<Password>;
using PasswordSPtr = std::shared_ptr<Password>;

} // namespace vigine
