#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class IDBufferRenderer
    {
    public:
        static void Init();
        static void Begin(const glm::mat4& projection, const glm::mat4& view);
        static void DrawQuad(const glm::mat4& transform, uint32_t entityId);
        static void DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityId);
        static void OnResize(float width, float height);
        static void End();
        static int32_t ReadPixel(int32_t x, int32_t y);
    public:
        struct IDBufferData
        {
            glm::vec4 Position;
            int32_t ObjectID;
        };
    };
}