#include "cwpch.h"

#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/RenderCommand.h"

namespace Crowny
{

	Mesh::Mesh(const Ref<VertexArray>& vao, const Ref<IndexBuffer>& ibo, const Ref<MaterialInstance>& material, const std::vector<Ref<Texture2D>>& textures) 
								: m_VertexArray(vao), m_IndexBuffer(ibo), m_MaterialInstance(material), m_Textures(textures)
	{

	}

	void Mesh::Draw() // this should prob not be here
	{
		Renderer::SubmitCommand([]() { m_VertexArray->Bind(); });
		Renderer::SubmitCommand([]() { RenderCommand::DrawIndexed(m_VertexArray); });
		Renderer::SubmitCommand([]() { m_VertexArray->Unbind(); });
		Renderer::SubmitCommand([]() { m_MaterialInstance->Unbind(); });
	}

}