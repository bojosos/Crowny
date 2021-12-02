#include "cwpch.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"

#include "Crowny/Scene/SceneManager.h"

namespace Crowny
{

    ScriptTime::ScriptTime() : ScriptObject() {}

    void ScriptTime::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("get_time", (void*)&Internal_GetTime);
        MetaData.ScriptClass->AddInternalCall("get_deltaTime", (void*)&Internal_GetDeltaTime);
        MetaData.ScriptClass->AddInternalCall("get_fixedDeltaTime", (void*)&Internal_GetFixedDeltaTime);
        MetaData.ScriptClass->AddInternalCall("get_smoothDeltaTime", (void*)&Internal_GetSmoothDeltaTime);
        MetaData.ScriptClass->AddInternalCall("get_realtimeSinceStartup", (void*)&Internal_RealtimeSinceStartup);
        MetaData.ScriptClass->AddInternalCall("get_frameCount", (void*)&Internal_GetFrameCount);
    }

    float ScriptTime::Internal_GetTime() { return Time::GetTime(); }

    float ScriptTime::Internal_GetDeltaTime() { return Time::GetDeltaTime(); }

    float ScriptTime::Internal_GetFixedDeltaTime() { return Time::GetFixedDeltaTime(); }

    float ScriptTime::Internal_GetSmoothDeltaTime() { return Time::GetSmoothDeltaTime(); }

    float ScriptTime::Internal_RealtimeSinceStartup() { return Time::GetRealtimeSinceStartup(); }

    float ScriptTime::Internal_GetFrameCount() { return Time::GetFrameCount(); }

} // namespace Crowny