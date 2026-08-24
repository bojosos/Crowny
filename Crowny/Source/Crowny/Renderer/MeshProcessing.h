#pragma once

#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    struct MeshProcessingSettings
    {
        uint32_t LodCount = 4;
        uint32_t MeshletMaxVertices = 64;
        uint32_t MeshletMaxTriangles = 64;
        float LodTargetError = 0.01f;
        float MeshletConeWeight = 0.5f;
        bool GenerateMeshlets = true;
    };

    class MeshProcessing
    {
    public:
        static MeshGpuGeometry BuildGpuGeometry(const MeshData& meshData, const Vector<SubMesh>& subMeshes,
                                                const MeshProcessingSettings& settings = {});
    };

} // namespace Crowny
