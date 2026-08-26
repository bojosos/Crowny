#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    class EnvironmentMap;

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
        static void Submit(const AssetHandle<Mesh>& mesh, const Vector<AssetHandle<Material>>& materials, const glm::mat4& transform);
        static void SubmitForwardOnlyOpaque(const AssetHandle<Mesh>& mesh, const Vector<AssetHandle<Material>>& materials,
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
