#include "rendercubetask.h"

#include <vigine/api/engine/iengine_token.h>

#include <iostream>

RenderCubeTask::RenderCubeTask() = default;

vigine::Result RenderCubeTask::run()
{
    if (_expired.load(std::memory_order_acquire))
    {
        // The bound state has been invalidated since the last tick. Stop
        // further work and return an error so the FSM transition table
        // can route the WorkState onto its error follow-on -- the
        // engine-token contract guarantees @ref api()->service /
        // @ref api()->ecs return @ref vigine::engine::Result::Code::Expired
        // here too (note: that is vigine::engine::Result<T>::Code, distinct
        // from the vigine::Result::Code returned by run()), but the
        // explicit short-circuit makes the intent obvious.
        std::cerr << "[RenderCubeTask] Bound state expired; stopping render loop"
                  << std::endl;
        return vigine::Result(vigine::Result::Code::Error,
                              "RenderCubeTask: bound state expired");
    }

    if (!_expirationSubscription)
    {
        if (auto *token = api())
        {
            _expirationSubscription =
                token->subscribeExpiration([this]() {
                    _expired.store(true, std::memory_order_release);
                });
        }
    }

    // Update cube rotation
    _rotationAngle += 0.05f; // Rotate by 0.05 radians per frame
    if (_rotationAngle > 2 * 3.14159f)
        _rotationAngle = 0.0f;

    std::cout << "Rendering cube... (rotation: " << _rotationAngle << " radians)" << std::endl;

    return vigine::Result();
}
