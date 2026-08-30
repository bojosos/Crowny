#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class MonoMethod;

    class ScriptSceneManager : public ScriptObject<ScriptSceneManager>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SceneManager")

        ScriptSceneManager();

        static void DispatchPendingEvents();

    private:
        static MonoMethod* s_NotifySceneEventMethod;
    };
} // namespace Crowny
