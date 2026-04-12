#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAssetDatabase.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{

    ScriptAssetDatabase::ScriptAssetDatabase() : ScriptObject() {}

    void ScriptAssetDatabase::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Load", (void*)&Internal_Load);
        MetaData.ScriptClass->AddInternalCall("Internal_LoadFromUUID", (void*)&Internal_LoadFromUUID);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAssetPath", (void*)&Internal_GetAssetPath);
        MetaData.ScriptClass->AddInternalCall("Internal_IsValid", (void*)&Internal_IsValid);
    }

    MonoObject* ScriptAssetDatabase::Internal_Load(MonoString* path)
    {
        Path nativePath = MonoUtils::FromMonoString(path);
        AssetHandle<Asset> handle = gAssetManager->Load(nativePath);
        if (!handle.IsLoaded())
            return nullptr;
        ScriptAssetBase* scriptAsset = ScriptAssetManager::Get().GetScriptAsset(handle, true);
        return scriptAsset != nullptr ? scriptAsset->GetManagedInstance() : nullptr;
    }

    MonoObject* ScriptAssetDatabase::Internal_LoadFromUUID(UUID* uuid)
    {
        AssetHandle<Asset> handle = gAssetManager->LoadFromUUID(*uuid);
        if (!handle.IsLoaded())
            return nullptr;
        ScriptAssetBase* scriptAsset = ScriptAssetManager::Get().GetScriptAsset(handle, true);
        return scriptAsset != nullptr ? scriptAsset->GetManagedInstance() : nullptr;
    }

    MonoString* ScriptAssetDatabase::Internal_GetAssetPath(UUID* uuid)
    {
        Path outPath;
        if (!gAssetManager->GetAssetPath(*uuid, outPath))
            return nullptr;
        return MonoUtils::ToMonoString(outPath.string());
    }

    bool ScriptAssetDatabase::Internal_IsValid(UUID* uuid)
    {
        if (*uuid == UUID::EMPTY)
            return false;
        Path outPath;
        return gAssetManager->GetAssetPath(*uuid, outPath);
    }

} // namespace Crowny
