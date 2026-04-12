#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Mesh.h"

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
        static void Submit(const AssetHandle<Mesh>& mesh, const Vector<AssetHandle<Material>>& materials, const glm::mat4& transform);
        static void SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform);
        static void SubmitLightSetup();
        static void EndScene();
        static void End();
        static void Flush();
        static void Shutdown();
    };
} // namespace Crowny
