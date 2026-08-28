#pragma once

#include <cstdint>

namespace Crowny
{
    enum class TransformInteractionCompletion : uint8_t
    {
        None,
        Commit,
        Cancel
    };

    constexpr TransformInteractionCompletion ResolveTransformInteractionCompletion(bool interactionActive, bool gizmoUsing,
                                                                                     bool cancelRequested)
    {
        if (!interactionActive)
            return TransformInteractionCompletion::None;
        if (cancelRequested)
            return TransformInteractionCompletion::Cancel;
        return gizmoUsing ? TransformInteractionCompletion::None : TransformInteractionCompletion::Commit;
    }
} // namespace Crowny
