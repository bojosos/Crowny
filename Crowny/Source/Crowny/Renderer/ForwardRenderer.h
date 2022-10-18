#pragma once

namespace Crowny
{
    class ForwardRenderer
    {
    public:
        static void Init();
        static void Begin();
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void Submit(const Ref<Model>& model, const glm::mat4& transform);
        static void SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform);
        // static void SubmitLightSetup(const LightSetup& setup);
        static void SubmitLightSetup();
        static void EndScene();
        static void End();
        static void Flush();
        static void Shutdown();
    };
} // namespace Crowny