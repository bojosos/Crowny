#pragma once

#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/RenderWorld.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{

    struct SceneRenderStatistics
    {
        uint64_t FrameNumber = 0;
        uint64_t VisibleVertices = 0;
        uint64_t VisibleTriangles = 0;
        uint64_t UploadedBytes = 0;
        uint32_t VisibleInstances = 0;
        uint32_t ActiveInstances = 0;
        uint32_t ActiveLights = 0;
        uint32_t LogicalDraws = 0;
        uint32_t RenderPasses = 0;
        uint32_t GraphicsPasses = 0;
        uint32_t ComputePasses = 0;
        uint32_t TransferPasses = 0;
        uint32_t Barriers = 0;
        double RenderGraphCpuTimeMs = 0.0;
        bool RenderGraphSucceeded = false;
    };

    class SceneRenderer
    {
    public:
        SceneRenderer(const Ref<Scene>& scene, const Ref<RenderTarget>& renderTarget);
        ~SceneRenderer();

        void Init();
        void RenderEditor(const EditorCamera& camera, bool drawGrid = true, const GridSettings& gridSettings = {});
        void Render();
        void SetRenderTarget(const Ref<RenderTarget>& renderTarget);
        void SetScene(const Ref<Scene>& scene);

        // Phase 0: Snapshot-based rendering (decouples scene traversal from GPU commands)
        RenderSnapshot ExtractSnapshot(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid = false) const;
        RenderSnapshot ExtractSnapshot(bool drawGrid = false) const; // Uses scene's primary camera
        void ExtractSnapshot(RenderSnapshot& snapshot, const Camera& camera, const glm::mat4& viewTransform, bool drawGrid = false) const;
        void ExtractSnapshot(RenderSnapshot& snapshot, bool drawGrid = false) const;
        static void RenderFromSnapshot(const RenderSnapshot& snapshot);
        static SceneRenderStatistics GetStatistics();

        // Evaluates all ProceduralMeshComponents that need rebuilding (call on sim thread before ExtractSnapshot)
        void UpdateProceduralMeshes();
        void UpdateAnimations(Timestep timestep);

        static void DrawGrid(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const GridSettings& settings = {});

    private:
        void Render(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid = false, const GridSettings& gridSettings = {});
        static void RenderLegacySnapshot(const RenderSnapshot& snapshot);
        static void RenderLegacyOverlays(const RenderSnapshot& snapshot);
        void SyncRenderWorld(RenderSnapshot& snapshot) const;
        void ResetTrackedRenderWorld();

        struct TrackedRenderInstance
        {
            RenderInstanceHandle Handle;
            glm::mat4 Transform = glm::mat4(1.0f);
            glm::vec4 BoundingSphere = glm::vec4(0.0f);
            uint32_t MeshResourceIndex = 0;
            uint32_t MaterialResourceIndex = 0;
            uint32_t ObjectID = RenderObjectID::InvalidValue;
            RenderInstanceFlags Flags = RenderInstanceFlags::None;
            RenderLayerMask VisibilityLayers = RenderLayerMask::All();
            float LodBias = 0.0f;
        };

        struct TrackedRenderLight
        {
            RenderLightHandle Handle;
            RenderLightData Data;
            LightShadowSettings Shadows;
        };

        struct CameraHistoryState
        {
            glm::mat4 View = glm::mat4(1.0f);
            glm::mat4 Projection = glm::mat4(1.0f);
            glm::vec3 Position = glm::vec3(0.0f);
            glm::vec3 Forward = glm::vec3(0.0f, 0.0f, -1.0f);
        };

        struct MaterialSetKey
        {
            Vector<const AssetHandleData*> Materials;

            bool operator==(const MaterialSetKey& other) const { return Materials == other.Materials; }
        };

        struct MaterialSetKeyHash
        {
            size_t operator()(const MaterialSetKey& key) const
            {
                size_t hash = key.Materials.size();
                for (const AssetHandleData* material : key.Materials)
                {
                    const size_t value = std::hash<const AssetHandleData*>{}(material);
                    hash ^= value + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
                }
                return hash;
            }
        };

        struct TrackedMeshResource
        {
            AssetHandle<Mesh> Resource;
            uint64_t Version = 0;
        };

        struct TrackedMaterialResource
        {
            AssetHandle<Material> Resource;
            uint64_t Version = 0;
        };

        uint32_t GetResourceIndex(const AssetHandleData* identity, UnorderedMap<const AssetHandleData*, uint32_t>& resources,
                                  uint32_t& nextIndex) const;
        uint32_t GetMaterialSetIndex(const Vector<AssetHandle<Material>>& materials) const;
        void TrackMeshResource(uint32_t index, const AssetHandle<Mesh>& mesh, RenderSnapshot& snapshot) const;
        void TrackMaterialResources(uint32_t baseIndex, const Vector<AssetHandle<Material>>& materials,
                                    RenderSnapshot& snapshot) const;
        void FinalizeResourceChanges(RenderSnapshot& snapshot) const;

    private:
        Ref<RenderTarget> m_RenderTarget;
        Ref<Scene> m_Scene;
        Ref<CommandBuffer> m_CommandBuffer;
        mutable RenderWorld m_RenderWorld;
        mutable RenderLightWorld m_RenderLightWorld;
        mutable UnorderedMap<uint64_t, TrackedRenderInstance> m_TrackedRenderInstances;
        mutable UnorderedSet<uint64_t> m_SeenRenderInstances;
        mutable Vector<RenderWorldChange> m_RenderWorldChangeScratch;
        mutable UnorderedMap<uint64_t, TrackedRenderLight> m_TrackedRenderLights;
        mutable UnorderedSet<uint64_t> m_SeenRenderLights;
        mutable Vector<RenderLightChange> m_RenderLightChangeScratch;
        mutable UnorderedMap<const AssetHandleData*, uint32_t> m_MeshResourceIndices;
        mutable UnorderedMap<MaterialSetKey, uint32_t, MaterialSetKeyHash> m_MaterialSetIndices;
        mutable UnorderedMap<uint32_t, TrackedMeshResource> m_ResidentMeshResources;
        mutable UnorderedMap<uint32_t, TrackedMaterialResource> m_ResidentMaterialResources;
        mutable UnorderedSet<uint32_t> m_SeenMeshResources;
        mutable UnorderedSet<uint32_t> m_SeenMaterialResources;
        mutable uint32_t m_NextMeshResourceIndex = 1;
        mutable uint32_t m_NextMaterialResourceIndex = 1;
        mutable UnorderedMap<uint64_t, CameraHistoryState> m_CameraHistory;
    };

} // namespace Crowny
