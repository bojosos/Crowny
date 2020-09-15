#pragma once

#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/VertexArray.h"

namespace Crowny
{

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 Uv;
		glm::vec3 Binormal;
		glm::vec3 Tangent;
	};

	class Mesh
	{
	private:
		Ref<VertexArray> m_VertexArray;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<MaterialInstance> m_MaterialInstance;


	public:
		Mesh(const Ref<VertexArray>& vao, const Ref<IndexBuffer>& ibo, const Ref<MaterialInstance>& material = nullptr);
		~Mesh() = default;

		Ref<VertexArray> GetVertexArray() { return m_VertexArray; }
		Ref<MaterialInstance> GetMaterialInstance() { return m_MaterialInstance; }
		void SetMaterialInstnace(const Ref<MaterialInstance>& materialInstance) { m_MaterialInstance = materialInstance; }

	};
}