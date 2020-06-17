#include "cwpch.h"
#include "mesh.h"

namespace Crowny
{
	Mesh::Mesh(const Ref<VertexArray>& vao, const Ref<Shader>& shader) : m_VertexArray(vao), m_Shader(shader)
	{

	}

	Mesh::~Mesh()
	{

	}

	Ref<Mesh> Mesh::Create(const Ref<VertexArray>& vao, const Ref<Shader>& shader)
	{
		return CreateRef<Mesh>(vao, shader);
	}

	void Mesh::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		m_Shader->UploadUniformMat4(name, matrix);
	}
}