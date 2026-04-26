#pragma once

/**
 * @file macros.h
 * @brief Boilerplate-generating preprocessor macros used across the engine.
 *
 * Each macro expands into a fixed set of @c using aliases so that
 * individual headers stay short and the naming convention stays uniform.
 * Macros are the only public symbols in this header.
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
 * @brief Declares the canonical owning-container alias for @p className.
 *
 * Expands to @c classNameUPtrContainer, a @c std::vector of
 * @c std::unique_ptr<className>. Assumes that @c classNameUPtr has
 * already been declared (typically as
 * @c "using classNameUPtr = std::unique_ptr<className>;").
 */
#define BUILD_SMART_PTR_CONTAINER(className)                                                       \
    using className##UPtrContainer = std::vector<className##UPtr>;
