#include "cwpch.h"

#include "Crowny/Import/MeshImporter.h"

#include "Crowny/Common//FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Renderer/Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

static_assert(sizeof(aiVector2D) == sizeof(glm::vec2));
static_assert(sizeof(aiVector3D) == sizeof(glm::vec3));
static_assert(sizeof(ai_real) == sizeof(float));

namespace Crowny
{
    bool MeshImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        Assimp::Importer importer;
        return importer.IsExtensionSupported("." + lower);
    }
    bool MeshImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    template <typename IndexType> static void SetIndexData(const aiMesh* mesh, const Ref<MeshData>& meshData)
    {
        Vector<IndexType> indices;
        indices.reserve(mesh->mNumFaces);
        for (uint32_t j = 0; j < mesh->mNumFaces; j++)
        {
            const aiFace& face = mesh->mFaces[j];
            // TODO: Add winding order control.
            CW_ENGINE_ASSERT(face.mNumIndices == 3);
            indices.push_back((IndexType)face.mIndices[0]);
            indices.push_back((IndexType)face.mIndices[1]);
            indices.push_back((IndexType)face.mIndices[2]);
        }
        meshData->SetIndexData<IndexType>(indices.data(), mesh->mNumFaces * 3);
    }

    Ref<Asset> MeshImporter::MeshImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Ref<const MeshImportOptions> meshImportOptions =
          std::static_pointer_cast<const MeshImportOptions>(importOptions);

        int flags = 0;

        if (meshImportOptions->Optimize)
            flags |= aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_ImproveCacheLocality |
                     aiProcess_RemoveRedundantMaterials;

        if (meshImportOptions->NormalsMode == NormalsImportMode::Calculate)
            flags |= meshImportOptions->SmoothNormals ? aiProcess_GenSmoothNormals : aiProcess_GenNormals;

        int removeComponentFlags = 0;
        if (meshImportOptions->NormalsMode != NormalsImportMode::Import)
            removeComponentFlags |= aiComponent_NORMALS;

        if (meshImportOptions->TangentsMode == NormalsImportMode::Calculate)
            flags |= aiProcess_CalcTangentSpace;
        if (meshImportOptions->TangentsMode != NormalsImportMode::Import)
            removeComponentFlags |= aiComponent_TANGENTS_AND_BITANGENTS;

        if (!meshImportOptions->KeepQuads)
            flags |= aiProcess_Triangulate;

#ifdef CW_DEBUG
            // This option will do a bunch of optional validation for meshes but is very strict.
            // flags |= aiProcess_ValidateDataStructure;
#endif
        Assimp::Importer importer;
        if (meshImportOptions->NormalsMode == NormalsImportMode::Calculate && meshImportOptions->SmoothNormals)
        {
            importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, meshImportOptions->SmoothingAngle);
            importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);
        }

        // std::vector<uint8_t> data;
        // Ref<DataStream> stream = FileSystem::OpenFile(path);
        // data.resize(stream->Size());
        // stream->Read(data.data(), data.size());
        // stream->Close();

        // const aiScene* scene = importer.ReadFileFromMemory(data.data(), data.size(), flags);
        const aiScene* scene = importer.ReadFile(path.string(), flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode)
        {
            CW_ENGINE_WARN("Failed mesh asset loading: Mesh: {0}, Error: {1}", path, importer.GetErrorString());
            return nullptr;
        }

        Vector<Ref<MeshData>> meshes;
        Vector<Vector<SubMesh>> subMeshes;
        for (uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            const uint32_t vertexCount = mesh->mNumVertices / 3;
            const uint32_t indexCount = mesh->mNumFaces * 3;
            CW_ENGINE_ASSERT(mesh->HasPositions() && mesh->HasFaces());

            BufferLayout bufferLayout = { BufferElement(ShaderDataType::Float3, VertexAttribute::Position) };
            const bool hasNormals = mesh->HasNormals();
            if (hasNormals)
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Normal));
            const bool hasTangents = mesh->HasTangentsAndBitangents();
            if (hasTangents)
            {
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Tangent));
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Bitangent));
            }
            static_assert(AI_MAX_NUMBER_OF_TEXTURECOORDS == 8);
            for (uint32_t uv = 0; uv < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
            {
                if (!mesh->HasTextureCoords(i))
                    break;
                bufferLayout.AddBufferElement(
                  BufferElement(ShaderDataType::Float2, (VertexAttribute)((int)VertexAttribute::TexCoord0 + i)));
            }

            const bool hasVertexColors = mesh->HasVertexColors(0);
            if (hasVertexColors)
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Color));

            IndexType indexType;
            if (meshImportOptions->IndexFormat == MeshIndexFormat::Auto)
                indexType = (mesh->mNumFaces < (uint32_t)std::numeric_limits<short>::max()) ? IndexType::Index_16
                                                                                            : IndexType::Index_32;
            else
                indexType = meshImportOptions->IndexFormat == MeshIndexFormat::Index16 ? IndexType::Index_16
                                                                                       : IndexType::Index_32;
            Ref<MeshData> meshData = MeshData::Create(vertexCount, indexCount, bufferLayout, indexType);
            meshData->SetVertexData(VertexAttribute::Position, mesh->mVertices, vertexCount * sizeof(glm::vec3));
            // For now only triangles are supported even though we don't always specify the Triangulate flag. In the
            // future the primitives should be sorted by their primitive type using the import flag and multiple
            // sub-meshes should be created with an index offset, count and draw mode. Also consider removing the
            // KeepQuads option as assimp might not have a way of triangulating only polygons and keep quads.
            CW_ENGINE_ASSERT((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == aiPrimitiveType_TRIANGLE);

            if (indexType == IndexType::Index_16)
                SetIndexData<uint16_t>(mesh, meshData);
            else
                SetIndexData<uint32_t>(mesh, meshData);
            if (hasNormals)
                meshData->SetVertexData(VertexAttribute::Normal, mesh->mNormals, vertexCount * sizeof(glm::vec3));
            for (uint32_t i = 0; i < vertexCount; i++)
            {
                CW_ENGINE_INFO("X: {0}, Y: {1}, Z: {2}", mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            if (hasVertexColors)
                meshData->SetVertexData(VertexAttribute::Color, mesh->mColors, vertexCount * sizeof(glm::vec3));
            if (hasTangents)
            {
                meshData->SetVertexData(VertexAttribute::Tangent, mesh->mTangents, vertexCount * sizeof(glm::vec3));
                meshData->SetVertexData(VertexAttribute::Bitangent, mesh->mBitangents, vertexCount * sizeof(glm::vec3));
            }

            for (uint32_t uv = 0; uv < AI_MAX_NUMBER_OF_TEXTURECOORDS; i++)
            {
                if (!mesh->HasTextureCoords(i))
                    break;
                const VertexAttribute uvAttr = VertexAttribute((int)VertexAttribute::TexCoord0 + i);
                Vector<glm::vec2> uvList;
                uvList.reserve(vertexCount);
                for (uint32_t i = 0; i < vertexCount; i++)
                    uvList.push_back(glm::vec2(mesh->mTextureCoords[uv][i].x, 1 - mesh->mTextureCoords[uv][i].y));
                meshData->SetVertexData(uvAttr, uvList.data(), vertexCount * sizeof(glm::vec2));
            }
            meshes.push_back(meshData);
        }
        if (meshes.size() == 0)
        {
            Ref<Mesh> mesh =
              Mesh::Create(0, 0, BufferLayout(), MeshUsage::Static, DrawMode::TRIANGLE_LIST, IndexType::Index_32);
            mesh->SetName(path.filename().string());
            return mesh;
        }
        CW_ENGINE_INFO("Mesh: {0} vertices, {1} indices", meshes[0]->GetVertexCount(), meshes[0]->GetIndexCount());
        Vector<SubMesh> outSubMeshes;
        if (meshes.size() == 1)
        {
            Ref<Mesh> mesh =
              Mesh::Create(meshes[0], {}, meshImportOptions->CpuCached ? MeshUsage::CpuCached : MeshUsage::Static);
            mesh->SetName(path.filename().string());
            return mesh;
        }
        const Ref<MeshData> combinedMeshData = MeshData::Combine(meshes, subMeshes, outSubMeshes);
        Ref<Mesh> mesh = Mesh::Create(combinedMeshData, outSubMeshes,
                                      meshImportOptions->CpuCached ? MeshUsage::CpuCached : MeshUsage::Static);
        mesh->SetName(path.filename().string());
        return mesh;
    }

    Ref<ImportOptions> MeshImporter::CreateImportOptions() const { return CreateRef<MeshImportOptions>(); }
} // namespace Crowny