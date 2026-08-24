#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptLight : public TScriptComponent<ScriptLight, LightComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "LightComponent");

        ScriptLight(MonoObject* instance, Entity entity);

    private:
        static int32_t Internal_GetType(ScriptLight* thisPtr);
        static void Internal_SetType(ScriptLight* thisPtr, int32_t value);
        static void Internal_GetColor(ScriptLight* thisPtr, glm::vec4* value);
        static void Internal_SetColor(ScriptLight* thisPtr, glm::vec4* value);
        static float Internal_GetIntensity(ScriptLight* thisPtr);
        static void Internal_SetIntensity(ScriptLight* thisPtr, float value);
        static float Internal_GetRange(ScriptLight* thisPtr);
        static void Internal_SetRange(ScriptLight* thisPtr, float value);
        static float Internal_GetSpotInnerAngle(ScriptLight* thisPtr);
        static void Internal_SetSpotInnerAngle(ScriptLight* thisPtr, float value);
        static float Internal_GetSpotOuterAngle(ScriptLight* thisPtr);
        static void Internal_SetSpotOuterAngle(ScriptLight* thisPtr, float value);
        static float Internal_GetSourceRadius(ScriptLight* thisPtr);
        static void Internal_SetSourceRadius(ScriptLight* thisPtr, float value);
        static bool Internal_GetUseColorTemperature(ScriptLight* thisPtr);
        static void Internal_SetUseColorTemperature(ScriptLight* thisPtr, bool value);
        static float Internal_GetTemperature(ScriptLight* thisPtr);
        static void Internal_SetTemperature(ScriptLight* thisPtr, float value);
        static uint32_t Internal_GetVisibilityLayers(ScriptLight* thisPtr);
        static void Internal_SetVisibilityLayers(ScriptLight* thisPtr, uint32_t value);
        static bool Internal_GetEnabled(ScriptLight* thisPtr);
        static void Internal_SetEnabled(ScriptLight* thisPtr, bool value);
        static bool Internal_GetAffectDiffuse(ScriptLight* thisPtr);
        static void Internal_SetAffectDiffuse(ScriptLight* thisPtr, bool value);
        static bool Internal_GetAffectSpecular(ScriptLight* thisPtr);
        static void Internal_SetAffectSpecular(ScriptLight* thisPtr, bool value);
        static bool Internal_GetVolumetric(ScriptLight* thisPtr);
        static void Internal_SetVolumetric(ScriptLight* thisPtr, bool value);
        static int32_t Internal_GetShadows(ScriptLight* thisPtr);
        static void Internal_SetShadows(ScriptLight* thisPtr, int32_t value);
        static float Internal_GetShadowBias(ScriptLight* thisPtr);
        static void Internal_SetShadowBias(ScriptLight* thisPtr, float value);
        static float Internal_GetShadowNormalBias(ScriptLight* thisPtr);
        static void Internal_SetShadowNormalBias(ScriptLight* thisPtr, float value);
        static float Internal_GetShadowNearPlane(ScriptLight* thisPtr);
        static void Internal_SetShadowNearPlane(ScriptLight* thisPtr, float value);
        static float Internal_GetShadowImportance(ScriptLight* thisPtr);
        static void Internal_SetShadowImportance(ScriptLight* thisPtr, float value);
        static uint32_t Internal_GetShadowResolution(ScriptLight* thisPtr);
        static void Internal_SetShadowResolution(ScriptLight* thisPtr, uint32_t value);
        static bool Internal_GetCacheStaticShadowCasters(ScriptLight* thisPtr);
        static void Internal_SetCacheStaticShadowCasters(ScriptLight* thisPtr, bool value);
    };
} // namespace Crowny
