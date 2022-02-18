#pragma once

namespace Crowny
{

    struct EditorSettings
    {
        bool ShowPhysicsColliders2D = false;
        bool ShowImGuiDemoWindow = false;

        float GridMoveSnapX = 0.1f;
        float GridMoveSnapY = 0.1f;
        float GridMoveSnapZ = 0.1f;

        float GridRotateSnap = 15.0f;

        float GridScaleSnap = 0.1f;
    };

}