#pragma once

#include "Crowny/Scene/Scene.h"

#include "Crowny/Renderer/EditorCamera.h"

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
    private:
        void Render(const Camera& camera, const glm::mat4& viewTransform);

    private:
        Ref<RenderTarget> m_RenderTarget;
        Ref<Scene> m_Scene;
        Ref<CommandBuffer> m_CommandBuffer;
    };

} // namespace Crowny