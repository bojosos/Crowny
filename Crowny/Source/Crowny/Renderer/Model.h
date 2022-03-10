#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Crowny
{
    struct MeshRendererComponent;

    class Model
    {
    public:
        Model(const Path& filepath);
        void Draw();
        const Vector<Ref<Texture>>& GetTextures() const { return m_Textures; }
        const Vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; }

    private:
        void ProcessNode(aiNode* node, const aiScene* scene);
        Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
        Vector<Ref<Texture>> LoadMaterialTextures(aiMaterial* material, aiTextureType type, const String& typeName);

        struct Vertex
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec2 Uv;
            glm::vec3 Tangent;
            // glm::vec3 Binormal;
        };
        friend struct MeshRendererComponent;

        Vector<Ref<Mesh>> m_Meshes;
        Vector<Ref<Texture>> m_Textures;
        Vector<Ref<Texture>> m_TexturesLoaded;
        String m_Directory;
    };
} // namespace Crowny
