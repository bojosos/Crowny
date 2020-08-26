#include "cwpch.h"

#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Renderer/VertexArray.h"

#include <glm/ext/matrix_transform.inl>

namespace Crowny
{
	Ref<Mesh> CreatePlane(float width, float height, const glm::vec3& normal, const Ref<MaterialInstance>& material)
	{
		glm::vec3 v = normal * 90.f;

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), v.z, glm::vec3(1, 0, 0)) * glm::rotate(glm::mat4(1.0f), v.y, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), v.x, glm::vec3(0, 0, 1));

		Vertex data[4];

		data[0].Position = rotation * glm::vec4(-width / 2.0f, 0.0f, -height / 2.0f, 1.0f);
		data[0].Normal = normal;
		data[0].Uv = glm::vec2(0.0f, 0.0f);
		data[0].Binormal = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 1, 0)) * glm::vec4(normal, 1.0f);
		data[0].Tangent = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 0, 1)) * glm::vec4(normal, 1.0f);

		data[1].Position = rotation * glm::vec4(-width / 2.0f, 0.0f, height / 2.0f, 1.0f);
		data[1].Normal = normal;
		data[1].Uv = glm::vec2(0.0f, 1.0f);
		data[1].Binormal = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 1, 0)) * glm::vec4(normal, 1.0f);
		data[1].Tangent = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 0, 1)) * glm::vec4(normal, 1.0f);

		data[2].Position = rotation * glm::vec4(width / 2.0f, 0.0f, height / 2.0f, 1.0f);
		data[2].Normal = normal;
		data[2].Uv = glm::vec2(1.0f, 1.0f);
		data[2].Binormal = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 1, 0)) * glm::vec4(normal, 1.0f);
		data[2].Tangent = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 0, 1)) * glm::vec4(normal, 1.0f);

		data[3].Position = rotation * glm::vec4(width / 2.0f, 0.0f, -height / 2.0f, 1.0f);
		data[3].Normal = normal;
		data[3].Uv = glm::vec2(1.0f, 0.0f);
		data[3].Binormal = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 1, 0)) * glm::vec4(normal, 1.0f);
		data[3].Tangent = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0, 0, 1)) * glm::vec4(normal, 1.0f);

		Ref<VertexBuffer> vbo = VertexBuffer::Create(data, 4);
		vbo->SetLayout({
						{ ShaderDataType::Float3, "Position" },
						{ ShaderDataType::Float3, "Normal" },
						{ ShaderDataType::Float2, "TextureCoord" },
						{ ShaderDataType::Float3, "Binormal" },
						{ ShaderDataType::Float3, "Tangent" }
			});

		Ref<VertexArray> vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);

		uint32_t* indices = new uint32_t[6]
		{
			0,1,2,
			2,3,0
		};

		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices, 6);
		vao->SetIndexBuffer(ibo);
		return CreateRef<Mesh>(vao, ibo, material);

	}

	Ref<Mesh> CreateCube(float size, const Ref<MaterialInstance>& material)
	{
		return nullptr;
	}
}