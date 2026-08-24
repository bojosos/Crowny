#include "cwpch.h"

#include "Crowny/Renderer/MeshProcessing.h"

#include <meshoptimizer.h>

namespace Crowny
{
    namespace
    {
        void BuildMeshlets(MeshGpuGeometry& result, MeshLod& lod, const Vector<uint32_t>& indices, uint32_t materialSlot,
                           float lodError, const Vector<glm::vec3>& positions, const MeshProcessingSettings& settings)
        {
            if (indices.empty())
                return;

            const size_t bound = meshopt_buildMeshletsBound(indices.size(), settings.MeshletMaxVertices, settings.MeshletMaxTriangles);
            Vector<meshopt_Meshlet> meshlets(bound);
            Vector<uint32_t> vertices(indices.size());
            Vector<uint8_t> triangles(indices.size());
            const size_t meshletCount =
              meshopt_buildMeshlets(meshlets.data(), vertices.data(), triangles.data(), indices.data(), indices.size(), &positions[0].x,
                                    positions.size(), sizeof(glm::vec3), settings.MeshletMaxVertices, settings.MeshletMaxTriangles,
                                    settings.MeshletConeWeight);
            meshlets.resize(meshletCount);
            if (meshlets.empty())
                return;

            const meshopt_Meshlet& last = meshlets.back();
            vertices.resize(last.vertex_offset + last.vertex_count);
            triangles.resize(last.triangle_offset + last.triangle_count * 3u);

            const uint32_t vertexBase = static_cast<uint32_t>(result.MeshletVertices.size());
            const uint32_t triangleBase = static_cast<uint32_t>(result.MeshletTriangles.size());
            result.MeshletVertices.insert(result.MeshletVertices.end(), vertices.begin(), vertices.end());
            result.MeshletTriangles.insert(result.MeshletTriangles.end(), triangles.begin(), triangles.end());
            result.MeshletIndices.resize(triangleBase + triangles.size());

            for (const meshopt_Meshlet& source : meshlets)
            {
                const meshopt_Bounds bounds =
                  meshopt_computeMeshletBounds(vertices.data() + source.vertex_offset, triangles.data() + source.triangle_offset,
                                               source.triangle_count, &positions[0].x, positions.size(), sizeof(glm::vec3));
                Meshlet meshlet;
                meshlet.VertexOffset = vertexBase + source.vertex_offset;
                meshlet.TriangleOffset = triangleBase + source.triangle_offset;
                meshlet.VertexCount = source.vertex_count;
                meshlet.TriangleCount = source.triangle_count;
                meshlet.MaterialSlot = materialSlot;
                meshlet.LodError = lodError;
                meshlet.BoundingSphere = { bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius };
                meshlet.NormalCone = { bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2], bounds.cone_cutoff };
                for (uint32_t index = 0; index < source.triangle_count * 3u; index++)
                {
                    const uint32_t localVertex = triangles[source.triangle_offset + index];
                    result.MeshletIndices[triangleBase + source.triangle_offset + index] =
                      vertices[source.vertex_offset + localVertex];
                }
                result.Meshlets.push_back(meshlet);
                lod.MeshletCount++;
            }
        }
    } // namespace

    MeshGpuGeometry MeshProcessing::BuildGpuGeometry(const MeshData& meshData, const Vector<SubMesh>& sourceSubMeshes,
                                                     const MeshProcessingSettings& inputSettings)
    {
        MeshGpuGeometry result;
        if (meshData.GetVertexCount() == 0 || meshData.GetIndexCount() < 3 ||
            !meshData.GetBufferLayout().HasAttribute(VertexAttribute::Position))
            return result;

        MeshProcessingSettings settings = inputSettings;
        settings.LodCount = std::clamp(settings.LodCount, 1u, 16u);
        settings.MeshletMaxVertices = std::clamp(settings.MeshletMaxVertices, 3u, 64u);
        settings.MeshletMaxTriangles = std::clamp(settings.MeshletMaxTriangles, 1u, 64u);

        const Vector<glm::vec3> positions = meshData.GetPositions();
        const Vector<uint32_t> sourceIndices = meshData.GetIndices();
        const Vector<SubMesh> subMeshes =
          sourceSubMeshes.empty() ? Vector<SubMesh>{ SubMesh(0, meshData.GetIndexCount(), DrawMode::TRIANGLE_LIST) } : sourceSubMeshes;
        const float simplifyScale = meshopt_simplifyScale(&positions[0].x, positions.size(), sizeof(glm::vec3));

        uint32_t previousIndexCount = std::numeric_limits<uint32_t>::max();
        for (uint32_t lodIndex = 0; lodIndex < settings.LodCount; lodIndex++)
        {
            MeshLod lod;
            lod.FirstSubMesh = static_cast<uint32_t>(result.LodSubMeshes.size());
            lod.FirstMeshlet = static_cast<uint32_t>(result.Meshlets.size());
            const size_t lodIndexStart = result.LodIndices.size();
            const size_t meshletVertexStart = result.MeshletVertices.size();
            const size_t meshletTriangleStart = result.MeshletTriangles.size();
            const size_t meshletIndexStart = result.MeshletIndices.size();
            uint32_t lodIndexCount = 0;

            for (uint32_t materialSlot = 0; materialSlot < subMeshes.size(); materialSlot++)
            {
                const SubMesh& subMesh = subMeshes[materialSlot];
                if (subMesh.MeshDrawMode != DrawMode::TRIANGLE_LIST || subMesh.IndexCount < 3 ||
                    subMesh.IndexOffset + subMesh.IndexCount > sourceIndices.size())
                    continue;

                Vector<uint32_t> indices(sourceIndices.begin() + subMesh.IndexOffset,
                                         sourceIndices.begin() + subMesh.IndexOffset + subMesh.IndexCount);
                float resultError = 0.0f;
                if (lodIndex > 0)
                {
                    size_t targetCount = std::max<size_t>(3, (indices.size() >> lodIndex) / 3u * 3u);
                    Vector<uint32_t> simplified(indices.size());
                    const size_t simplifiedCount =
                      meshopt_simplify(simplified.data(), indices.data(), indices.size(), &positions[0].x, positions.size(), sizeof(glm::vec3),
                                       targetCount, settings.LodTargetError, meshopt_SimplifyLockBorder, &resultError);
                    simplified.resize(simplifiedCount);
                    if (simplified.size() >= 3)
                        indices = std::move(simplified);
                }

                meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), positions.size());
                meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &positions[0].x, positions.size(), sizeof(glm::vec3), 1.05f);

                MeshLodSubMesh lodSubMesh;
                lodSubMesh.IndexOffset = static_cast<uint32_t>(result.LodIndices.size());
                lodSubMesh.IndexCount = static_cast<uint32_t>(indices.size());
                lodSubMesh.MaterialSlot = materialSlot;
                result.LodIndices.insert(result.LodIndices.end(), indices.begin(), indices.end());
                result.LodSubMeshes.push_back(lodSubMesh);
                lod.SubMeshCount++;
                lodIndexCount += lodSubMesh.IndexCount;
                lod.Error = std::max(lod.Error, resultError * simplifyScale);
                if (settings.GenerateMeshlets)
                    BuildMeshlets(result, lod, indices, materialSlot, lod.Error, positions, settings);
            }

            if (lod.SubMeshCount == 0 || (lodIndex > 0 && lodIndexCount >= previousIndexCount))
            {
                result.LodIndices.resize(lodIndexStart);
                result.LodSubMeshes.resize(lod.FirstSubMesh);
                result.Meshlets.resize(lod.FirstMeshlet);
                result.MeshletVertices.resize(meshletVertexStart);
                result.MeshletTriangles.resize(meshletTriangleStart);
                result.MeshletIndices.resize(meshletIndexStart);
                break;
            }

            previousIndexCount = lodIndexCount;
            result.Lods.push_back(lod);
        }

        return result;
    }

} // namespace Crowny
