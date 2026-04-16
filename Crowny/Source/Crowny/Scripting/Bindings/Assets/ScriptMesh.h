#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    // Must match C# VertexAttributeFormat enum
    enum class ScriptVertexAttributeFormat : int32_t
    {
        Float32 = 1,
        Float16 = 2,
        UNorm8 = 3,
        SNorm8 = 4,
        UInt8 = 5,
        SInt8 = 6,
        UInt16 = 7,
        SInt16 = 8,
        UInt32 = 9,
        SInt32 = 10
    };

    // Must match C# VertexAttributeDescriptor struct layout
    struct ScriptVertexAttributeDescriptor
    {
        VertexAttribute Attribute;
        ScriptVertexAttributeFormat Format;
        int32_t Dimension;
        int32_t Stream;
    };

    class ScriptMesh : public TScriptAsset<ScriptMesh, Mesh>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Mesh");
        ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh);

    private:
        static ShaderDataType DescriptorToShaderDataType(ScriptVertexAttributeFormat format, int32_t dimension);

        static uint32_t Internal_GetVertexCount(ScriptMesh* thisPtr);
        static uint32_t Internal_GetIndexCount(ScriptMesh* thisPtr);
        static void Internal_GetVertices(ScriptMesh* thisPtr, MonoArray** outArray);
        static void Internal_SetVertices(ScriptMesh* thisPtr, MonoArray* array);
        static void Internal_GetNormals(ScriptMesh* thisPtr, MonoArray** outArray);
        static void Internal_SetNormals(ScriptMesh* thisPtr, MonoArray* array);
        static void Internal_GetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray** outArray);
        static void Internal_SetUVs(ScriptMesh* thisPtr, uint32_t channel, MonoArray* array);
        static void Internal_GetColors(ScriptMesh* thisPtr, MonoArray** outArray);
        static void Internal_SetColors(ScriptMesh* thisPtr, MonoArray* array);
        static void Internal_GetIndices(ScriptMesh* thisPtr, MonoArray** outArray);
        static void Internal_SetIndices(ScriptMesh* thisPtr, MonoArray* array);
        static void Internal_RecalculateBounds(ScriptMesh* thisPtr);
        static void Internal_RecalculateNormals(ScriptMesh* thisPtr);
        static void Internal_RecalculateTangents(ScriptMesh* thisPtr);
        static void Internal_UploadMeshData(ScriptMesh* thisPtr);
        static void Internal_Clear(ScriptMesh* thisPtr);
        static void Internal_GetBoundsMin(ScriptMesh* thisPtr, glm::vec3* outMin);
        static void Internal_GetBoundsMax(ScriptMesh* thisPtr, glm::vec3* outMax);
        static void Internal_SetVertexBufferParams(ScriptMesh* thisPtr, uint32_t vertexCount, MonoArray* layout);
        static void Internal_SetVertexBufferData(ScriptMesh* thisPtr, void* data, uint32_t meshBufferStart, uint32_t count, uint32_t stride);
        static void Internal_GetVertexBufferData(ScriptMesh* thisPtr, void* outData, uint32_t count, uint32_t stride);
        static uint32_t Internal_GetVertexStride(ScriptMesh* thisPtr);
        static uint32_t Internal_GetVertexAttributeCount(ScriptMesh* thisPtr);
        static bool Internal_HasVertexAttribute(ScriptMesh* thisPtr, VertexAttribute attr);
        static void Internal_GetVertexAttribute(ScriptMesh* thisPtr, int32_t index, ScriptVertexAttributeDescriptor* outDesc);
    };

} // namespace Crowny
