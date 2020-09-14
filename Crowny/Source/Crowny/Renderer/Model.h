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
		void ProcessNode(aiNode* node, aiScene* scene);
		Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<Ref<Texture2D>> LoadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName);

		const std::vector<Ref<Texture2D>>& GetTextures() const { return m_Textures; }
		const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; }

	private:
		std::vector<Ref<Mesh>> m_Meshes;
		std::vector<Ref<Texture2D>> m_Textures;
		std::string m_Directory;
	};
}