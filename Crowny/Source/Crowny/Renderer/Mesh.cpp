#include "cwpch.h"

#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Renderer.h"

namespace Crowny
{

    Mesh::Mesh(const Ref<VertexBuffer>& vbo, const Ref<IndexBuffer>& ibo) : m_VertexBuffer(vbo), m_IndexBuffer(ibo) {}

    void Mesh::Draw()
    {
        auto& rapi = RenderAPI::Get();
        rapi.SetVertexBuffers(0, &m_VertexBuffer, 1);
        rapi.SetIndexBuffer(m_IndexBuffer);
        rapi.DrawIndexed(0, m_IndexBuffer->GetCount(), 0, 1);
    }

} // namespace Crowny