#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptAssetDatabase : public ScriptObject<ScriptAssetDatabase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AssetDatabase")
        ScriptAssetDatabase();

    private:
        static MonoObject* Internal_Load(MonoString* path);
        static MonoObject* Internal_LoadFromUUID(UUID* uuid);
        static MonoString* Internal_GetAssetPath(UUID* uuid);
        static bool Internal_IsValid(UUID* uuid);
    };
} // namespace Crowny
