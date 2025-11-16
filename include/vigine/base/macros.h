#pragma once

#define BUILD_PTR(className)                                                                       \
    using className##Ptr  = className *;                                                           \
    using className##CPtr = const className *;                                                     \
    using className##Ref  = className &;                                                           \
    using className##CRef = const className &

#define BUILD_PTR_VECTOR_VECTOR(className)                                                         \
    using className##Vector     = std::vector<className>;                                          \
    using className##PtrVector  = std::vector<className *>;                                        \
    using className##UPtrVector = std::vector<std::unique_ptr<className>>;                         \
    using className##CPtrVector = std::vector<const className *>;                                  \
    using className##RefVector  = std::vector<className &>;                                        \
    using className##CRefVector = std::vector<const className &>

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

#define BUILD_SMART_PTR_CONTAINER(className)                                                       \
    using className##UPtrContainer = std::vector<className##UPtr>;
