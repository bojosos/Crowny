#pragma once

#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Model.h"
#include "Crowny/Renderer/EditorCamera.h"

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
        
        static std::vector<MeshTran> s_Meshes;
    };
}