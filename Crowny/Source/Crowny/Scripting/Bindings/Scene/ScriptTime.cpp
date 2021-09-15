#include "cwpch.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{

    void ScriptTime::InitRuntimeFunctions()
    {
        CWMonoClass* timeClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Time");

        timeClass->AddInternalCall("get_time", (void*)&Internal_GetTime);
        timeClass->AddInternalCall("get_deltaTime", (void*)&Internal_GetDeltaTime);
        timeClass->AddInternalCall("get_fixedDeltaTime", (void*)&Internal_GetFixedDeltaTime);
        timeClass->AddInternalCall("get_smoothDeltaTime", (void*)&Internal_GetSmoothDeltaTime);
        timeClass->AddInternalCall("get_realtimeSinceStartup", (void*)&Internal_RealtimeSinceStartup);
        timeClass->AddInternalCall("get_frameCount", (void*)&Internal_GetFrameCount);
    }

    float ScriptTime::Internal_GetTime() { return Time::GetTime(); }

    float ScriptTime::Internal_GetDeltaTime() { return Time::GetDeltaTime(); }

    float ScriptTime::Internal_GetFixedDeltaTime() { return Time::GetFixedDeltaTime(); }

    float ScriptTime::Internal_GetSmoothDeltaTime() { return Time::GetSmoothDeltaTime(); }

    float ScriptTime::Internal_RealtimeSinceStartup() { return Time::GetRealtimeSinceStartup(); }

    float ScriptTime::Internal_GetFrameCount() { return Time::GetFrameCount(); }

} // namespace Crowny