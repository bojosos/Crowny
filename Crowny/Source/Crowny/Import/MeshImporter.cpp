#include "cwpch.h"

#include "Crowny/Import/MeshImporter.h"

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common//FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

static_assert(sizeof(aiVector2D) == sizeof(glm::vec2));
static_assert(sizeof(aiVector3D) == sizeof(glm::vec3));
static_assert(sizeof(aiColor3D) == sizeof(glm::vec3));
static_assert(sizeof(aiColor4D) == sizeof(glm::vec4));
static_assert(sizeof(ai_real) == sizeof(float));

namespace Crowny
{
    static glm::vec3 toGlmVec(const aiVector3D& vec) { return glm::vec3(vec.x, vec.y, vec.z); }

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
            // CW_ENGINE_INFO("Face: {}, {}, {}", face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            // CW_ENGINE_INFO("First vertex: X: {}, Y: {}, Z: {}", mesh->mVertices[face.mIndices[0]].x, mesh->mVertices[face.mIndices[0]].y,
            // mesh->mVertices[face.mIndices[0]].z);
            indices.push_back((IndexType)face.mIndices[0]);
            indices.push_back((IndexType)face.mIndices[1]);
            indices.push_back((IndexType)face.mIndices[2]);
        }
        meshData->SetIndexData<IndexType>(indices.data(), mesh->mNumFaces * 3);
    }

    static Ref<Mesh> ReadMeshData(const String& meshName, const aiScene* scene, const Ref<const MeshImportOptions>& meshImportOptions)
    {
        Vector<Ref<MeshData>> meshes;
        Vector<Vector<SubMesh>> subMeshes;
        for (uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            const uint32_t vertexCount = mesh->mNumVertices;
            const uint32_t indexCount = mesh->mNumFaces * 3;
            CW_ENGINE_ASSERT(mesh->HasPositions() && mesh->HasFaces());

            BufferLayout bufferLayout = { BufferElement(ShaderDataType::Float3, VertexAttribute::Position) };
            const bool hasNormals = mesh->HasNormals();
            if (hasNormals)
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Normal));
            const bool hasTangents = mesh->HasTangentsAndBitangents();
            CW_ENGINE_INFO("Normals: {}, tangents: {}, uvs0: {}", hasNormals, hasTangents, mesh->HasTextureCoords(0));
            if (hasTangents)
            {
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Tangent));
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Bitangent));
            }
            static_assert(AI_MAX_NUMBER_OF_TEXTURECOORDS == 8);
            for (uint32_t uv = 0; uv < AI_MAX_NUMBER_OF_TEXTURECOORDS; uv++)
            {
                if (!mesh->HasTextureCoords(uv))
                    break;
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float2, (VertexAttribute)((int)VertexAttribute::TexCoord0 + uv)));
            }
            const bool hasVertexColors = mesh->HasVertexColors(0);
            if (hasVertexColors)
                bufferLayout.AddBufferElement(BufferElement(ShaderDataType::Float4, VertexAttribute::Color));

            IndexType indexType;
            if (meshImportOptions->IndexFormat == MeshIndexFormat::Auto)
                indexType = (mesh->mNumFaces < (uint32_t)std::numeric_limits<short>::max()) ? IndexType::Index_16 : IndexType::Index_32;
            else
                indexType = meshImportOptions->IndexFormat == MeshIndexFormat::Index16 ? IndexType::Index_16 : IndexType::Index_32;
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
            // for (uint32_t i = 0; i < vertexCount; i++)
            //     CW_ENGINE_INFO("X: {0}, Y: {1}, Z: {2}", mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            // TODO: This is a color 4 in assimp!!!
            if (hasVertexColors)
                meshData->SetVertexData(VertexAttribute::Color, mesh->mColors[0], vertexCount * sizeof(glm::vec4));
            if (hasTangents)
            {
                meshData->SetVertexData(VertexAttribute::Tangent, mesh->mTangents, vertexCount * sizeof(glm::vec3));
                meshData->SetVertexData(VertexAttribute::Bitangent, mesh->mBitangents, vertexCount * sizeof(glm::vec3));
            }

            for (uint32_t uv = 0; uv < AI_MAX_NUMBER_OF_TEXTURECOORDS; uv++)
            {
                if (!mesh->HasTextureCoords(uv))
                    break;
                const VertexAttribute uvAttr = VertexAttribute((int)VertexAttribute::TexCoord0 + uv);
                Vector<glm::vec2> uvList;
                uvList.reserve(vertexCount);
                for (uint32_t j = 0; j < vertexCount; j++)
                    uvList.push_back(glm::vec2(mesh->mTextureCoords[uv][j].x, mesh->mTextureCoords[uv][j].y));
                meshData->SetVertexData(uvAttr, uvList.data(), vertexCount * sizeof(glm::vec2));
            }
            meshes.push_back(meshData);
        }
        if (meshes.size() == 0)
        {
            CW_ENGINE_WARN("Mesh import produced no mesh data: {}", meshName);
            return nullptr;
        }
        CW_ENGINE_INFO("Mesh: {0} vertices, {1} indices", meshes[0]->GetVertexCount(), meshes[0]->GetIndexCount());
        CW_ENGINE_INFO("Meshes: {0}", meshes.size());

        if (meshes.size() == 1)
        {
            Ref<MeshMorph> finalMorphs;
            CW_ENGINE_ASSERT(scene->mNumMeshes == 1);
            for (uint32_t i = 0; i < scene->mMeshes[0]->mNumAnimMeshes; i++)
            {
                // TODO: enum aiMorphingMethod...
                const aiAnimMesh* animMesh = scene->mMeshes[0]->mAnimMeshes[i];
                if (animMesh->mNumVertices != meshes[0]->GetVertexCount())
                {
                    CW_ENGINE_ERROR("Invalid blend morph mesh: source vertices {}, morph vertices {}", animMesh->mNumVertices,
                                    meshes[0]->GetVertexCount());
                    continue;
                }

                Vector<MorphData> vertexMorphs;
                const bool hasNormals = scene->mMeshes[0]->HasNormals() && animMesh->HasNormals();
                for (uint32_t j = 0; j < animMesh->mNumVertices; j++)
                {
                    const glm::vec3 vertexDelta = toGlmVec(scene->mMeshes[0]->mVertices[j] - animMesh->mVertices[j]);
                    const glm::vec3 normalDelta = hasNormals ? toGlmVec(scene->mMeshes[0]->mNormals[j] - animMesh->mNormals[j]) : glm::vec3();
                    if (glm::length2(vertexDelta) > 1e-5 || glm::length2(normalDelta) > 1e-5)
                        vertexMorphs.push_back(MorphData{ vertexDelta, normalDelta, j });
                }
                SingleMorph morph(animMesh->mName.C_Str(), animMesh->mWeight, vertexMorphs);
                FullMorph fullMorph(animMesh->mName.C_Str(), { std::move(morph) });
                finalMorphs = CreateRef<MeshMorph>(Vector<FullMorph>{ std::move(fullMorph) }, 1);
            }

            Ref<Mesh> mesh = Mesh::Create({meshes[0], meshImportOptions->CpuCached ? MeshUsage::CpuCached : MeshUsage::Static,
                                          DrawMode::TRIANGLE_LIST, finalMorphs});
            mesh->SetName(meshName);
            return mesh;
        }

        Vector<SubMesh> outSubMeshes;
        const Ref<MeshData> combinedMeshData = MeshData::Combine(meshes, subMeshes, outSubMeshes);
        MeshDesc meshDesc;
        meshDesc.Data = combinedMeshData;
        meshDesc.Usage = meshImportOptions->CpuCached ? MeshUsage::CpuCached : MeshUsage::Static;
        meshDesc.SubMeshes = outSubMeshes;
        Ref<Mesh> mesh = Mesh::Create(meshDesc);
        mesh->SetName(meshName);
        return mesh;
    }

    static const aiScene* ReadAssimpScene(const Path& path, const Ref<const MeshImportOptions>& meshImportOptions)
    {
        int flags = 0;

        flags |= aiProcess_JoinIdenticalVertices; // Always deduplicate vertices

        if (meshImportOptions->Optimize)
            flags |= aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials;

        if (meshImportOptions->NormalsMode == NormalsImportMode::Calculate)
            flags |= meshImportOptions->SmoothNormals ? aiProcess_GenSmoothNormals : aiProcess_GenNormals;

        int removeComponentFlags = 0;
        if (meshImportOptions->NormalsMode == NormalsImportMode::None)
            removeComponentFlags |= aiComponent_NORMALS;

        if (meshImportOptions->TangentsMode != NormalsImportMode::None)
            flags |= aiProcess_CalcTangentSpace;
        else
            removeComponentFlags |= aiComponent_TANGENTS_AND_BITANGENTS;

        if (!meshImportOptions->KeepQuads)
            flags |= aiProcess_Triangulate;

#ifdef CW_DEBUG
            // This option will do a bunch of optional validation for meshes but is very strict.
            // flags |= aiProcess_ValidateDataStructure;
#endif
        static Assimp::Importer importer;
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
        return scene;
    }

    static Vector<Ref<AnimationClip>> ImportAnimationClips(const aiScene* scene)
    {
        Vector<Ref<AnimationClip>> clips;
        for (uint32_t i = 0; i < scene->mNumAnimations; i++)
        {
            const aiAnimation* anim = scene->mAnimations[i];
            for (uint32_t j = 0; j < anim->mNumMorphMeshChannels; j++)
            {
                const aiMeshMorphAnim* morphAnim = anim->mMorphMeshChannels[j];
                if (morphAnim->mNumKeys < 2)
                {
                    CW_ENGINE_ERROR("Cannot import morph animation {}", morphAnim->mName.C_Str());
                    continue;
                }

                const double keyTime = morphAnim->mKeys[1].mTime - morphAnim->mKeys[0].mTime;
                bool simplify = true;
                for (uint32_t k = 2; k < morphAnim->mNumKeys; k++)
                {
                    aiMeshMorphKey& key = morphAnim->mKeys[k];
                    aiMeshMorphKey& lastKey = morphAnim->mKeys[k - 1];
                    if (!glm::epsilonEqual(key.mTime - lastKey.mTime, keyTime, 1e-3))
                    {
                        simplify = false;
                        break;
                    }
                }
                Vector<KeyFrame<float>> morphKeys;
                if (simplify)
                {
                    KeyFrame<float> start, end;
                    start.Time = float(morphAnim->mKeys[0].mTime / anim->mTicksPerSecond);
                    start.Value = float(morphAnim->mKeys[0].mWeights[0]);
                    end.Time = float(morphAnim->mKeys[morphAnim->mNumKeys - 1].mTime / anim->mTicksPerSecond);
                    end.Value = float(morphAnim->mKeys[morphAnim->mNumKeys - 1].mWeights[0]);
                    morphKeys = { start, end };
                }
                else
                {
                    morphKeys.reserve(morphAnim->mNumKeys);
                    for (uint32_t k = 2; k < morphAnim->mNumKeys; k++)
                    {
                        KeyFrame<float> morphKey;
                        morphKey.Time = float(morphAnim->mKeys[k].mTime / anim->mTicksPerSecond);
                        morphKey.Value = float(morphAnim->mKeys[k].mWeights[0]);
                        morphKeys.push_back(morphKey);
                    }
                }
                AnimationCurve<float> morphCurve(morphKeys);
                Ref<AnimationClip> animationClip = AnimationClip::Create(morphCurve);
                animationClip->SetName(anim->mName.C_Str());
                clips.push_back(animationClip);
            }
            for (uint32_t j = 0; j < anim->mNumChannels; j++)
            {
                CW_ENGINE_ASSERT(false);
            }

            for (uint32_t j = 0; j < anim->mNumMeshChannels; j++)
            {
                CW_ENGINE_ASSERT(false);
            }
        }

        return clips;
    }

    static UnorderedMap<String, Ref<Texture>> textureCache;

    static Ref<Texture> ImportTexture(const aiMaterial* meshMaterial, const Ref<Material>& material, aiTextureType textureType,
                                      const String& shaderParameter)
    {
        aiString texturePath;
        if (meshMaterial->GetTexture(textureType, 0, &texturePath) == aiReturn_SUCCESS)
        {
            if (textureCache.count(texturePath.C_Str()) != 0)
                return textureCache[texturePath.C_Str()];
            else
            {
                Ref<Texture> texture = Importer::Get().Import<Texture>(texturePath.C_Str());
                textureCache[texturePath.C_Str()] = texture;
                material->SetTexture(shaderParameter, texture);
                return texture;
            }
        }

        return nullptr;
    }

    static Vector<Ref<Asset>> ImportMaterials(const aiScene* scene)
    {
        Vector<Ref<Asset>> assets;
        AssetHandle<Shader> pbriblHandle = gAssetManager->Load<Shader>(PBRIBL_SHADER_PATH);

        // static Ref<Shader> pbriblShader = Importer::Get().Import<Shader>("Resources/Shaders/Pbribl.glsl");
        // static const AssetHandle<Shader> pbriblHandle = static_asset_cast<Shader>(gAssetManager->CreateAssetHandle(pbriblShader));
        for (uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            const aiMesh* aiMesh = scene->mMeshes[i];
            const aiMaterial* meshMaterial = scene->mMaterials[aiMesh->mMaterialIndex];

            Ref<Material> material = Material::Create(pbriblHandle);
            material->SetName(meshMaterial->GetName().C_Str());

            assets.push_back(ImportTexture(meshMaterial, material, aiTextureType_DIFFUSE, "albedoMap"));
            assets.push_back(ImportTexture(meshMaterial, material, aiTextureType_METALNESS, "metallicMap"));
            assets.push_back(ImportTexture(meshMaterial, material, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap"));
            assets.push_back(ImportTexture(meshMaterial, material, aiTextureType_NORMALS, "normalMap"));
            assets.push_back(ImportTexture(meshMaterial, material, aiTextureType_AMBIENT_OCCLUSION, "aoMap"));

            // Read PBR parameters from assimp
            aiColor3D color(1.0f, 1.0f, 1.0f);
            if (meshMaterial->Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS)
                material->SetColor("albedo", glm::vec4(color.r, color.g, color.b, 1.0f));

            float metalness = 0.0f;
            if (meshMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness) == aiReturn_SUCCESS)
                material->SetFloat("metalness", metalness);

            float roughness = 1.0f;
            if (meshMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS)
                material->SetFloat("roughness", roughness);

            assets.push_back(material);
        }

        textureCache.clear();
        return assets;
    }

    Ref<Asset> MeshImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Vector<Ref<Asset>> assets = ImportAll(path, importOptions);
        textureCache.clear();
        if (assets.empty())
            return nullptr;
        return assets[0];
    }

    Vector<Ref<Asset>> MeshImporter::ImportAll(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Ref<const MeshImportOptions> meshImportOptions = StaticRefCast<const MeshImportOptions>(importOptions);

        const aiScene* scene = ReadAssimpScene(path, meshImportOptions);
        if (!scene)
            return {};

        Vector<Ref<Asset>> assets;
        Ref<Mesh> mesh = ReadMeshData(path.filename().string(), scene, meshImportOptions);
        if (!mesh)
            return {};
        assets.push_back(mesh);

        const Vector<Ref<AnimationClip>> animations = ImportAnimationClips(scene);
        assets.insert(assets.end(), animations.cbegin(), animations.cend());

        const Vector<Ref<Asset>> materialAssets = ImportMaterials(scene);
        assets.insert(assets.end(), materialAssets.cbegin(), materialAssets.cend());

        return assets;
    }

    Ref<ImportOptions> MeshImporter::CreateImportOptions() const { return CreateRef<MeshImportOptions>(); }
} // namespace Crowny