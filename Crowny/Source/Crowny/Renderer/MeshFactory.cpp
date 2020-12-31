#include "cwpch.h"

#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Renderer/VertexArray.h"

#include <glm/ext/matrix_transform.inl>

namespace Crowny
{
	Ref<Mesh> MeshFactory::CreatePlane(float width, float height, const glm::vec3& normal, const Ref<MaterialInstance>& material)
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

	Ref<Mesh> MeshFactory::CreateCube(float size, const Ref<MaterialInstance>& material)
	{
		return nullptr;
	}

	Ref<Mesh> MeshFactory::CreateSphere(float xSegments, float ySegments)
	{
		Ref<VertexArray> vao = VertexArray::Create(DrawMode::TRIANGLE_STRIP);
		
		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uv;
		std::vector<glm::vec3> normals;
		std::vector<uint32_t> indices;

		const float PI = 3.14159265359;
		for (uint32_t y = 0; y <= ySegments; y++)
		{
			for (uint32_t x = 0; x <= xSegments; x++)
			{
				float xSegment = (float)x / (float)xSegments;
				float ySegment = (float)y / (float)ySegments;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

				positions.push_back(glm::vec3(xPos, yPos, zPos));
				uv.push_back(glm::vec2(xSegment, ySegment));
				normals.push_back(glm::vec3(xPos, yPos, zPos));
			}
		}

		bool oddRow = false;
		for (uint32_t y = 0; y < ySegments; y++)
		{
			if (!oddRow) // even rows: y == 0, y == 2; and so on
			{
				for (uint32_t x = 0; x <= xSegments; x++)
				{
					indices.push_back(y * (xSegments + 1) + x);
					indices.push_back((y + 1) * (xSegments + 1) + x);
				}
			}
			else
			{
				for (int32_t x = xSegments; x >= 0; x--)
				{
					indices.push_back((y + 1) * (xSegments + 1) + x);
					indices.push_back(y * (xSegments + 1) + x);
				}
			}
			oddRow = !oddRow;
		}

		std::vector<float> data;
		for (std::size_t i = 0; i < positions.size(); i++)
		{
			data.push_back(positions[i].x);
			data.push_back(positions[i].y);
			data.push_back(positions[i].z);
			if (!uv.empty())
			{
				data.push_back(uv[i].x);
				data.push_back(uv[i].y);
			}
			if (!normals.empty())
			{
				data.push_back(normals[i].x);
				data.push_back(normals[i].y);
				data.push_back(normals[i].z);
			}
		}
		
		Ref<VertexBuffer> vbo = VertexBuffer::Create(data.data(), data.size() * sizeof(float));
		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices.data(), indices.size());
		vbo->SetLayout({
					   { ShaderDataType::Float3, "Position" },
			           { ShaderDataType::Float2, "Uv" },
					   { ShaderDataType::Float3, "Normals" }
					  });
		vao->AddVertexBuffer(vbo);
		vao->SetIndexBuffer(ibo);
		return CreateRef<Mesh>(vao, ibo);
	}
}