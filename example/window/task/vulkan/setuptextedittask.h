#pragma once

#include <vigine/api/taskflow/abstracttask.h>

/**
 * @brief Creates entities for the 3D text editor:
 *   "TextEditBgEntity"               - flat rectangular background panel
 *   "TextEditEntity"                 - editable text (instanced glyph quads)
 *   "TextEditScrollbarTrackEntity"   - scrollbar track
 *   "TextEditScrollbarThumbEntity"   - scrollbar thumb
 *   "TextEditFocus*Entity"           - focus frame (4 thin lines, hidden by default)
 *
 * The task self-resolves every dependency through @ref apiToken in
 * @ref run — the engine-default entity manager + the well-known
 * graphics service for render-system access, and the app-scope
 * @c TextEditorService for the editor's state + system pair.
 */
class SetupTextEditTask final : public vigine::AbstractTask
{
  public:
    SetupTextEditTask();

    [[nodiscard]] vigine::Result run() override;
};
