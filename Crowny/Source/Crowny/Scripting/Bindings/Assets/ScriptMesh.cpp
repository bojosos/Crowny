#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"

namespace Crowny
{
    ScriptMesh::ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh) : TScriptAsset(instance, mesh) {}

    void ScriptMesh::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexCount", (void*)&Internal_GetVertexCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIndexCount", (void*)&Internal_GetIndexCount);
    }

    uint32_t ScriptMesh::Internal_GetVertexCount(ScriptMesh* thisPtr)
    {
        return thisPtr->GetHandle()->GetVertexCount();
    }

    uint32_t ScriptMesh::Internal_GetIndexCount(ScriptMesh* thisPtr)
    {
        return thisPtr->GetHandle()->GetIndexCount();
    }
} // namespace Crowny