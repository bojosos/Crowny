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

        static void Reload();
        static void UnloadAssemblies();
    };
} // namespace Crowny