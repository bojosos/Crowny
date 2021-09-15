#pragma once

namespace Crowny
{
    class ScriptRuntime
    {
    public:
        static void Init();
        static void OnStart();
        static void OnUpdate();
        static void OnShutdown();
    };
} // namespace Crowny