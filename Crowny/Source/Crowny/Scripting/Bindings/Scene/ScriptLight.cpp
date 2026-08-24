#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptLight.h"

namespace Crowny
{
    ScriptLight::ScriptLight(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptLight::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetType", (void*)&Internal_GetType);
        MetaData.ScriptClass->AddInternalCall("Internal_SetType", (void*)&Internal_SetType);
        MetaData.ScriptClass->AddInternalCall("Internal_GetColor", (void*)&Internal_GetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColor", (void*)&Internal_SetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIntensity", (void*)&Internal_GetIntensity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetIntensity", (void*)&Internal_SetIntensity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetRange", (void*)&Internal_GetRange);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRange", (void*)&Internal_SetRange);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSpotInnerAngle", (void*)&Internal_GetSpotInnerAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSpotInnerAngle", (void*)&Internal_SetSpotInnerAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSpotOuterAngle", (void*)&Internal_GetSpotOuterAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSpotOuterAngle", (void*)&Internal_SetSpotOuterAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSourceRadius", (void*)&Internal_GetSourceRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSourceRadius", (void*)&Internal_SetSourceRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUseColorTemperature", (void*)&Internal_GetUseColorTemperature);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUseColorTemperature", (void*)&Internal_SetUseColorTemperature);
        MetaData.ScriptClass->AddInternalCall("Internal_GetTemperature", (void*)&Internal_GetTemperature);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTemperature", (void*)&Internal_SetTemperature);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVisibilityLayers", (void*)&Internal_GetVisibilityLayers);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVisibilityLayers", (void*)&Internal_SetVisibilityLayers);
        MetaData.ScriptClass->AddInternalCall("Internal_GetEnabled", (void*)&Internal_GetEnabled);
        MetaData.ScriptClass->AddInternalCall("Internal_SetEnabled", (void*)&Internal_SetEnabled);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAffectDiffuse", (void*)&Internal_GetAffectDiffuse);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAffectDiffuse", (void*)&Internal_SetAffectDiffuse);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAffectSpecular", (void*)&Internal_GetAffectSpecular);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAffectSpecular", (void*)&Internal_SetAffectSpecular);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVolumetric", (void*)&Internal_GetVolumetric);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVolumetric", (void*)&Internal_SetVolumetric);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadows", (void*)&Internal_GetShadows);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadows", (void*)&Internal_SetShadows);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowBias", (void*)&Internal_GetShadowBias);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowBias", (void*)&Internal_SetShadowBias);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowNormalBias", (void*)&Internal_GetShadowNormalBias);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowNormalBias", (void*)&Internal_SetShadowNormalBias);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowNearPlane", (void*)&Internal_GetShadowNearPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowNearPlane", (void*)&Internal_SetShadowNearPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowImportance", (void*)&Internal_GetShadowImportance);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowImportance", (void*)&Internal_SetShadowImportance);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowResolution", (void*)&Internal_GetShadowResolution);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowResolution", (void*)&Internal_SetShadowResolution);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCacheStaticShadowCasters", (void*)&Internal_GetCacheStaticShadowCasters);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCacheStaticShadowCasters", (void*)&Internal_SetCacheStaticShadowCasters);
    }

    int32_t ScriptLight::Internal_GetType(ScriptLight* thisPtr) { return static_cast<int32_t>(thisPtr->GetComponent().Type); }

    void ScriptLight::Internal_SetType(ScriptLight* thisPtr, int32_t value)
    {
        thisPtr->GetComponent().Type = static_cast<LightType>(std::clamp(value, 0, 2));
    }

    void ScriptLight::Internal_GetColor(ScriptLight* thisPtr, glm::vec4* value)
    {
        *value = glm::vec4(thisPtr->GetComponent().Color, 1.0f);
    }

    void ScriptLight::Internal_SetColor(ScriptLight* thisPtr, glm::vec4* value)
    {
        thisPtr->GetComponent().Color = glm::max(glm::vec3(*value), glm::vec3(0.0f));
    }

    float ScriptLight::Internal_GetIntensity(ScriptLight* thisPtr) { return thisPtr->GetComponent().Intensity; }
    void ScriptLight::Internal_SetIntensity(ScriptLight* thisPtr, float value) { thisPtr->GetComponent().Intensity = std::max(value, 0.0f); }
    float ScriptLight::Internal_GetRange(ScriptLight* thisPtr) { return thisPtr->GetComponent().Range; }
    void ScriptLight::Internal_SetRange(ScriptLight* thisPtr, float value) { thisPtr->GetComponent().Range = std::max(value, 0.001f); }
    float ScriptLight::Internal_GetSpotInnerAngle(ScriptLight* thisPtr) { return glm::degrees(thisPtr->GetComponent().SpotInnerAngle); }

    void ScriptLight::Internal_SetSpotInnerAngle(ScriptLight* thisPtr, float value)
    {
        LightComponent& light = thisPtr->GetComponent();
        light.SpotInnerAngle = glm::radians(std::clamp(value, 0.0f, glm::degrees(light.SpotOuterAngle)));
    }

    float ScriptLight::Internal_GetSpotOuterAngle(ScriptLight* thisPtr) { return glm::degrees(thisPtr->GetComponent().SpotOuterAngle); }

    void ScriptLight::Internal_SetSpotOuterAngle(ScriptLight* thisPtr, float value)
    {
        LightComponent& light = thisPtr->GetComponent();
        light.SpotOuterAngle = glm::radians(std::clamp(value, glm::degrees(light.SpotInnerAngle), 180.0f));
    }

    float ScriptLight::Internal_GetSourceRadius(ScriptLight* thisPtr) { return thisPtr->GetComponent().SourceRadius; }
    void ScriptLight::Internal_SetSourceRadius(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().SourceRadius = std::max(value, 0.0f);
    }
    bool ScriptLight::Internal_GetUseColorTemperature(ScriptLight* thisPtr) { return thisPtr->GetComponent().UseColorTemperature; }
    void ScriptLight::Internal_SetUseColorTemperature(ScriptLight* thisPtr, bool value)
    {
        thisPtr->GetComponent().UseColorTemperature = value;
    }
    float ScriptLight::Internal_GetTemperature(ScriptLight* thisPtr) { return thisPtr->GetComponent().Temperature; }
    void ScriptLight::Internal_SetTemperature(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().Temperature = std::clamp(value, 1000.0f, 40000.0f);
    }
    uint32_t ScriptLight::Internal_GetVisibilityLayers(ScriptLight* thisPtr)
    {
        return thisPtr->GetComponent().VisibilityLayers.Value;
    }
    void ScriptLight::Internal_SetVisibilityLayers(ScriptLight* thisPtr, uint32_t value)
    {
        thisPtr->GetComponent().VisibilityLayers.Value = value;
    }

    bool ScriptLight::Internal_GetEnabled(ScriptLight* thisPtr) { return thisPtr->GetComponent().Enabled; }
    void ScriptLight::Internal_SetEnabled(ScriptLight* thisPtr, bool value) { thisPtr->GetComponent().Enabled = value; }
    bool ScriptLight::Internal_GetAffectDiffuse(ScriptLight* thisPtr) { return thisPtr->GetComponent().AffectDiffuse; }
    void ScriptLight::Internal_SetAffectDiffuse(ScriptLight* thisPtr, bool value) { thisPtr->GetComponent().AffectDiffuse = value; }
    bool ScriptLight::Internal_GetAffectSpecular(ScriptLight* thisPtr) { return thisPtr->GetComponent().AffectSpecular; }
    void ScriptLight::Internal_SetAffectSpecular(ScriptLight* thisPtr, bool value) { thisPtr->GetComponent().AffectSpecular = value; }
    bool ScriptLight::Internal_GetVolumetric(ScriptLight* thisPtr) { return thisPtr->GetComponent().Volumetric; }
    void ScriptLight::Internal_SetVolumetric(ScriptLight* thisPtr, bool value) { thisPtr->GetComponent().Volumetric = value; }
    int32_t ScriptLight::Internal_GetShadows(ScriptLight* thisPtr) { return static_cast<int32_t>(thisPtr->GetComponent().Shadows.Mode); }

    void ScriptLight::Internal_SetShadows(ScriptLight* thisPtr, int32_t value)
    {
        thisPtr->GetComponent().Shadows.Mode = static_cast<LightShadowMode>(std::clamp(value, 0, 2));
    }
    float ScriptLight::Internal_GetShadowBias(ScriptLight* thisPtr) { return thisPtr->GetComponent().Shadows.Bias; }
    void ScriptLight::Internal_SetShadowBias(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().Shadows.Bias = std::max(value, 0.0f);
    }
    float ScriptLight::Internal_GetShadowNormalBias(ScriptLight* thisPtr) { return thisPtr->GetComponent().Shadows.NormalBias; }
    void ScriptLight::Internal_SetShadowNormalBias(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().Shadows.NormalBias = std::max(value, 0.0f);
    }
    float ScriptLight::Internal_GetShadowNearPlane(ScriptLight* thisPtr) { return thisPtr->GetComponent().Shadows.NearPlane; }
    void ScriptLight::Internal_SetShadowNearPlane(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().Shadows.NearPlane = std::max(value, 0.001f);
    }
    float ScriptLight::Internal_GetShadowImportance(ScriptLight* thisPtr) { return thisPtr->GetComponent().Shadows.Importance; }
    void ScriptLight::Internal_SetShadowImportance(ScriptLight* thisPtr, float value)
    {
        thisPtr->GetComponent().Shadows.Importance = std::max(value, 0.0f);
    }
    uint32_t ScriptLight::Internal_GetShadowResolution(ScriptLight* thisPtr) { return thisPtr->GetComponent().Shadows.Resolution; }
    void ScriptLight::Internal_SetShadowResolution(ScriptLight* thisPtr, uint32_t value)
    {
        thisPtr->GetComponent().Shadows.Resolution = static_cast<uint16_t>(std::clamp(value, 64u, 8192u));
    }
    bool ScriptLight::Internal_GetCacheStaticShadowCasters(ScriptLight* thisPtr)
    {
        return thisPtr->GetComponent().Shadows.CacheStaticCasters;
    }
    void ScriptLight::Internal_SetCacheStaticShadowCasters(ScriptLight* thisPtr, bool value)
    {
        thisPtr->GetComponent().Shadows.CacheStaticCasters = value;
    }
} // namespace Crowny
