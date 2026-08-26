#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{
    ScriptPhysicsMaterial2D::ScriptPhysicsMaterial2D(MonoObject* instance, const AssetHandle<PhysicsMaterial2D>& material)
        : TScriptAsset(instance, material)
    {
    }

    void ScriptPhysicsMaterial2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Create", (void*)&Internal_Create);
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

    MonoObject* ScriptPhysicsMaterial2D::Internal_Create()
    {
        if (AssetManager::TryGet() == nullptr || !ScriptAssetManager::IsStartedUp())
            return nullptr;

        const AssetHandle<PhysicsMaterial2D> material = CreateRuntimePhysicsMaterial2D(*AssetManager::TryGet());
        ScriptAssetBase* const scriptAsset = ScriptAssetManager::Get().GetScriptAsset(material, true);
        return scriptAsset != nullptr ? scriptAsset->GetManagedInstance() : nullptr;
    }

    float ScriptPhysicsMaterial2D::Internal_GetDensity(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetDensity() : 0.0f;
    }

    void ScriptPhysicsMaterial2D::Internal_SetDensity(ScriptPhysicsMaterial2D* thisPtr, float value)
    {
        if (!thisPtr->GetHandle())
            return;
        thisPtr->GetHandle()->SetDensity(value);
    }

    float ScriptPhysicsMaterial2D::Internal_GetFriction(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetFriction() : 0.0f;
    }

    void ScriptPhysicsMaterial2D::Internal_SetFriction(ScriptPhysicsMaterial2D* thisPtr, float value)
    {
        if (!thisPtr->GetHandle())
            return;
        thisPtr->GetHandle()->SetFriction(value);
    }

    float ScriptPhysicsMaterial2D::Internal_GetRestitution(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitution() : 0.0f;
    }

    void ScriptPhysicsMaterial2D::Internal_SetRestitution(ScriptPhysicsMaterial2D* thisPtr, float value)
    {
        if (!thisPtr->GetHandle())
            return;
        thisPtr->GetHandle()->SetRestitution(value);
    }

    float ScriptPhysicsMaterial2D::Internal_GetRestitutionThreshold(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitutionThreshold() : 0.0f;
    }

    void ScriptPhysicsMaterial2D::Internal_SetRestitutionThreshold(ScriptPhysicsMaterial2D* thisPtr, float value)
    {
        if (!thisPtr->GetHandle())
            return;
        thisPtr->GetHandle()->SetRestitutionThreshold(value);
    }

    PhysicsCombineMode ScriptPhysicsMaterial2D::Internal_GetFrictionCombine(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetFrictionCombine() : PhysicsCombineMode::GeometricMean;
    }

    void ScriptPhysicsMaterial2D::Internal_SetFrictionCombine(ScriptPhysicsMaterial2D* thisPtr, PhysicsCombineMode value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetFrictionCombine(value);
    }

    PhysicsCombineMode ScriptPhysicsMaterial2D::Internal_GetRestitutionCombine(ScriptPhysicsMaterial2D* thisPtr)
    {
        return thisPtr->GetHandle() ? thisPtr->GetHandle()->GetRestitutionCombine() : PhysicsCombineMode::Maximum;
    }

    void ScriptPhysicsMaterial2D::Internal_SetRestitutionCombine(ScriptPhysicsMaterial2D* thisPtr, PhysicsCombineMode value)
    {
        if (thisPtr->GetHandle())
            thisPtr->GetHandle()->SetRestitutionCombine(value);
    }
} // namespace Crowny
