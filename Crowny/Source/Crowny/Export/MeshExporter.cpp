#include <cwpch.h>

#include "Crowny/Export/MeshExporter.h"

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

namespace Crowny
{
    // Assimp will try to delete the arrays, so we use this magic to clean up after.
    template <typename T> struct ScopedAssign
    {
        T& target;
        ScopedAssign(T& targetRef, T val) : target(targetRef) { target = val; }
        ~ScopedAssign() { target = nullptr; }
    };

    template <typename F> struct ScopeExit
    {
        F cleanupFunc;

        // Add this constructor to fix C++17 deduction
        ScopeExit(F f) : cleanupFunc(std::move(f)) {}

        ~ScopeExit() { cleanupFunc(); }
    };

    MeshExporter::MeshExporter(const Ref<MeshData>& meshData) : m_MeshData(meshData) {}

    void MeshExporter::Export(const Path& path)
    {
        String ext = path.extension().string();
        if (ext.empty())
        {
            CW_ENGINE_ERROR("You need to provide a valid mesh file extension");
            return;
        }
        ext = ext.substr(1, ext.size() - 1);

        Assimp::Exporter exporter;
        String assimpFormatId;
        for (size_t i = 0; i < exporter.GetExportFormatCount(); i++)
        {
            auto* exportFormat = exporter.GetExportFormatDescription(i);
            if (ext == exportFormat->fileExtension)
            {
                assimpFormatId = exportFormat->id;
                break;
            }
        }
        if (assimpFormatId.empty())
        {
            CW_ENGINE_ERROR("Unsupported mesh file format: {}", ext);
            return;
        }

        aiMesh mesh{};
        mesh.mName = aiString("Crowny Mesh");

        static_assert(alignof(aiVector3D) == alignof(glm::vec3) && sizeof(aiVector3D) == sizeof(glm::vec3));

        auto verts = m_MeshData->GetPositions();
        ScopedAssign assignVerts(mesh.mVertices, reinterpret_cast<aiVector3D*>(const_cast<glm::vec3*>(verts.data())));
        mesh.mNumVertices = verts.size();

        auto normals = m_MeshData->GetNormals();
        ScopedAssign assignNormals(mesh.mNormals, reinterpret_cast<aiVector3D*>(const_cast<glm::vec3*>(normals.data())));

        auto uvs0 = m_MeshData->GetUVs();
        ScopedAssign assignUVs(mesh.mTextureCoords[0], reinterpret_cast<aiVector3D*>(const_cast<glm::vec2*>(uvs0.data())));

        auto tangents = m_MeshData->GetTangents();
        ScopedAssign assignTangents(mesh.mTangents, reinterpret_cast<aiVector3D*>(const_cast<glm::vec3*>(tangents.data())));

        aiString str("uvs0");
        aiString* uvsNames[AI_MAX_NUMBER_OF_TEXTURECOORDS] = { &str };
        ScopedAssign assignUVNames(mesh.mTextureCoordsNames, uvsNames);

        auto indices = m_MeshData->GetIndices();
        Vector<aiFace> faces;
        faces.resize(indices.size() / 3);

        // Set up the faces
        for (size_t i = 0; i < indices.size() / 3; i++)
        {
            faces[i].mNumIndices = 3;
            faces[i].mIndices = const_cast<unsigned int*>(indices.data() + i * 3);
        }

        ScopeExit facesGuard{ [&]() {
            for (auto& face : faces)
                face.mIndices = nullptr;
        } };

        ScopedAssign assignFaces(mesh.mFaces, faces.data());
        mesh.mNumFaces = indices.size() / 3;

        aiScene scene{};
        aiMesh* meshPtr = &mesh;
        ScopedAssign assignSceneMeshes(scene.mMeshes, &meshPtr);
        scene.mNumMeshes = 1;

        aiNode rootNode{};
        rootNode.mNumMeshes = 1;
        unsigned int ar[1] = { 0 };
        ScopedAssign assignRootNodeMeshes(rootNode.mMeshes, ar);

        ScopedAssign assignSceneRoot(scene.mRootNode, &rootNode);

        aiMaterial mat{};
        aiMaterial* materials[1] = { &mat };
        ScopedAssign assignSceneMaterials(scene.mMaterials, materials);
        scene.mNumMaterials = 1;
        mesh.mMaterialIndex = 0;

        // When this function finishes, all ScopedAssigns and the ScopeExit will trigger in reverse order,
        // nulling out the pointers before Assimp's destructors run.
        exporter.Export(&scene, assimpFormatId, path.string());
    }

} // namespace Crowny
