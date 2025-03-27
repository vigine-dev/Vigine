#include "vigine/system/rendersystem.h"
#include <iostream>

namespace vigine {

RenderSystem::RenderSystem() {
    // Конструктор
}

void RenderSystem::update() {
    std::cout << "Updating render system" << std::endl;
}

} // namespace vigine
