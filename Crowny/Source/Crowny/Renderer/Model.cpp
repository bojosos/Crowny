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
		//const aiScene* scene = importer.ReadFileFromMemory(data, size, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
		const aiScene* scene = nullptr;
		std::string outstr;
		if (VirtualFileSystem::Get()->ResolvePhyiscalPath(filepath, outstr))
			scene = importer.ReadFile(outstr, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
		CW_ENGINE_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode, "Error loading scene");
		
		m_Directory = filepath.substr(0, filepath.find_last_of('/'));
		ProcessNode(scene->mRootNode, scene);
		delete data;
	}

	void Model::Draw()
	{
		for (uint32_t i = 0; i < m_Meshes.size(); i++)
		{
			if (m_Meshes[i])
				m_Meshes[i]->Draw();
		}	
	}

	void Model::ProcessNode(aiNode* node, const aiScene* scene)
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
			verts[i].Position = glm::vec3{ mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
			verts[i].Normal = glm::vec3{ mesh->mNormals[i].x	, mesh->mNormals[i].y, mesh->mNormals[i].z };

			if (mesh->mTextureCoords[0])
			{
				verts[i].Uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			}
			else
				verts[i].Uv = glm::vec2(0.0f, 0.0f);

			verts[i].Binormal = glm::vec3{ mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
			verts[i].Tangent = glm::vec3{ mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
		}

		Ref<VertexBuffer> vbo = VertexBuffer::Create(sizeof(Vertex) * mesh->mNumVertices);
		void* dest = vbo->Map(0, sizeof(Vertex) * mesh->mNumVertices, GpuLockOptions::WRITE_DISCARD);
		memcpy(dest, verts, sizeof(Vertex) * mesh->mNumVertices);
		vbo->Unbind();
		vbo->SetLayout({ {ShaderDataType::Float3, "Position"},
						 {ShaderDataType::Float3, "Normal"},
						 {ShaderDataType::Float2, "Uv"},
						 {ShaderDataType::Float3, "Tangent"},
						 {ShaderDataType::Float3, "Binormal"}
					   });
		/*vbo->SetLayout({ {ShaderDataType::Float3, "Position"},
			{ShaderDataType::Float2, "Uv"},
			{ShaderDataType::Float3, "Normal"},
		});*/
		delete[] verts;

		//TODO: Optimize
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
		std::vector<Ref<Texture2D>> textures;
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Ref<Texture2D>> diffuseMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			std::vector<Ref<Texture2D>> specularMaps = this->LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
			std::vector<Ref<Texture2D>> normalMaps = this->LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
			textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
			std::vector<Ref<Texture2D>> heightMaps = this->LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
			textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
		}

		return CreateRef<Mesh>(vao, ibo, nullptr, textures);
	}

	std::vector<Ref<Texture2D>> Model::LoadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName)
	{
		std::vector<Ref<Texture2D>> textures;
		textures.resize(material->GetTextureCount(type));
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		for (uint32_t i = 0; i < material->GetTextureCount(type); i++)
		{
			aiString str;
			material->GetTexture(type, i, &str);
			bool skip = false;
			for (uint32_t j = 0; j < m_TexturesLoaded.size(); j++)
			{
				if (m_TexturesLoaded[j]->GetFilepath() == std::string(m_Directory + "/" + str.C_Str()))
				{
					textures.push_back(m_TexturesLoaded[j]);
					skip = true;
					break;
				}
			}
			if (!skip)
			{
				Ref<Texture2D> texture = Texture2D::Create(m_Directory + "/" + str.C_Str(), {}, str.C_Str());
				textures[i] = texture;
				m_TexturesLoaded.push_back(texture);
			}
		}

		return textures;
	}

}
