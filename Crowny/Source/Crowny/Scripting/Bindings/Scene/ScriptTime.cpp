#include "cwpch.h"

#include "ScriptTime.h"

#include "Crowny/Scripting/CWMonoRuntime.h"
#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{

    void ScriptTime::InitRuntimeFunctions()
	{
		CWMonoClass* timeClass = CWMonoRuntime::GetAssembly("")->GetClass("Crowny", "Time");

        timeClass->AddInternalCall("Internal_GetTime", (void*)&Internal_GetTime);
		timeClass->AddInternalCall("Internal_GetDeltaTime", (void*)&Internal_GetDeltaTime);
        timeClass->AddInternalCall("Internal_GetFixedDeltaTime", (void*)&Internal_GetFixedDeltaTime);
        timeClass->AddInternalCall("Internal_GetSmoothDeltaTime", (void*)&Internal_GetSmoothDeltaTime);
        timeClass->AddInternalCall("Internal_RealtimeSinceStartup", (void*)&Internal_RealtimeSinceStartup);
        timeClass->AddInternalCall("Internal_GetFrameCount", (void*)&Internal_GetFrameCount);
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