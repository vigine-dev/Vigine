#pragma once

#include <vigine/abstractservice.h>

namespace vigine {
class DatabaseService : public AbstractService {
public:
  DatabaseService(const ServiceName& name);

  ServiceId id() const override;
};
} // namespace vigine
