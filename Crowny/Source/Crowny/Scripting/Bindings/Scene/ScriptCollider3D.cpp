#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider3D.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{
    ScriptCollider3DBase::ScriptCollider3DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    bool ScriptCollider3DBase::Internal_IsTrigger(ScriptCollider3DBase* thisPtr) { return thisPtr->GetCollider3D().IsTrigger(); }
    void ScriptCollider3DBase::Internal_SetTrigger(ScriptCollider3DBase* thisPtr, bool value) { thisPtr->GetCollider3D().SetIsTrigger(value); }
    void ScriptCollider3DBase::Internal_GetOffset(ScriptCollider3DBase* thisPtr, glm::vec3* value) { *value = thisPtr->GetCollider3D().GetOffset(); }
    void ScriptCollider3DBase::Internal_SetOffset(ScriptCollider3DBase* thisPtr, glm::vec3* value)
    {
        thisPtr->GetCollider3D().SetOffset(*value, thisPtr->GetEntity());
    }
    void ScriptCollider3DBase::Internal_GetRotation(ScriptCollider3DBase* thisPtr, glm::quat* value)
    {
        *value = thisPtr->GetCollider3D().GetRotation();
    }
    void ScriptCollider3DBase::Internal_SetRotation(ScriptCollider3DBase* thisPtr, glm::quat* value)
    {
        thisPtr->GetCollider3D().SetRotation(*value, thisPtr->GetEntity());
    }
    MonoObject* ScriptCollider3DBase::Internal_GetMaterial(ScriptCollider3DBase* thisPtr)
    {
        const AssetHandle<PhysicsMaterial3D>& material = thisPtr->GetCollider3D().GetMaterial();
        if (!material || !ScriptAssetManager::IsStartedUp())
            return nullptr;
        ScriptAssetBase* const asset = ScriptAssetManager::Get().GetScriptAsset(material, true);
        return asset ? asset->GetManagedInstance() : nullptr;
    }
    void ScriptCollider3DBase::Internal_SetMaterial(ScriptCollider3DBase* thisPtr, MonoObject* value)
    {
        ScriptPhysicsMaterial3D* const material = ScriptPhysicsMaterial3D::ToNative(value);
        thisPtr->GetCollider3D().SetMaterial(material ? material->GetHandle() : AssetHandle<PhysicsMaterial3D>());
    }
    void ScriptCollider3DBase::Internal_GetFilter(ScriptCollider3DBase* thisPtr, PhysicsFilter3D* value)
    {
        *value = thisPtr->GetCollider3D().GetFilter();
    }
    void ScriptCollider3DBase::Internal_SetFilter(ScriptCollider3DBase* thisPtr, PhysicsFilter3D* value)
    {
        thisPtr->GetCollider3D().SetFilter(*value, thisPtr->GetEntity());
    }

    ScriptCollider3D::ScriptCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_IsTrigger", (void*)&ScriptCollider3DBase::Internal_IsTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTrigger", (void*)&ScriptCollider3DBase::Internal_SetTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOffset", (void*)&ScriptCollider3DBase::Internal_GetOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOffset", (void*)&ScriptCollider3DBase::Internal_SetOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_GetRotation", (void*)&ScriptCollider3DBase::Internal_GetRotation);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRotation", (void*)&ScriptCollider3DBase::Internal_SetRotation);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMaterial", (void*)&ScriptCollider3DBase::Internal_GetMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMaterial", (void*)&ScriptCollider3DBase::Internal_SetMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFilter", (void*)&ScriptCollider3DBase::Internal_GetFilter);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFilter", (void*)&ScriptCollider3DBase::Internal_SetFilter);
    }

    ScriptBoxCollider3D::ScriptBoxCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}
    void ScriptBoxCollider3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);
    }
    void ScriptBoxCollider3D::Internal_GetSize(ScriptBoxCollider3D* thisPtr, glm::vec3* value) { *value = thisPtr->GetComponent().GetSize(); }
    void ScriptBoxCollider3D::Internal_SetSize(ScriptBoxCollider3D* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().SetSize(*value, thisPtr->GetNativeEntity());
    }

    ScriptSphereCollider3D::ScriptSphereCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}
    void ScriptSphereCollider3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetRadius", (void*)&Internal_GetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRadius", (void*)&Internal_SetRadius);
    }
    float ScriptSphereCollider3D::Internal_GetRadius(ScriptSphereCollider3D* thisPtr) { return thisPtr->GetComponent().GetRadius(); }
    void ScriptSphereCollider3D::Internal_SetRadius(ScriptSphereCollider3D* thisPtr, float value)
    {
        thisPtr->GetComponent().SetRadius(value, thisPtr->GetNativeEntity());
    }

    ScriptCapsuleCollider3D::ScriptCapsuleCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}
    void ScriptCapsuleCollider3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetRadius", (void*)&Internal_GetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRadius", (void*)&Internal_SetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_GetHeight", (void*)&Internal_GetHeight);
        MetaData.ScriptClass->AddInternalCall("Internal_SetHeight", (void*)&Internal_SetHeight);
    }
    float ScriptCapsuleCollider3D::Internal_GetRadius(ScriptCapsuleCollider3D* thisPtr) { return thisPtr->GetComponent().GetRadius(); }
    void ScriptCapsuleCollider3D::Internal_SetRadius(ScriptCapsuleCollider3D* thisPtr, float value)
    {
        thisPtr->GetComponent().SetRadius(value, thisPtr->GetNativeEntity());
    }
    float ScriptCapsuleCollider3D::Internal_GetHeight(ScriptCapsuleCollider3D* thisPtr) { return thisPtr->GetComponent().GetHeight(); }
    void ScriptCapsuleCollider3D::Internal_SetHeight(ScriptCapsuleCollider3D* thisPtr, float value)
    {
        thisPtr->GetComponent().SetHeight(value, thisPtr->GetNativeEntity());
    }
} // namespace Crowny
