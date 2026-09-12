#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Math/AABox.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class AssetManager;
    class Mesh;
    class MeshData;
    struct SubMesh;

    struct PhysicsMeshBuildSettings
    {
        // Upper bound on the point cloud handed to convex hull builders. Box3D and Jolt cap hulls at
        // 255/256 vertices and PhysX at 255, so larger clouds are decimated before cooking. 0 keeps every point.
        uint32_t MaxConvexPoints = 255;
        // Positions closer than this are welded. 0 welds only bit-identical positions.
        float WeldTolerance = 0.0f;
    };

    /**
     * Backend-neutral collision geometry cooked from a render mesh: welded positions, a clean triangle list
     * (no degenerate or out-of-range triangles) and a decimated point cloud for convex hulls. Imported as a
     * dependent of the source Mesh asset; the Mesh stores this asset's UUID so colliders can find it at runtime.
     */
    class PhysicsMesh : public Asset
    {
    public:
        PhysicsMesh() = default;

        AssetType GetAssetType() const override { return AssetType::PhysicsMesh; }
        static AssetType GetStaticType() { return AssetType::PhysicsMesh; }

        const Vector<glm::vec3>& GetPositions() const { return m_Positions; }
        const Vector<uint32_t>& GetIndices() const { return m_Indices; }
        /** Point cloud for hull building: the decimated set when one was produced, otherwise every position. */
        const Vector<glm::vec3>& GetConvexPoints() const { return m_ConvexPoints.empty() ? m_Positions : m_ConvexPoints; }
        const AABox& GetBounds() const { return m_Bounds; }
        uint32_t GetTriangleCount() const { return static_cast<uint32_t>(m_Indices.size() / 3); }
        uint32_t GetSourceVertexCount() const { return m_SourceVertexCount; }
        bool IsEmpty() const { return m_Positions.size() < 3 || m_Indices.size() < 3; }
        bool IsConvexCapable() const { return m_Positions.size() >= 4; }

        /** Unique undirected edges, built lazily. Used by editor overlays. */
        const Vector<glm::u32vec2>& GetEdges() const;

        void SetGeometry(Vector<glm::vec3> positions, Vector<uint32_t> indices, Vector<glm::vec3> convexPoints, uint32_t sourceVertexCount);

        /**
         * Fills a shape description with copies scaled by a (possibly negative, non-uniform) world scale.
         * Triangle winding is flipped for mirrored scales so outward faces stay outward. Convex mode copies the
         * hull point cloud and leaves the indices empty.
         */
        void CopyScaled(const glm::vec3& scale, bool convex, Vector<glm::vec3>& outVertices, Vector<uint32_t>& outIndices) const;

        /** Cooks collision geometry from mesh data. Only TRIANGLE_LIST sub-meshes contribute. Returns nullptr when nothing usable remains. */
        static Ref<PhysicsMesh> Build(const MeshData& meshData, const Vector<SubMesh>& subMeshes, const PhysicsMeshBuildSettings& settings = {});
        static Ref<PhysicsMesh> Build(const Vector<glm::vec3>& positions, const Vector<uint32_t>& indices,
                                      const PhysicsMeshBuildSettings& settings = {});

    private:
        CW_SERIALIZABLE(PhysicsMesh);
        friend class AssetManager;

        void RecalculateBounds();

        Vector<glm::vec3> m_Positions;
        Vector<uint32_t> m_Indices;
        Vector<glm::vec3> m_ConvexPoints;
        AABox m_Bounds;
        uint32_t m_SourceVertexCount = 0;
        mutable Vector<glm::u32vec2> m_Edges;
        mutable bool m_EdgesBuilt = false;
    };

    /** Registers an in-memory physics mesh with the asset manager so components and scenes can reference it by UUID. */
    AssetHandle<PhysicsMesh> CreateRuntimePhysicsMesh(AssetManager& assetManager, const Ref<PhysicsMesh>& physicsMesh);

    /**
     * Finds the collision geometry for a render mesh. Resolution order:
     *  1. an explicit registration made through Register() (primitives, tests, runtime-generated meshes),
     *  2. the PhysicsMesh asset referenced by the loaded Mesh (cooked at import),
     *  3. a transient PhysicsMesh built from the CPU copy of a CpuCached mesh (cached per GPU version).
     */
    class PhysicsMeshResolver
    {
    public:
        static void Register(const UUID& meshUuid, const AssetHandle<PhysicsMesh>& physicsMesh);
        static void Unregister(const UUID& meshUuid);
        static void Clear();

        static Ref<PhysicsMesh> Resolve(const AssetHandle<Mesh>& mesh, String* outError = nullptr);
        static bool CanResolve(const AssetHandle<Mesh>& mesh) { return Resolve(mesh) != nullptr; }
    };
} // namespace Crowny
