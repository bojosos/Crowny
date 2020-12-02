#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"

#include "Crowny/Scripting/CWMonoRuntime.h"
#include "Crowny/Scene/SceneManager.h"

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

    float ScriptTime::Internal_GetTime()
    {
        return SceneManager::GetActiveScene()->GetTime();
    }

    float ScriptTime::Internal_GetDeltaTime()
    {
        return SceneManager::GetActiveScene()->GetDeltaTime();
    }

    float ScriptTime::Internal_GetFixedDeltaTime()
    {
        return SceneManager::GetActiveScene()->GetFixedDeltaTime();
    }

    float ScriptTime::Internal_GetSmoothDeltaTime()
    {
        return SceneManager::GetActiveScene()->GetSmoothDeltaTime();
    }

    float ScriptTime::Internal_RealtimeSinceStartup()
    {
        return SceneManager::GetActiveScene()->GetRealtimeSinceStartup();
    }

    float ScriptTime::Internal_GetFrameCount()
    {
        return SceneManager::GetActiveScene()->GetFrameCount();
    }

}