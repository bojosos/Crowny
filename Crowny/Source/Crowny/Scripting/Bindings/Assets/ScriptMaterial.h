#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptMaterial : public TScriptAsset<ScriptMaterial, Material>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Material");
        ScriptMaterial(MonoObject* instance, const AssetHandle<Material>& material);

    private:
        static void Internal_SetFloat(ScriptMaterial* thisPtr, MonoString* name, float value);
        static void Internal_SetFloat2(ScriptMaterial* thisPtr, MonoString* name, glm::vec2* value);
        static void Internal_SetInt(ScriptMaterial* thisPtr, MonoString* name, int value);
        static void Internal_SetColor(ScriptMaterial* thisPtr, MonoString* name, glm::vec4* color);
        static void Internal_SetVector3(ScriptMaterial* thisPtr, MonoString* name, glm::vec3* value);
        static void Internal_SetMatrix(ScriptMaterial* thisPtr, MonoString* name, glm::mat4* matrix);
        static void Internal_SetTexture(ScriptMaterial* thisPtr, MonoString* name, MonoObject* texture);
    };
} // namespace Crowny
