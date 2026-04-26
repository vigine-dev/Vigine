#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/service/serviceid.h>

#include "../../system/texteditorsystem.h"
#include "../../texteditstate.h"

#include <memory>


namespace vigine
{
class EntityManager;

namespace ecs
{
namespace graphics
{
class GraphicsService;
}
} // namespace ecs
} // namespace vigine

// Creates two entities for the 3D text editor:
//   "TextEditBgEntity"  - flat rectangular background panel (MeshComponent plane)
//   "TextEditEntity"    - bitmap text with individual character planes
//
// Each character is rendered as a small plane with a unique color based on its ASCII value.
// Shares TextEditState with RunWindowTask so that keyboard input can update the
// text and the dirty flag triggers a mesh rebuild every frame.
class SetupTextEditTask final : public vigine::AbstractTask
{
  public:
    SetupTextEditTask(std::shared_ptr<TextEditState> state,
                      std::shared_ptr<TextEditorSystem> editorSystem);

    [[nodiscard]] vigine::Result run() override;

    void setEntityManager(vigine::EntityManager *entityManager) noexcept;
    void setGraphicsServiceId(vigine::service::ServiceId id) noexcept;

  private:
    std::shared_ptr<TextEditState> _state;
    std::shared_ptr<TextEditorSystem> _editorSystem;
    vigine::EntityManager *_entityManager{nullptr};
    vigine::service::ServiceId _graphicsServiceId{};
};
