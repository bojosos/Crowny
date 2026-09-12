#pragma once

#include <span>

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    class EnvironmentMap;
    struct RenderSnapshot;
    struct DecalRenderStats;
    class GpuDecalWorld;
    enum class LegacySurfacePass : uint8_t
    {
        All,
        Opaque,
        Coating,
        Transparent
    };

    class ForwardRenderer
    {
    public:
        static void Init();
        static void Begin();
        static void BeginScene(const Camera& camera, const glm::mat4& transform, const Ref<EnvironmentMap>& environment = nullptr);
        static void BeginScene(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition,
                               const Ref<EnvironmentMap>& environment = nullptr);
        static void BeginForwardOnlyScene(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition,
                                          const Ref<EnvironmentMap>& environment = nullptr);
        static void PrepareDecals(const RenderSnapshot& snapshot, GpuDecalWorld* world = nullptr);
        static void ReleaseDecalView(uint64_t view);
        static const DecalRenderStats& GetDecalStatistics();
        static void SetSurfacePass(LegacySurfacePass pass);
        static void Submit(const AssetHandle<Mesh>& mesh, std::span<const AssetHandle<Material>> materials, const glm::mat4& transform,
                           uint32_t objectId = 0);
        static void SubmitForwardOnlyOpaque(const AssetHandle<Mesh>& mesh, std::span<const AssetHandle<Material>> materials,
                                            const glm::mat4& transform);
        static void SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform);
        static void SubmitLightSetup();
        static void SetLights(const RenderLightData* lights, uint32_t lightCount);
        static void EndScene();
        static void End();
        static void Flush();
        static void Shutdown();

        static void SetPolygonMode(PolygonMode mode);
    };
} // namespace Crowny
