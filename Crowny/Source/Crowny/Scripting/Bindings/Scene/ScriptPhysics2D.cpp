#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics2D.h"
#include "Crowny/Scripting/Mono/MonoClass.h"

namespace Crowny
{
    void ScriptPhysics2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Raycast", (void*)&Internal_Raycast);
    }

    void ScriptPhysics2D::Internal_Raycast(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask, MonoArray** outResults)
    {
        // TODO: Implement
    }
} // namespace Crowny
