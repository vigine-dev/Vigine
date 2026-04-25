#pragma once

#include "vigine/core/graph/abstractgraph.h"

namespace vigine::service
{
/**
 * @brief Internal graph specialisation that the service wrapper uses
 *        to hold its service-domain storage.
 *
 * @ref ServiceRegistry is a concrete @c vigine::core::graph::AbstractGraph
 * subtype that seals the inheritance chain for the services wrapper.
 * It carries no additional state and no additional virtual methods; it
 * exists only to keep the graph substrate in a typed wrapper so
 * @ref AbstractService can hold an opaque @c std::unique_ptr to it in
 * a public header without leaking any graph primitives.
 *
 * This header lives under @c src/service on purpose: the INV-11 rule
 * forbids @c vigine::core::graph types from surfacing in
 * @c include/vigine/service. Only the wrapper implementation consumes
 * the registry; callers of @ref IService / @ref AbstractService see
 * neither the registry nor its graph base.
 *
 * Thread-safety inherits from @c AbstractGraph: every mutating entry
 * point takes the graph's exclusive lock; reads take a shared lock.
 * The wrapper layer does not add any additional synchronisation on
 * top; every service-side access path funnels through the registry.
 */
class ServiceRegistry final : public vigine::core::graph::AbstractGraph
{
  public:
    ServiceRegistry();
    ~ServiceRegistry() override;

    ServiceRegistry(const ServiceRegistry &)            = delete;
    ServiceRegistry &operator=(const ServiceRegistry &) = delete;
    ServiceRegistry(ServiceRegistry &&)                 = delete;
    ServiceRegistry &operator=(ServiceRegistry &&)      = delete;
};

} // namespace vigine::service
