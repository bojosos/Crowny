#include "cwpch.h"

#include "Crowny/Renderer/Model.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include <glm/glm.hpp>

namespace Crowny
{

	Model::Model(const std::string& filepath)
	{
		auto [data, size] = VirtualFileSystem::Get()->ReadFile(filepath);
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFileFromMemory(data, size, aiProcess_Triangulate | aiProcess_FlipUVs);
		CW_ENGINE_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode, "Error loading scene");
		
		m_Directory = filepath.substr(0, filepath.find_last_of('/'));
	}

	void Model::ProcessNode(aiNode* node, aiScene* scene)
	{
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Meshes.push_back(ProcessMesh(mesh, scene));
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene);
		}
	}

	Ref<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
	{
		Ref<VertexArray> vao = VertexArray::Create();
		
		Vertex* verts = new Vertex[mesh->mNumVertices];

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			verts[i].Position = glm::vec3{ mesh->mVertices->x, mesh->mVertices->y, mesh->mVertices->z };
			verts[i].Normal = glm::vec3{ mesh->mNormals->x, mesh->mNormals->y, mesh->mNormals->z };

			if (mesh->mTextureCoords[0])
			{
				verts[i].Uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			}
			else
				verts[i].Uv = glm::vec2(0.0f, 0.0f);
		}

		Ref<VertexBuffer> vbo = VertexBuffer::Create(verts, sizeof(Vertex) * mesh->mNumVertices);
		vbo->SetLayout({ {ShaderDataType::Float3, "Position"},
						 {ShaderDataType::Float2, "Uv"},
						 {ShaderDataType::Float3, "Normal"}
					   });

		//TODO: Optimize
		//std::vector<uint32_t> indices(mesh->mNumFaces * 3);
		std::vector<uint32_t> indices;

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
			{
				indices.push_back(face.mIndices[j]);
			}
		}

		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices.data(), indices.size());
		vao->AddVertexBuffer(vbo);
		vao->SetIndexBuffer(ibo);

		return CreateRef<Mesh>(vao, ibo);
	}

	std::vector<Ref<Texture2D>> Model::LoadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName)
	{
		std::vector<Ref<Texture2D>> textures(material->GetTextureCount(type));
		for (uint32_t i = 0; i < material->GetTextureCount(type); i++)
		{
			aiString str;
			material->GetTexture(type, i, &str);
			Ref<Texture2D> texture = Texture2D::Create(m_Directory + str.C_Str());
			textures[i] = texture;
		}

		return textures;
	}

}