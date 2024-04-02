#include "cwpch.h"

#include "Crowny/Renderer/Model.h"

#include "Crowny/Common/VirtualFileSystem.h"

#include <glm/glm.hpp>

namespace Crowny
{

    Model::Model(const Path& filepath)
    {
        // auto [data, size] = VirtualFileSystem::Get()->ReadFile(filepath);
        // Assimp::Importer importer;
        //// const aiScene* scene = importer.ReadFileFromMemory(data, size, aiProcess_Triangulate | aiProcess_FlipUVs |
        //// aiProcess_CalcTangentSpace);
        // const aiScene* scene = nullptr;
        // String outstr;
        // if (VirtualFileSystem::Get()->ResolvePhyiscalPath(filepath, outstr))
        //    scene = importer.ReadFile(outstr, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // CW_ENGINE_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
        //                 "Error loading scene");

        // m_Directory = filepath.parent_path();
        // ProcessNode(scene->mRootNode, scene);
        // delete data;
    }

    void Model::Draw()
    {
        // for (uint32_t i = 0; i < m_Meshes.size(); i++)
        // {
        //     if (m_Meshes[i])
        //         m_Meshes[i]->Draw();
        // }
    }
#if 0
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
        Vertex* verts = new Vertex[mesh->mNumVertices];
        for (uint32_t i = 0; i < mesh->mNumVertices; i++)
        {
            verts[i].Position = glm::vec3{ mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            verts[i].Normal = glm::vec3{ mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

            if (mesh->mTextureCoords[0])
            {
                verts[i].Uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            }
            else
                verts[i].Uv = glm::vec2(0.0f, 0.0f);

            // verts[i].Binormal = glm::vec3{ mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
            verts[i].Tangent = glm::vec3{ mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
        }

        Ref<VertexBuffer> vbo = VertexBuffer::Create(sizeof(Vertex) * mesh->mNumVertices);
        void* dest = vbo->Map(0, sizeof(Vertex) * mesh->mNumVertices, GpuLockOptions::WRITE_DISCARD);
        memcpy(dest, verts, sizeof(Vertex) * mesh->mNumVertices);
        vbo->Unmap();
        /*vbo->SetLayout({ { ShaderDataType::Float3, "Position" },
                         { ShaderDataType::Float3, "Normal" },
                         { ShaderDataType::Float2, "Uv" },
                         { ShaderDataType::Float3, "Tangent" },
                         { ShaderDataType::Float3, "Binormal" } });*/
        vbo->SetLayout({ { ShaderDataType::Float3, "Position" },
                         { ShaderDataType::Float3, "Normal" },
                         { ShaderDataType::Float2, "Uv" },
                         { ShaderDataType::Float3, "Tangent" } });
        delete[] verts;

        // TODO: Optimize
        Vector<uint32_t> indices;

        for (uint32_t i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(face.mIndices[j]);
            }
        }

        Ref<IndexBuffer> ibo = IndexBuffer::Create(indices.data(), indices.size());
        /*
        Vector<Ref<Texture>> textures;
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            Vector<Ref<Texture>> diffuseMaps =
              this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
            Vector<Ref<Texture>> specularMaps =
              this->LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
            Vector<Ref<Texture>> normalMaps =
              this->LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
            Vector<Ref<Texture>> heightMaps =
              this->LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
            textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        }
        */
        return CreateRef<Mesh>(vbo, ibo);
    }

    Vector<Ref<Texture>> Model::LoadMaterialTextures(aiMaterial* material, aiTextureType type, const String& typeName)
    {
        Vector<Ref<Texture>> textures;
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
                // VULKAN IMPL: Fix
                // if (m_TexturesLoaded[j]->GetFilepath() == String(m_Directory + "/" + str.C_Str()))
                {
                    textures.push_back(m_TexturesLoaded[j]);
                    skip = true;
                    break;
                }
            }
            if (!skip)
            {
                // VULKAN IMPL: Fix
                // Ref<Texture> texture = Texture::Create(m_Directory + "/" + str.C_Str(), {}, str.C_Str());
                // textures[i] = texture;
                // m_TexturesLoaded.push_back(texture);
            }
        }

        return textures;
    }
#endif
} // namespace Crowny