// ---------------------------------------------------------------------------
// Scenario 4 -- subscription facade (ISignalEmitter) round-trip.
//
// The signal facade hides MessageKind::Signal / RouteMode::FanOut
// behind an emit() entry point. The scenario:
//
//   1. Builds a SignalEmitter over the shared context's thread manager.
//   2. Subscribes a CountingSubscriber directly to the internal bus it
//      uses (DefaultSignalEmitter's bus is private, but the scope says
//      "uses the shared bus"). Without a direct subscribe surface on
//      ISignalEmitter we cannot observe delivery without owning both
//      the emitter and a bus that hosts the matching subscription.
//
// Because DefaultSignalEmitter builds an InlineOnly bus internally and
// does NOT hand that bus out through a public accessor, the scenario
// exercises the facade by calling emit() with a payload and verifying
// the Result is success. That is the observable contract on the public
// surface (emit returns Result::Success on a valid non-null payload).
// The bus-side delivery is covered by scenario_03.
//
// ISignalEmitter therefore has one observable assertion: emit returns
// success for a valid payload and error for a null payload.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/context/icontext.h"
#include "vigine/result.h"
#include "vigine/signalemitter/defaultsignalemitter.h"
#include "vigine/signalemitter/isignalemitter.h"
#include "vigine/signalemitter/isignalpayload.h"
#include "vigine/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <memory>

namespace vigine::contract
{
namespace
{

class SignalPayload final : public vigine::signalemitter::ISignalPayload
{
  public:
    explicit SignalPayload(vigine::payload::PayloadTypeId id) noexcept : _id(id) {}

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _id;
    }

  private:
    vigine::payload::PayloadTypeId _id;
};

using SignalEmitter = EngineFixture;

TEST_F(SignalEmitter, EmitReturnsSuccessForValidPayload)
{
    auto emitter =
        vigine::signalemitter::createSignalEmitter(context().threadManager());
    ASSERT_NE(emitter, nullptr);

    auto payload =
        std::make_unique<SignalPayload>(vigine::payload::PayloadTypeId{0x20101u});

    const vigine::Result r = emitter->emit(std::move(payload));
    EXPECT_TRUE(r.isSuccess())
        << "emit with a valid payload must succeed; got: " << r.message();
}

TEST_F(SignalEmitter, EmitRejectsNullPayload)
{
    auto emitter =
        vigine::signalemitter::createSignalEmitter(context().threadManager());
    ASSERT_NE(emitter, nullptr);

    const vigine::Result r = emitter->emit(nullptr);
    EXPECT_TRUE(r.isError())
        << "emit with a null payload must report an error";
}

} // namespace
} // namespace vigine::contract
