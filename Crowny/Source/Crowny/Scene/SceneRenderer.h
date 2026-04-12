#pragma once

#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{

    class SceneRenderer
    {
    public:
        SceneRenderer(const Ref<Scene>& scene, const Ref<RenderTarget>& renderTarget);

        void Init();
        void RenderEditor(const EditorCamera& camera);
        void Render();
        void SetRenderTarget(const Ref<RenderTarget>& renderTarget);
        void SetScene(const Ref<Scene>& scene);

        // Phase 0: Snapshot-based rendering (decouples scene traversal from GPU commands)
        RenderSnapshot ExtractSnapshot(const Camera& camera, const glm::mat4& viewTransform) const;
        RenderSnapshot ExtractSnapshot() const; // Uses scene's primary camera
        static void RenderFromSnapshot(const RenderSnapshot& snapshot);

        // Evaluates all ProceduralMeshComponents that need rebuilding (call on sim thread before ExtractSnapshot)
        void UpdateProceduralMeshes();

        static void DrawGrid(const glm::mat4& viewProjection, const glm::vec3& cameraPos);

    private:
        void Render(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid = false);

    private:
        Ref<RenderTarget> m_RenderTarget;
        Ref<Scene> m_Scene;
        Ref<CommandBuffer> m_CommandBuffer;
    };

} // namespace Crowny