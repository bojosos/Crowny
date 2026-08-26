#include "cwpch.h"

#include <mono/metadata/object.h>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{
    namespace
    {
        MonoObject* CreateManagedMesh(const Ref<Mesh>& mesh)
        {
            if (!mesh || AssetManager::TryGet() == nullptr || !ScriptAssetManager::IsStartedUp())
                return nullptr;

            const AssetHandle<Mesh> handle = static_asset_cast<Mesh>(AssetManager::TryGet()->CreateAssetHandle(mesh));
            ScriptAssetBase* scriptAsset = ScriptAssetManager::Get().CreateManagedOwnedScriptAsset(handle);
            return scriptAsset != nullptr ? scriptAsset->GetManagedInstance() : nullptr;
        }
    } // namespace

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
        MetaData.ScriptClass->AddInternalCall("Internal_RecalculateTangents", (void*)&Internal_RecalculateTangents);
        MetaData.ScriptClass->AddInternalCall("Internal_UploadMeshData", (void*)&Internal_UploadMeshData);
        MetaData.ScriptClass->AddInternalCall("Internal_Clear", (void*)&Internal_Clear);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBoundsMin", (void*)&Internal_GetBoundsMin);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBoundsMax", (void*)&Internal_GetBoundsMax);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVertexBufferParams", (void*)&Internal_SetVertexBufferParams);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVertexBufferData", (void*)&Internal_SetVertexBufferData);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexBufferData", (void*)&Internal_GetVertexBufferData);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexStride", (void*)&Internal_GetVertexStride);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexAttributeCount", (void*)&Internal_GetVertexAttributeCount);
        MetaData.ScriptClass->AddInternalCall("Internal_HasVertexAttribute", (void*)&Internal_HasVertexAttribute);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVertexAttribute", (void*)&Internal_GetVertexAttribute);
        MetaData.ScriptClass->AddInternalCall("Internal_CreatePlane", (void*)&Internal_CreatePlane);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateBox", (void*)&Internal_CreateBox);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateCube", (void*)&Internal_CreateCube);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateSphere", (void*)&Internal_CreateSphere);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateCylinder", (void*)&Internal_CreateCylinder);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateCone", (void*)&Internal_CreateCone);
        MetaData.ScriptClass->AddInternalCall("Internal_CreateCapsule", (void*)&Internal_CreateCapsule);
    }

    uint32_t ScriptMesh::Internal_GetVertexCount(ScriptMesh* thisPtr) { return thisPtr->GetHandle()->GetVertexCount(); }

    uint32_t ScriptMesh::Internal_GetIndexCount(ScriptMesh* thisPtr) { return thisPtr->GetHandle()->GetIndexCount(); }

    void ScriptMesh::Internal_GetVertices(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        const Vector<glm::vec3> positions = data->GetPositions();
        ::MonoClass* const vec3Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector3->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec3Class, (uintptr_t)positions.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec3, 0), positions.data(), positions.size() * sizeof(glm::vec3));
    }

    void ScriptMesh::Internal_SetVertices(ScriptMesh* thisPtr, MonoArray* array)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        const uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec3> positions(count);
        std::memcpy(positions.data(), mono_array_addr(array, glm::vec3, 0), count * sizeof(glm::vec3));
        data->SetPositions(positions);
    }

    void ScriptMesh::Internal_GetNormals(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        const Vector<glm::vec3> normals = data->GetNormals();
        ::MonoClass* const vec3Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector3->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec3Class, (uintptr_t)normals.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec3, 0), normals.data(), normals.size() * sizeof(glm::vec3));
    }

    void ScriptMesh::Internal_SetNormals(ScriptMesh* thisPtr, MonoArray* array)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        const uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec3> normals(count);
        std::memcpy(normals.data(), mono_array_addr(array, glm::vec3, 0), count * sizeof(glm::vec3));
        data->SetNormals(normals);
    }

    void ScriptMesh::Internal_GetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray** outArray)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        const Vector<glm::vec2> uvs = data->GetUVs(channel);
        ::MonoClass* const vec2Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector2->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec2Class, (uintptr_t)uvs.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec2, 0), uvs.data(), uvs.size() * sizeof(glm::vec2));
    }

    void ScriptMesh::Internal_SetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray* array)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        const uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec2> uvs(count);
        std::memcpy(uvs.data(), mono_array_addr(array, glm::vec2, 0), count * sizeof(glm::vec2));
        data->SetUVs(channel, uvs);
    }

    void ScriptMesh::Internal_GetColors(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        const Vector<glm::vec4> colors = data->GetColors();
        ::MonoClass* const vec4Class = ScriptInfoManager::Get().GetBuiltinClasses().Vector4->GetInternalPtr();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), vec4Class, (uintptr_t)colors.size());
        std::memcpy(mono_array_addr(*outArray, glm::vec4, 0), colors.data(), colors.size() * sizeof(glm::vec4));
    }

    void ScriptMesh::Internal_SetColors(ScriptMesh* thisPtr, MonoArray* array)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        const uint32_t count = (uint32_t)mono_array_length(array);
        Vector<glm::vec4> colors(count);
        std::memcpy(colors.data(), mono_array_addr(array, glm::vec4, 0), count * sizeof(glm::vec4));
        data->SetColors(colors);
    }

    void ScriptMesh::Internal_GetIndices(ScriptMesh* thisPtr, MonoArray** outArray)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data)
        {
            *outArray = nullptr;
            return;
        }

        const Vector<uint32_t> indices = data->GetIndices();
        *outArray = mono_array_new(MonoManager::Get().GetDomain(), MonoUtils::GetI32Class(), (uintptr_t)indices.size());
        std::memcpy(mono_array_addr(*outArray, int32_t, 0), indices.data(), indices.size() * sizeof(int32_t));
    }

    void ScriptMesh::Internal_SetIndices(ScriptMesh* thisPtr, MonoArray* array)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
        if (!data || !array)
            return;

        const uint32_t count = (uint32_t)mono_array_length(array);
        Vector<uint32_t> indices(count);
        std::memcpy(indices.data(), mono_array_addr(array, int32_t, 0), count * sizeof(uint32_t));
        data->SetIndices(indices);
    }

    void ScriptMesh::Internal_RecalculateBounds(ScriptMesh* thisPtr) { thisPtr->GetHandle()->RecalculateBounds(); }

    void ScriptMesh::Internal_RecalculateNormals(ScriptMesh* thisPtr) { thisPtr->GetHandle()->RecalculateNormals(); }

    void ScriptMesh::Internal_RecalculateTangents(ScriptMesh* thisPtr) { thisPtr->GetHandle()->RecalculateTangents(); }

    void ScriptMesh::Internal_UploadMeshData(ScriptMesh* thisPtr) { thisPtr->GetHandle()->UploadToGpu(); }

    void ScriptMesh::Internal_Clear(ScriptMesh* thisPtr)
    {
        const Ref<MeshData> data = thisPtr->GetHandle()->GetMeshData();
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

    ShaderDataType ScriptMesh::DescriptorToShaderDataType(ScriptVertexAttributeFormat format, int32_t dimension)
    {
        if (format == ScriptVertexAttributeFormat::Float32)
        {
            switch (dimension)
            {
            case 1: return ShaderDataType::Float;
            case 2: return ShaderDataType::Float2;
            case 3: return ShaderDataType::Float3;
            case 4: return ShaderDataType::Float4;
            }
        }
        else if (format == ScriptVertexAttributeFormat::SInt32 || format == ScriptVertexAttributeFormat::UInt32)
        {
            switch (dimension)
            {
            case 1: return ShaderDataType::Int;
            case 2: return ShaderDataType::Int2;
            case 3: return ShaderDataType::Int3;
            case 4: return ShaderDataType::Int4;
            }
        }
        else if (format == ScriptVertexAttributeFormat::UNorm8 || format == ScriptVertexAttributeFormat::UInt8)
        {
            if (dimension == 4)
                return ShaderDataType::UByte4;
        }
        else if (format == ScriptVertexAttributeFormat::SNorm8 || format == ScriptVertexAttributeFormat::SInt8)
        {
            switch (dimension)
            {
            case 1: return ShaderDataType::SByte;
            case 2: return ShaderDataType::SByte2;
            case 3: return ShaderDataType::SByte3;
            case 4: return ShaderDataType::SByte4;
            }
        }
        CW_ENGINE_WARN("Unsupported vertex attribute format/dimension combination: format={}, dim={}", (int)format, dimension);
        return ShaderDataType::Float3;
    }

    static void ShaderDataTypeToFormatAndDimension(ShaderDataType type, ScriptVertexAttributeFormat& outFormat, int32_t& outDimension)
    {
        switch (type)
        {
        case ShaderDataType::Float:  outFormat = ScriptVertexAttributeFormat::Float32; outDimension = 1; return;
        case ShaderDataType::Float2: outFormat = ScriptVertexAttributeFormat::Float32; outDimension = 2; return;
        case ShaderDataType::Float3: outFormat = ScriptVertexAttributeFormat::Float32; outDimension = 3; return;
        case ShaderDataType::Float4: outFormat = ScriptVertexAttributeFormat::Float32; outDimension = 4; return;
        case ShaderDataType::Int:    outFormat = ScriptVertexAttributeFormat::SInt32; outDimension = 1; return;
        case ShaderDataType::Int2:   outFormat = ScriptVertexAttributeFormat::SInt32; outDimension = 2; return;
        case ShaderDataType::Int3:   outFormat = ScriptVertexAttributeFormat::SInt32; outDimension = 3; return;
        case ShaderDataType::Int4:   outFormat = ScriptVertexAttributeFormat::SInt32; outDimension = 4; return;
        case ShaderDataType::UByte4: outFormat = ScriptVertexAttributeFormat::UNorm8; outDimension = 4; return;
        case ShaderDataType::SByte:  outFormat = ScriptVertexAttributeFormat::SNorm8; outDimension = 1; return;
        case ShaderDataType::SByte2: outFormat = ScriptVertexAttributeFormat::SNorm8; outDimension = 2; return;
        case ShaderDataType::SByte3: outFormat = ScriptVertexAttributeFormat::SNorm8; outDimension = 3; return;
        case ShaderDataType::SByte4: outFormat = ScriptVertexAttributeFormat::SNorm8; outDimension = 4; return;
        case ShaderDataType::Color:  outFormat = ScriptVertexAttributeFormat::UNorm8; outDimension = 4; return;
        default: outFormat = ScriptVertexAttributeFormat::Float32; outDimension = 3; return;
        }
    }

    void ScriptMesh::Internal_SetVertexBufferParams(ScriptMesh* thisPtr, uint32_t vertexCount, MonoArray* layout)
    {
        if (!layout)
            return;

        const uint32_t numDescs = (uint32_t)mono_array_length(layout);
        BufferLayout bufferLayout;
        for (uint32_t i = 0; i < numDescs; i++)
        {
            ScriptVertexAttributeDescriptor desc = mono_array_get(layout, ScriptVertexAttributeDescriptor, i);
            ShaderDataType dataType = DescriptorToShaderDataType(desc.Format, desc.Dimension);
            BufferElement element(dataType, desc.Attribute);
            element.StreamIdx = (uint32_t)desc.Stream;
            bufferLayout.AddBufferElement(element);
        }

        Ref<MeshData> meshData = MeshData::Create(vertexCount, 0, bufferLayout);
        thisPtr->GetHandle()->SetMeshData(meshData);
    }

    void ScriptMesh::Internal_SetVertexBufferData(ScriptMesh* thisPtr, void* data, uint32_t meshBufferStart, uint32_t count, uint32_t stride)
    {
        if (!data)
            return;
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData)
            return;

        const uint32_t meshStride = meshData->GetBufferLayout().GetStride();
        uint8_t* dst = meshData->GetVertexBufferData() + meshBufferStart * meshStride;
        if (stride == meshStride)
        {
            std::memcpy(dst, data, count * stride);
        }
        else
        {
            const uint32_t copySize = std::min(stride, meshStride);
            const uint8_t* src = static_cast<const uint8_t*>(data);
            for (uint32_t i = 0; i < count; i++)
            {
                std::memcpy(dst + i * meshStride, src + i * stride, copySize);
            }
        }
    }

    void ScriptMesh::Internal_GetVertexBufferData(ScriptMesh* thisPtr, void* outData, uint32_t count, uint32_t stride)
    {
        if (!outData)
            return;
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData)
            return;

        const uint32_t meshStride = meshData->GetBufferLayout().GetStride();
        const uint32_t copyCount = std::min(count, meshData->GetVertexCount());
        const uint8_t* src = meshData->GetVertexBufferData();
        if (stride == meshStride)
        {
            std::memcpy(outData, src, copyCount * stride);
        }
        else
        {
            const uint32_t copySize = std::min(stride, meshStride);
            uint8_t* dst = static_cast<uint8_t*>(outData);
            for (uint32_t i = 0; i < copyCount; i++)
            {
                std::memcpy(dst + i * stride, src + i * meshStride, copySize);
            }
        }
    }

    uint32_t ScriptMesh::Internal_GetVertexStride(ScriptMesh* thisPtr)
    {
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData)
            return 0;
        return meshData->GetBufferLayout().GetStride();
    }

    uint32_t ScriptMesh::Internal_GetVertexAttributeCount(ScriptMesh* thisPtr)
    {
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData)
            return 0;
        return (uint32_t)meshData->GetBufferLayout().GetElements().size();
    }

    bool ScriptMesh::Internal_HasVertexAttribute(ScriptMesh* thisPtr, VertexAttribute attr)
    {
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData)
            return false;
        return meshData->GetBufferLayout().HasAttribute(attr);
    }

    void ScriptMesh::Internal_GetVertexAttribute(ScriptMesh* thisPtr, int32_t index, ScriptVertexAttributeDescriptor* outDesc)
    {
        const Ref<MeshData> meshData = thisPtr->GetHandle()->GetMeshData();
        if (!meshData || !outDesc)
            return;

        const auto& elements = meshData->GetBufferLayout().GetElements();
        if (index < 0 || index >= (int32_t)elements.size())
            return;

        const BufferElement& elem = elements[index];
        outDesc->Attribute = elem.Attribute;
        outDesc->Stream = (int32_t)elem.StreamIdx;
        ShaderDataTypeToFormatAndDimension(elem.Type, outDesc->Format, outDesc->Dimension);
    }

    MonoObject* ScriptMesh::Internal_CreatePlane(float width, float height, uint32_t subdivisionsX, uint32_t subdivisionsY)
    {
        return CreateManagedMesh(MeshFactory::CreatePlane(width, height, glm::vec3(0.0f, 1.0f, 0.0f), subdivisionsX,
                                                          subdivisionsY, MeshUsage::CpuCached));
    }

    MonoObject* ScriptMesh::Internal_CreateBox(glm::vec3* dimensions)
    {
        return dimensions != nullptr ? CreateManagedMesh(MeshFactory::CreateBox(*dimensions, MeshUsage::CpuCached)) : nullptr;
    }

    MonoObject* ScriptMesh::Internal_CreateCube(float size)
    {
        return CreateManagedMesh(MeshFactory::CreateCube(size, MeshUsage::CpuCached));
    }

    MonoObject* ScriptMesh::Internal_CreateSphere(float radius, uint32_t segments, uint32_t rings)
    {
        return CreateManagedMesh(MeshFactory::CreateSphere(radius, segments, rings, MeshUsage::CpuCached));
    }

    MonoObject* ScriptMesh::Internal_CreateCylinder(float radius, float height, uint32_t segments, bool capped)
    {
        return CreateManagedMesh(MeshFactory::CreateCylinder(radius, height, segments, capped, MeshUsage::CpuCached));
    }

    MonoObject* ScriptMesh::Internal_CreateCone(float radius, float height, uint32_t segments, bool capped)
    {
        return CreateManagedMesh(MeshFactory::CreateCone(radius, height, segments, capped, MeshUsage::CpuCached));
    }

    MonoObject* ScriptMesh::Internal_CreateCapsule(float radius, float height, uint32_t segments, uint32_t hemisphereRings)
    {
        return CreateManagedMesh(MeshFactory::CreateCapsule(radius, height, segments, hemisphereRings, MeshUsage::CpuCached));
    }

} // namespace Crowny
