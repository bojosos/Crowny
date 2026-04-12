#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    ScriptMesh::ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh) : TScriptAsset(instance, mesh) {}

    void ScriptMesh::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexCount", (void*)&Internal_GetVertexCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIndexCount", (void*)&Internal_GetIndexCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertices", (void*)&Internal_GetVertices);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVertices", (void*)&Internal_SetVertices);
        MetaData.ScriptClass->AddInternalCall("Internal_GetNormals", (void*)&Internal_GetNormals);
        MetaData.ScriptClass->AddInternalCall("Internal_SetNormals", (void*)&Internal_SetNormals);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUVs", (void*)&Internal_GetUVs);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUVs", (void*)&Internal_SetUVs);
        MetaData.ScriptClass->AddInternalCall("Internal_GetColors", (void*)&Internal_GetColors);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColors", (void*)&Internal_SetColors);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIndices", (void*)&Internal_GetIndices);
        MetaData.ScriptClass->AddInternalCall("Internal_SetIndices", (void*)&Internal_SetIndices);
        MetaData.ScriptClass->AddInternalCall("Internal_RecalculateBounds", (void*)&Internal_RecalculateBounds);
        MetaData.ScriptClass->AddInternalCall("Internal_RecalculateNormals", (void*)&Internal_RecalculateNormals);
        MetaData.ScriptClass->AddInternalCall("Internal_UploadMeshData", (void*)&Internal_UploadMeshData);
        MetaData.ScriptClass->AddInternalCall("Internal_Clear", (void*)&Internal_Clear);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBoundsMin", (void*)&Internal_GetBoundsMin);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBoundsMax", (void*)&Internal_GetBoundsMax);
    }

    uint32_t ScriptMesh::Internal_GetVertexCount(ScriptMesh* thisPtr) { return thisPtr->GetHandle()->GetVertexCount(); }

    uint32_t ScriptMesh::Internal_GetIndexCount(ScriptMesh* thisPtr) { return thisPtr->GetHandle()->GetIndexCount(); }

    void ScriptMesh::Internal_GetVertices(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        Vector<glm::vec3> positions = data->GetPositions();
        ::MonoClass* vec3Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector3->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec3Class, (uintptr_t)positions.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec3, 0), positions.data(), positions.size() * sizeof(glm::vec3));
    }

    void ScriptMesh::Internal_SetVertices(ScriptMesh* thisPtr, MonoArray* array)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec3> positions(count);
        std::memcpy(positions.data(), mono_array_addr(array, glm::vec3, 0), count * sizeof(glm::vec3));
        data->SetPositions(positions);
    }

    void ScriptMesh::Internal_GetNormals(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        Vector<glm::vec3> normals = data->GetNormals();
        ::MonoClass* vec3Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector3->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec3Class, (uintptr_t)normals.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec3, 0), normals.data(), normals.size() * sizeof(glm::vec3));
    }

    void ScriptMesh::Internal_SetNormals(ScriptMesh* thisPtr, MonoArray* array)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec3> normals(count);
        std::memcpy(normals.data(), mono_array_addr(array, glm::vec3, 0), count * sizeof(glm::vec3));
        data->SetNormals(normals);
    }

    void ScriptMesh::Internal_GetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray** outArray)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        Vector<glm::vec2> uvs = data->GetUVs(channel);
        ::MonoClass* vec2Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector2->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec2Class, (uintptr_t)uvs.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec2, 0), uvs.data(), uvs.size() * sizeof(glm::vec2));
    }

    void ScriptMesh::Internal_SetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray* array)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec2> uvs(count);
        std::memcpy(uvs.data(), mono_array_addr(array, glm::vec2, 0), count * sizeof(glm::vec2));
        data->SetUVs(channel, uvs);
    }

    void ScriptMesh::Internal_GetColors(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        Vector<glm::vec4> colors = data->GetColors();
        ::MonoClass* vec4Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector4->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec4Class, (uintptr_t)colors.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec4, 0), colors.data(), colors.size() * sizeof(glm::vec4));
    }

    void ScriptMesh::Internal_SetColors(ScriptMesh* thisPtr, MonoArray* array)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec4> colors(count);
        std::memcpy(colors.data(), mono_array_addr(array, glm::vec4, 0), count * sizeof(glm::vec4));
        data->SetColors(colors);
    }

    void ScriptMesh::Internal_GetIndices(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        Vector<uint32_t> indices = data->GetIndices();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), MonoUtils::GetI32Class(), (uintptr_t)indices.size());
        std::memcpy(mono_array_addr(*outArray, int32_t, 0), indices.data(), indices.size() * sizeof(int32_t));
    }

    void ScriptMesh::Internal_SetIndices(ScriptMesh* thisPtr, MonoArray* array)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        uint32_t count = (uint32_t)mono_array_length(array);
        Vector<uint32_t> indices(count);
        std::memcpy(indices.data(), mono_array_addr(array, int32_t, 0), count * sizeof(uint32_t));
        data->SetIndices(indices);
    }

    void ScriptMesh::Internal_RecalculateBounds(ScriptMesh* thisPtr) { thisPtr->GetHandle()->RecalculateBounds(); }

    void ScriptMesh::Internal_RecalculateNormals(ScriptMesh* thisPtr) { thisPtr->GetHandle()->RecalculateNormals(); }

    void ScriptMesh::Internal_UploadMeshData(ScriptMesh* thisPtr) { thisPtr->GetHandle()->UploadToGpu(); }

    void ScriptMesh::Internal_Clear(ScriptMesh* thisPtr)
    {
        Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (data)
        {
            data->AllocateBuffer(); // Re-zero the buffer
        }
    }

    void ScriptMesh::Internal_GetBoundsMin(ScriptMesh* thisPtr, glm::vec3* outMin)
    {
        // TODO: Access AABox min from Mesh
        *outMin = glm::vec3(0.0f);
    }

    void ScriptMesh::Internal_GetBoundsMax(ScriptMesh* thisPtr, glm::vec3* outMax)
    {
        // TODO: Access AABox max from Mesh
        *outMax = glm::vec3(0.0f);
    }

} // namespace Crowny
