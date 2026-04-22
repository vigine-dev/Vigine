#include "vigine/channelfactory/factory.h"

// createChannelFactory is defined in defaultchannelfactory.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
