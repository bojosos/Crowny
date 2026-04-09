#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    class ScriptMesh : public TScriptAsset<ScriptMesh, Mesh>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Mesh");
        ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh);

    private:
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
        static void Internal_UploadMeshData(ScriptMesh* thisPtr);
        static void Internal_Clear(ScriptMesh* thisPtr);
        static void Internal_GetBoundsMin(ScriptMesh* thisPtr, glm::vec3* outMin);
        static void Internal_GetBoundsMax(ScriptMesh* thisPtr, glm::vec3* outMax);
    };

} // namespace Crowny
