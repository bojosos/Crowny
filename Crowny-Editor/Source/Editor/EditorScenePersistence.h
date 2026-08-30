#pragma once

#include "Crowny/Scene/SceneManager.h"

namespace Crowny
{
    constexpr bool CanSaveEditorScene(SceneExecutionState state) { return state == SceneExecutionState::Edit; }
} // namespace Crowny
