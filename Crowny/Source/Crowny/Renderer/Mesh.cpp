#include "cwpch.h"

#include "Crowny/Renderer/Mesh.h"

#include "Crowny/Renderer/Renderer.h"

namespace Crowny
{

	Mesh::Mesh(const Ref<VertexArray>& vao, const Ref<IndexBuffer>& ibo, const Ref<MaterialInstance>& material) : m_VertexArray(vao), m_IndexBuffer(ibo), m_MaterialInstance(material)
	{

	}

}