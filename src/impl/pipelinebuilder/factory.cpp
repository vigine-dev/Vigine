#include "vigine/api/pipelinebuilder/factory.h"

// createPipelineBuilder is defined in src/impl/pipelinebuilder/pipelinebuilder.cpp.
// This translation unit exists so factory.h has a corresponding .cpp
// and the linker always sees the definition regardless of which TU
// callers include factory.h from.
