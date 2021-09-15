#pragma once

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 Uv;
        glm::vec3 Tangent;
    };

    class Mesh
    {

    public:
        Mesh(const Ref<VertexBuffer>& vbo, const Ref<IndexBuffer>& ibo);
        ~Mesh() = default;

        void Draw();
        Ref<MaterialInstance> GetMaterialInstance() { return m_MaterialInstance; }
        void SetMaterialInstance(const Ref<MaterialInstance>& materialInstance)
        {
            m_MaterialInstance = materialInstance;
        }

    private:
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<MaterialInstance> m_MaterialInstance;
    };
} // namespace Crowny
