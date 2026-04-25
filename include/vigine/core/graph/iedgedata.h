#pragma once

#include <cstdint>

namespace vigine::core::graph
{
/**
 * @brief Optional polymorphic payload carried by an edge.
 *
 * Pure-virtual interface. Concrete data types subclass @ref IEdgeData and
 * expose their own accessors; callers branch on @ref dataTypeId to select
 * the correct static downcast. The interface is intentionally minimal so
 * that edges without payload can pass `nullptr` through
 * @ref IEdge::data without penalty.
 *
 * @note `dataTypeId` is a runtime tag, not an RTTI hook; implementations
 *       MUST return a stable value across program runs so that downstream
 *       wrappers can match payloads by identifier alone and avoid
 *       `dynamic_cast` in hot paths.
 */
class IEdgeData
{
  public:
    virtual ~IEdgeData() = default;

    /**
     * @brief Returns the stable identifier of the concrete payload type.
     *
     * Each concrete payload class returns its own unique value. Consumers
     * branch on this value to perform a safe `static_cast` to the expected
     * concrete type.
     */
    [[nodiscard]] virtual std::uint32_t dataTypeId() const noexcept = 0;

  protected:
    IEdgeData() = default;

  public:
    IEdgeData(const IEdgeData &)            = delete;
    IEdgeData &operator=(const IEdgeData &) = delete;
    IEdgeData(IEdgeData &&)                 = delete;
    IEdgeData &operator=(IEdgeData &&)      = delete;
};

} // namespace vigine::core::graph
