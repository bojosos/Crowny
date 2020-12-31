#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Crowny
{

	class Model
	{
	public:
		Model(const std::string& filepath);
		void Draw();
		const std::vector<Ref<Texture2D>>& GetTextures() const { return m_Textures; }
		const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; }
	private:
		void ProcessNode(aiNode* node, const aiScene* scene);
		Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<Ref<Texture2D>> LoadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName);

		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
      glm::vec2 Uv;
			glm::vec3 Tangent;
			glm::vec3 Binormal;
		};

		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Ref<Texture2D>> m_Textures;
		std::vector<Ref<Texture2D>> m_TexturesLoaded;
		std::string m_Directory;
	};
}
