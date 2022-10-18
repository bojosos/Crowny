#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
    struct MeshTran
    {
        Ref<Mesh> mesh;
        glm::mat4 tran;
    };

    class ForwardPlusRenderer
    {
    public:
        static void Init();
        static void BeginFrame(const EditorCamera& camera);
        static void BeginFrame(const glm::mat4& projection, const glm::mat4& transform);
        static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform);
        static void Submit(const Ref<Model>& model, const glm::mat4& transform);
        static void EndFrame();
        static void Shutdown();

        static Vector<MeshTran> s_Meshes;
    };
} // namespace Crowny