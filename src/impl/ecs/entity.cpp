#include "vigine/impl/ecs/entity.h"

namespace vigine
{

// Entity has no out-of-line members; its destructor and special
// members are defaulted on the header. The translation unit stays in
// the build so the CMake target keeps tracking the file -- if a future
// leaf adds out-of-line state to Entity, it lands here.

} // namespace vigine
