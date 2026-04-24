#pragma once

/**
 * @file macros.h
 * @brief Boilerplate-generating preprocessor macros used across the engine.
 *
 * Each macro expands into a fixed set of @c using aliases (and, for the
 * smart-pointer variant, matching @c make_ helpers) so that individual
 * headers stay short and the naming convention stays uniform. Macros are
 * the only public symbols in this header.
 */

/**
 * @brief Declares the canonical raw-pointer / reference alias set for
 *        @p className.
 *
 * Expands to:
 *   - @c classNamePtr  — non-const raw pointer.
 *   - @c classNameCPtr — const raw pointer.
 *   - @c classNameRef  — non-const reference.
 *   - @c classNameCRef — const reference.
 */
#define BUILD_PTR(className)                                                                       \
    using className##Ptr  = className *;                                                           \
    using className##CPtr = const className *;                                                     \
    using className##Ref  = className &;                                                           \
    using className##CRef = const className &

/**
 * @brief Declares the canonical @c std::vector alias set for @p className.
 *
 * Covers vectors of values, raw pointers, @c std::unique_ptr, const raw
 * pointers, references, and const references. Requires @c <vector> and
 * @c <memory> to be included by the translation unit using the macro.
 */
#define BUILD_PTR_VECTOR_VECTOR(className)                                                         \
    using className##Vector     = std::vector<className>;                                          \
    using className##PtrVector  = std::vector<className *>;                                        \
    using className##UPtrVector = std::vector<std::unique_ptr<className>>;                         \
    using className##CPtrVector = std::vector<const className *>;                                  \
    using className##RefVector  = std::vector<className &>;                                        \
    using className##CRefVector = std::vector<const className &>

/**
 * @brief Declares smart-pointer aliases plus matching factory helpers for
 *        @p className.
 *
 * Expands to the @c classNameUPtr / @c classNameSPtr aliases and the
 * @c make_classNameUPtr / @c make_classNameSPtr forwarding helpers that
 * perfect-forward their arguments into @c std::make_unique and
 * @c std::make_shared respectively.
 */
#define BUILD_SMART_PTR(className)                                                                 \
    using className##UPtr = std::unique_ptr<className>;                                            \
    using className##SPtr = std::shared_ptr<className>;                                            \
    template <typename... Args>                                                                    \
    inline className##UPtr make_##className##UPtr(Args &&...args)                                  \
    {                                                                                              \
        return std::make_unique<className>(std::forward<Args>(args)...);                           \
    }                                                                                              \
    template <typename... Args>                                                                    \
    inline className##SPtr make_##className##SPtr(Args &&...args)                                  \
    {                                                                                              \
        return std::make_shared<className>(std::forward<Args>(args)...);                           \
    }

/**
 * @brief Declares the canonical owning-container alias for @p className.
 *
 * Expands to @c classNameUPtrContainer, a @c std::vector of
 * @c std::unique_ptr<className>. Assumes @ref BUILD_SMART_PTR has already
 * been expanded for the same class so that @c classNameUPtr exists.
 */
#define BUILD_SMART_PTR_CONTAINER(className)                                                       \
    using className##UPtrContainer = std::vector<className##UPtr>;
