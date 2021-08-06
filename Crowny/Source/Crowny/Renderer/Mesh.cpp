#include "cwpch.h"

#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Renderer.h"

namespace Crowny
{

    Mesh::Mesh(const Ref<VertexArray>& vao, const Ref<IndexBuffer>& ibo, const Ref<MaterialInstance>& material,
               const std::vector<Ref<Texture>>& textures)
      : m_VertexArray(vao), m_IndexBuffer(ibo), m_MaterialInstance(material), m_Textures(textures)
    {
    }

    void Mesh::Draw() // this should prob not be here
    {
        m_VertexArray->Bind();
        RenderCommand::DrawIndexed(m_VertexArray);
        m_VertexArray->Unbind();
        m_MaterialInstance->Unbind();
    }

} // namespace Crowny