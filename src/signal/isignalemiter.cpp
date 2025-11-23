#include "vigine/signal/isignalemiter.h"

#include <utility>

namespace vigine
{
void ISignalEmiter::setProxyEmiter(SignalEmiterProxy proxyEmiter)
{
    _proxyEmiter = std::move(proxyEmiter);
}

const ISignalEmiter::SignalEmiterProxy &ISignalEmiter::proxyEmiter() const { return _proxyEmiter; }
} // namespace vigine
