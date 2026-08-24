#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"

namespace Crowny
{
    ScriptPhysicsMaterial3D::ScriptPhysicsMaterial3D(MonoObject* instance, const AssetHandle<PhysicsMaterial3D>& material)
        : TScriptAsset(instance, material)
    {
    }

    void ScriptPhysicsMaterial3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetDensity", (void*)&Internal_GetDensity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetDensity", (void*)&Internal_SetDensity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFriction", (void*)&Internal_GetFriction);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFriction", (void*)&Internal_SetFriction);
        MetaData.ScriptClass->AddInternalCall("Internal_GetRestitution", (void*)&Internal_GetRestitution);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRestitution", (void*)&Internal_SetRestitution);
        MetaData.ScriptClass->AddInternalCall("Internal_GetRestitutionThreshold", (void*)&Internal_GetRestitutionThreshold);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRestitutionThreshold", (void*)&Internal_SetRestitutionThreshold);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFrictionCombine", (void*)&Internal_GetFrictionCombine);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFrictionCombine", (void*)&Internal_SetFrictionCombine);
        MetaData.ScriptClass->AddInternalCall("Internal_GetRestitutionCombine", (void*)&Internal_GetRestitutionCombine);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRestitutionCombine", (void*)&Internal_SetRestitutionCombine);
    }

    float ScriptPhysicsMaterial3D::Internal_GetDensity(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetDensity() : 0.0f;
    }

    void ScriptPhysicsMaterial3D::Internal_SetDensity(ScriptPhysicsMaterial3D* thisPtr, float value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetDensity(value);
    }

    float ScriptPhysicsMaterial3D::Internal_GetFriction(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetFriction() : 0.0f;
    }

    void ScriptPhysicsMaterial3D::Internal_SetFriction(ScriptPhysicsMaterial3D* thisPtr, float value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetFriction(value);
    }

    float ScriptPhysicsMaterial3D::Internal_GetRestitution(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitution() : 0.0f;
    }

    void ScriptPhysicsMaterial3D::Internal_SetRestitution(ScriptPhysicsMaterial3D* thisPtr, float value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetRestitution(value);
    }

    float ScriptPhysicsMaterial3D::Internal_GetRestitutionThreshold(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitutionThreshold() : 0.0f;
    }

    void ScriptPhysicsMaterial3D::Internal_SetRestitutionThreshold(ScriptPhysicsMaterial3D* thisPtr, float value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetRestitutionThreshold(value);
    }

    PhysicsCombineMode ScriptPhysicsMaterial3D::Internal_GetFrictionCombine(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetFrictionCombine() : PhysicsCombineMode::GeometricMean;
    }

    void ScriptPhysicsMaterial3D::Internal_SetFrictionCombine(ScriptPhysicsMaterial3D* thisPtr, PhysicsCombineMode value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetFrictionCombine(value);
    }

    PhysicsCombineMode ScriptPhysicsMaterial3D::Internal_GetRestitutionCombine(ScriptPhysicsMaterial3D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitutionCombine() : PhysicsCombineMode::Maximum;
    }

    void ScriptPhysicsMaterial3D::Internal_SetRestitutionCombine(ScriptPhysicsMaterial3D* thisPtr, PhysicsCombineMode value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetRestitutionCombine(value);
    }
} // namespace Crowny
