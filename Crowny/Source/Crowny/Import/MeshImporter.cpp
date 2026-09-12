#include "cwpch.h"

#include "Crowny/Import/MeshImporter.h"

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/ImageLoader.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Import/TextureImporter.h"
#include "Crowny/Physics/PhysicsMesh.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MeshProcessing.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Utils/Cryptography.h"
#include "Crowny/Utils/PixelUtils.h"
#include <assimp/GltfMaterial.h>
#include <atomic>
#include <fstream>
#include <future>
#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MAX_BONE_INFLUENCES = 4;

        struct VertexBoneData
        {
            glm::vec4 Weights{ 0.0f };
            glm::ivec4 Indices{ 0 };
        };

        glm::vec3 ToGlm(const aiVector3D& value) { return { value.x, value.y, value.z }; }

        glm::quat ToGlm(const aiQuaternion& value) { return glm::normalize(glm::quat(value.w, value.x, value.y, value.z)); }

        glm::mat4 ToGlm(const aiMatrix4x4& value)
        {
            return { value.a1, value.b1, value.c1, value.d1, value.a2, value.b2, value.c2, value.d2,
                     value.a3, value.b3, value.c3, value.d3, value.a4, value.b4, value.c4, value.d4 };
        }

        Transform ToTransform(const aiMatrix4x4& value, float scaleFactor)
        {
            glm::mat4 matrix = ToGlm(value);
            matrix[3] = glm::vec4(glm::vec3(matrix[3]) * scaleFactor, matrix[3].w);
            glm::vec3 position(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);
            if (!Math::DecomposeMatrix(matrix, position, rotation, scale))
                return Transform();
            return Transform(position, glm::normalize(rotation), scale);
        }

        struct MeshInstanceTransform
        {
            glm::mat4 NodeToScene{ 1.0f };
            glm::mat3 Linear{ 1.0f };
            glm::mat3 Normal{ 1.0f };
            float ScaleFactor = 1.0f;
            bool Mirrored = false;
            bool HasNodeTransform = false;
        };

        bool IsFinite(const glm::vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

        bool IsFinite(const glm::mat4& value)
        {
            for (uint32_t column = 0; column < 4; column++)
            {
                for (uint32_t row = 0; row < 4; row++)
                {
                    if (!std::isfinite(value[column][row]))
                        return false;
                }
            }
            return true;
        }

        bool IsFinite(const glm::mat3& value)
        {
            for (uint32_t column = 0; column < 3; column++)
            {
                for (uint32_t row = 0; row < 3; row++)
                {
                    if (!std::isfinite(value[column][row]))
                        return false;
                }
            }
            return true;
        }

        bool IsIdentity(const glm::mat4& value)
        {
            constexpr float tolerance = 1e-5f;
            for (uint32_t column = 0; column < 4; column++)
            {
                for (uint32_t row = 0; row < 4; row++)
                {
                    const float expected = column == row ? 1.0f : 0.0f;
                    if (std::abs(value[column][row] - expected) > tolerance)
                        return false;
                }
            }
            return true;
        }

        bool NormalizeDirection(glm::vec3& value)
        {
            const float lengthSquared = glm::dot(value, value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= std::numeric_limits<float>::epsilon())
                return false;
            value /= std::sqrt(lengthSquared);
            return IsFinite(value);
        }

        bool BuildInstanceTransform(const glm::mat4& nodeToScene, float scaleFactor, StringView instanceName, MeshInstanceTransform& transform)
        {
            constexpr float affineTolerance = 1e-5f;
            if (!IsFinite(nodeToScene) || std::abs(nodeToScene[0][3]) > affineTolerance || std::abs(nodeToScene[1][3]) > affineTolerance ||
                std::abs(nodeToScene[2][3]) > affineTolerance || std::abs(nodeToScene[3][3] - 1.0f) > affineTolerance)
            {
                CW_ENGINE_WARN("Skipping mesh instance '{}' because its node transform is non-finite or non-affine.", instanceName);
                return false;
            }

            const glm::mat3 nodeLinear(nodeToScene);
            const float determinant = glm::determinant(nodeLinear);
            const float basisScale = glm::length(nodeLinear[0]) * glm::length(nodeLinear[1]) * glm::length(nodeLinear[2]);
            if (!std::isfinite(determinant) || !std::isfinite(basisScale) || basisScale <= std::numeric_limits<float>::min() ||
                std::abs(determinant) <= basisScale * 1e-6f)
            {
                CW_ENGINE_WARN("Skipping mesh instance '{}' because its node transform collapses an axis.", instanceName);
                return false;
            }

            transform.NodeToScene = nodeToScene;
            transform.Linear = nodeLinear * scaleFactor;
            transform.Normal = glm::transpose(glm::inverse(nodeLinear));
            if (scaleFactor < 0.0f)
                transform.Normal *= -1.0f;
            transform.ScaleFactor = scaleFactor;
            transform.Mirrored = (determinant < 0.0f) != (scaleFactor < 0.0f);
            transform.HasNodeTransform = !IsIdentity(nodeToScene);
            if (!IsFinite(transform.Linear) || !IsFinite(transform.Normal))
            {
                CW_ENGINE_WARN("Skipping mesh instance '{}' because its direction transforms are non-finite.", instanceName);
                return false;
            }
            return true;
        }

        DrawMode GetDrawMode(const aiMesh& mesh)
        {
            switch (mesh.mPrimitiveTypes)
            {
            case aiPrimitiveType_POINT:
                return DrawMode::POINT_LIST;
            case aiPrimitiveType_LINE:
                return DrawMode::LINE_LIST;
            case aiPrimitiveType_TRIANGLE:
                return DrawMode::TRIANGLE_LIST;
            default:
                return DrawMode::TRIANGLE_LIST;
            }
        }

        uint32_t GetExpectedIndicesPerFace(DrawMode drawMode)
        {
            switch (drawMode)
            {
            case DrawMode::POINT_LIST:
                return 1;
            case DrawMode::LINE_LIST:
                return 2;
            case DrawMode::TRIANGLE_LIST:
                return 3;
            default:
                return 0;
            }
        }

        float GetScaleFactor(const MeshImportOptions& options)
        {
            if (std::isfinite(options.ScaleFactor) && glm::abs(options.ScaleFactor) > std::numeric_limits<float>::epsilon())
                return options.ScaleFactor;
            CW_ENGINE_WARN("Invalid mesh scale factor {}. Using 1.0.", options.ScaleFactor);
            return 1.0f;
        }

        uint32_t ConfigureImporter(Assimp::Importer& importer, const MeshImportOptions& options)
        {
            uint32_t flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_FindInvalidData;

            importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, glm::clamp(options.SmoothingAngle, 0.0f, 175.0f));

            if (options.Optimize)
                flags |= aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials;
            if (options.FlipUVs)
                flags |= aiProcess_FlipUVs;
            if (options.FlipWindingOrder)
                flags |= aiProcess_FlipWindingOrder;

            uint32_t removeComponents = 0;
            switch (options.NormalsMode)
            {
            case NormalsImportMode::Calculate:
                flags |= aiProcess_DropNormals;
                flags |= options.SmoothNormals ? aiProcess_GenSmoothNormals : aiProcess_GenNormals;
                break;
            case NormalsImportMode::None:
                removeComponents |= aiComponent_NORMALS;
                break;
            default:
                break;
            }

            switch (options.TangentsMode)
            {
            case NormalsImportMode::Calculate:
                removeComponents |= aiComponent_TANGENTS_AND_BITANGENTS;
                flags |= aiProcess_CalcTangentSpace;
                break;
            case NormalsImportMode::None:
                removeComponents |= aiComponent_TANGENTS_AND_BITANGENTS;
                break;
            default:
                break;
            }

            if (removeComponents != 0)
            {
                importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, static_cast<int>(removeComponents));
                flags |= aiProcess_RemoveComponent;
            }

            if (options.KeepQuads)
                CW_ENGINE_WARN("KeepQuads is ignored because Crowny has no quad draw topology. Polygons will be triangulated.");

            return flags;
        }

        const aiScene* ReadScene(Assimp::Importer& importer, const Path& path, const MeshImportOptions& options)
        {
            const aiScene* scene = importer.ReadFile(path.string(), ConfigureImporter(importer, options));
            if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode)
            {
                CW_ENGINE_WARN("Failed mesh asset loading: Mesh: {}, Error: {}", path, importer.GetErrorString());
                return nullptr;
            }
            return scene;
        }

        UnorderedMap<String, uint32_t, StringHash, StringEqual> ReadBones(const aiScene& scene, const MeshImportOptions& options,
                                                                          MeshImportResult& result)
        {
            UnorderedMap<String, glm::mat4, StringHash, StringEqual> inverseBindPoses;
            UnorderedMap<String, const aiNode*, StringHash, StringEqual> nodes;
            std::function<void(const aiNode*)> mapNodes = [&](const aiNode* node) {
                nodes.insert_or_assign(node->mName.C_Str(), node);
                for (uint32_t child = 0; child < node->mNumChildren; child++)
                    mapNodes(node->mChildren[child]);
            };
            mapNodes(scene.mRootNode);

            for (uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; meshIndex++)
            {
                const aiMesh& mesh = *scene.mMeshes[meshIndex];
                for (uint32_t boneIndex = 0; boneIndex < mesh.mNumBones; boneIndex++)
                {
                    const aiBone& bone = *mesh.mBones[boneIndex];
                    const String name = bone.mName.C_Str();
                    if (inverseBindPoses.find(name) != inverseBindPoses.end())
                        continue;
                    glm::mat4 inverseBindPose = ToGlm(bone.mOffsetMatrix);
                    inverseBindPose[3] = glm::vec4(glm::vec3(inverseBindPose[3]) * GetScaleFactor(options), inverseBindPose[3].w);
                    inverseBindPoses.emplace(name, inverseBindPose);
                }
            }

            UnorderedSet<String, StringHash, StringEqual> requiredNodes;
            for (const auto& [name, inverseBindPose] : inverseBindPoses)
            {
                const auto found = nodes.find(name);
                if (found == nodes.end())
                    continue;
                const aiNode* node = found->second;
                while (node != nullptr)
                {
                    requiredNodes.emplace(node->mName.C_Str());
                    node = node->mParent;
                }
            }

            UnorderedMap<String, uint32_t, StringHash, StringEqual> boneIndices;
            const float scaleFactor = GetScaleFactor(options);
            std::function<void(const aiNode*, uint32_t)> addHierarchy = [&](const aiNode* node, uint32_t parentIndex) {
                uint32_t nextParent = parentIndex;
                const String name = node->mName.C_Str();
                if (requiredNodes.find(name) != requiredNodes.end())
                {
                    const auto inverse = inverseBindPoses.find(name);
                    MeshImportedBone bone;
                    bone.Name = name;
                    bone.ParentIndex = parentIndex;
                    bone.LocalBindPose = ToTransform(node->mTransformation, scaleFactor);
                    bone.InverseBindPose = inverse != inverseBindPoses.end() ? inverse->second : glm::mat4(1.0f);
                    nextParent = static_cast<uint32_t>(result.Bones.size());
                    result.Bones.push_back(std::move(bone));
                    boneIndices.emplace(name, nextParent);
                }
                for (uint32_t child = 0; child < node->mNumChildren; child++)
                    addHierarchy(node->mChildren[child], nextParent);
            };
            addHierarchy(scene.mRootNode, INVALID_BONE_INDEX);

            for (const auto& [name, inverseBindPose] : inverseBindPoses)
            {
                if (boneIndices.find(name) != boneIndices.end())
                    continue;
                const uint32_t index = static_cast<uint32_t>(result.Bones.size());
                result.Bones.push_back({ name, INVALID_BONE_INDEX, Transform(), inverseBindPose });
                boneIndices.emplace(name, index);
                CW_ENGINE_WARN("Bone '{}' has no matching node in the imported hierarchy; treating it as a root bone.", name);
            }

            Vector<SkeletonBone> bones;
            bones.reserve(result.Bones.size());
            for (const MeshImportedBone& imported : result.Bones)
                bones.push_back({ imported.Name, imported.ParentIndex, imported.LocalBindPose, imported.InverseBindPose });
            if (!bones.empty())
            {
                result.MeshSkeleton = Skeleton::Create(std::move(bones));
                if (!result.MeshSkeleton->IsValid())
                    CW_ENGINE_WARN("Imported skeleton hierarchy is invalid.");
            }
            return boneIndices;
        }

        void InsertBoneInfluence(VertexBoneData& vertex, uint32_t boneIndex, float weight)
        {
            if (weight <= 0.0f)
                return;

            for (uint32_t influence = 0; influence < MAX_BONE_INFLUENCES; influence++)
            {
                if (weight <= vertex.Weights[influence])
                    continue;

                for (uint32_t move = MAX_BONE_INFLUENCES - 1; move > influence; move--)
                {
                    vertex.Weights[move] = vertex.Weights[move - 1];
                    vertex.Indices[move] = vertex.Indices[move - 1];
                }
                vertex.Weights[influence] = weight;
                vertex.Indices[influence] = static_cast<int32_t>(boneIndex);
                break;
            }
        }

        Vector<VertexBoneData> ReadBoneWeights(const aiMesh& mesh, const UnorderedMap<String, uint32_t, StringHash, StringEqual>& boneIndices)
        {
            Vector<VertexBoneData> vertices(mesh.mNumVertices);
            for (uint32_t boneIndex = 0; boneIndex < mesh.mNumBones; boneIndex++)
            {
                const aiBone& bone = *mesh.mBones[boneIndex];
                const auto globalBone = boneIndices.find(bone.mName.C_Str());
                if (globalBone == boneIndices.end())
                    continue;

                for (uint32_t weightIndex = 0; weightIndex < bone.mNumWeights; weightIndex++)
                {
                    const aiVertexWeight& influence = bone.mWeights[weightIndex];
                    if (influence.mVertexId >= mesh.mNumVertices)
                    {
                        CW_ENGINE_WARN("Bone '{}' contains an invalid vertex index {}.", bone.mName.C_Str(), influence.mVertexId);
                        continue;
                    }
                    InsertBoneInfluence(vertices[influence.mVertexId], globalBone->second, influence.mWeight);
                }
            }

            for (VertexBoneData& vertex : vertices)
            {
                const float totalWeight = vertex.Weights.x + vertex.Weights.y + vertex.Weights.z + vertex.Weights.w;
                if (totalWeight > 0.0f)
                    vertex.Weights /= totalWeight;
            }
            return vertices;
        }

        bool ReadIndices(const aiMesh& mesh, DrawMode drawMode, Vector<uint32_t>& indices)
        {
            const uint32_t indicesPerFace = GetExpectedIndicesPerFace(drawMode);
            if (indicesPerFace == 0)
                return false;

            indices.clear();
            indices.reserve(static_cast<size_t>(mesh.mNumFaces) * indicesPerFace);
            for (uint32_t faceIndex = 0; faceIndex < mesh.mNumFaces; faceIndex++)
            {
                const aiFace& face = mesh.mFaces[faceIndex];
                if (face.mNumIndices != indicesPerFace)
                {
                    CW_ENGINE_WARN("Mesh '{}' has an unsupported face with {} indices after post-processing.", mesh.mName.C_Str(), face.mNumIndices);
                    return false;
                }
                for (uint32_t index = 0; index < face.mNumIndices; index++)
                {
                    if (face.mIndices[index] >= mesh.mNumVertices)
                    {
                        CW_ENGINE_WARN("Mesh '{}' contains an out-of-range vertex index {}.", mesh.mName.C_Str(), face.mIndices[index]);
                        return false;
                    }
                    indices.push_back(face.mIndices[index]);
                }
            }
            return !indices.empty();
        }

        Ref<MeshData> ReadMesh(const aiMesh& mesh, const MeshImportOptions& options,
                               const UnorderedMap<String, uint32_t, StringHash, StringEqual>& boneIndices,
                               const MeshInstanceTransform& instanceTransform, StringView instanceName, DrawMode& drawMode)
        {
            if (!mesh.HasPositions() || !mesh.HasFaces() || mesh.mNumVertices == 0)
            {
                CW_ENGINE_WARN("Skipping mesh '{}' because it has no readable geometry.", mesh.mName.C_Str());
                return nullptr;
            }

            drawMode = GetDrawMode(mesh);
            Vector<uint32_t> indices;
            if (!ReadIndices(mesh, drawMode, indices))
                return nullptr;
            if (instanceTransform.Mirrored && drawMode == DrawMode::TRIANGLE_LIST)
            {
                for (size_t index = 0; index < indices.size(); index += 3)
                    std::swap(indices[index + 1], indices[index + 2]);
            }

            BufferLayout layout = { BufferElement(ShaderDataType::Float3, VertexAttribute::Position) };
            if (mesh.HasNormals())
                layout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Normal));
            if (mesh.HasTangentsAndBitangents())
            {
                layout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Tangent));
                layout.AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Bitangent));
            }

            static_assert(AI_MAX_NUMBER_OF_TEXTURECOORDS == 8);
            for (uint32_t channel = 0; channel < AI_MAX_NUMBER_OF_TEXTURECOORDS; channel++)
            {
                if (mesh.HasTextureCoords(channel))
                    layout.AddBufferElement(BufferElement(ShaderDataType::Float2,
                                                          static_cast<VertexAttribute>(static_cast<int32_t>(VertexAttribute::TexCoord0) + channel)));
            }

            const bool hasVertexColors = options.ImportVertexColors && mesh.HasVertexColors(0);
            if (hasVertexColors)
                layout.AddBufferElement(BufferElement(ShaderDataType::Float4, VertexAttribute::Color));

            const bool hasBones = options.ImportBones && mesh.HasBones();
            if (hasBones)
            {
                layout.AddBufferElement(BufferElement(ShaderDataType::Float4, VertexAttribute::BlendWeights));
                layout.AddBufferElement(BufferElement(ShaderDataType::Int4, VertexAttribute::BlendIndices));
            }

            const bool needs32BitIndices = mesh.mNumVertices > static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1U;
            IndexType indexType = IndexType::Index_32;
            if (options.IndexFormat == MeshIndexFormat::Index16 || options.IndexFormat == MeshIndexFormat::Auto)
                indexType = needs32BitIndices ? IndexType::Index_32 : IndexType::Index_16;
            if (options.IndexFormat == MeshIndexFormat::Index16 && needs32BitIndices)
                CW_ENGINE_WARN("Mesh '{}' exceeds the 16-bit index range. Using 32-bit indices.", mesh.mName.C_Str());

            const Ref<MeshData> data = MeshData::Create(mesh.mNumVertices, static_cast<uint32_t>(indices.size()), layout, indexType);

            Vector<glm::vec3> positions(mesh.mNumVertices);
            for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
            {
                positions[vertex] =
                  glm::vec3(instanceTransform.NodeToScene * glm::vec4(ToGlm(mesh.mVertices[vertex]), 1.0f)) * instanceTransform.ScaleFactor;
                if (!IsFinite(positions[vertex]))
                {
                    CW_ENGINE_WARN("Skipping mesh instance '{}' because transformed position {} is non-finite.", instanceName, vertex);
                    return nullptr;
                }
            }
            data->SetPositions(positions);
            data->SetIndices(indices);

            Vector<glm::vec3> normals;
            if (mesh.HasNormals())
            {
                normals.resize(mesh.mNumVertices);
                for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
                {
                    normals[vertex] = instanceTransform.Normal * ToGlm(mesh.mNormals[vertex]);
                    if (!NormalizeDirection(normals[vertex]))
                    {
                        CW_ENGINE_WARN("Skipping mesh instance '{}' because transformed normal {} is invalid.", instanceName, vertex);
                        return nullptr;
                    }
                }
                data->SetNormals(normals);
            }
            if (mesh.HasTangentsAndBitangents())
            {
                Vector<glm::vec3> tangents(mesh.mNumVertices);
                Vector<glm::vec3> bitangents(mesh.mNumVertices);
                uint32_t repairedBases = 0;
                for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
                {
                    const glm::vec3 sourceTangent = ToGlm(mesh.mTangents[vertex]);
                    const glm::vec3 sourceBitangent = ToGlm(mesh.mBitangents[vertex]);
                    tangents[vertex] = instanceTransform.Linear * sourceTangent;

                    if (mesh.HasNormals())
                    {
                        const glm::vec3 sourceNormal = ToGlm(mesh.mNormals[vertex]);
                        const glm::vec3& normal = normals[vertex];
                        const float sourceOrientation = glm::dot(glm::cross(sourceNormal, sourceTangent), sourceBitangent);
                        tangents[vertex] -= normal * glm::dot(normal, tangents[vertex]);
                        const bool validTangent = NormalizeDirection(tangents[vertex]);
                        if (!validTangent || !std::isfinite(sourceOrientation) ||
                            std::abs(sourceOrientation) <= std::numeric_limits<float>::epsilon())
                        {
                            // A collapsed UV edge can produce a zero tangent at just one vertex.
                            // Keep the geometry and use an orthonormal basis at that vertex.
                            if (!validTangent)
                            {
                                const glm::vec3 axis = std::abs(normal.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                                tangents[vertex] = glm::normalize(glm::cross(axis, normal));
                            }
                            repairedBases++;
                        }
                        bitangents[vertex] = glm::cross(normal, tangents[vertex]);
                        bitangents[vertex] *= sourceOrientation < 0.0f ? -1.0f : 1.0f;
                        bitangents[vertex] *= instanceTransform.Mirrored ? -1.0f : 1.0f;
                    }
                    else
                    {
                        bitangents[vertex] = instanceTransform.Linear * sourceBitangent;
                        if (!NormalizeDirection(tangents[vertex]))
                        {
                            tangents[vertex] = glm::vec3(1, 0, 0);
                            repairedBases++;
                        }
                        if (!NormalizeDirection(bitangents[vertex]))
                        {
                            const glm::vec3 axis = std::abs(tangents[vertex].y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
                            bitangents[vertex] = glm::normalize(glm::cross(tangents[vertex], axis));
                            repairedBases++;
                        }
                    }
                }
                if (repairedBases != 0)
                    CW_ENGINE_WARN("Repaired {} degenerate tangent bases in mesh instance '{}'.", repairedBases, instanceName);
                data->SetTangents(tangents);
                data->SetBitangents(bitangents);
            }

            for (uint32_t channel = 0; channel < AI_MAX_NUMBER_OF_TEXTURECOORDS; channel++)
            {
                if (!mesh.HasTextureCoords(channel))
                    continue;

                Vector<glm::vec2> textureCoordinates(mesh.mNumVertices);
                for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
                    textureCoordinates[vertex] = { mesh.mTextureCoords[channel][vertex].x, mesh.mTextureCoords[channel][vertex].y };
                data->SetUVs(channel, textureCoordinates);
            }

            if (hasVertexColors)
            {
                Vector<glm::vec4> colors(mesh.mNumVertices);
                for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
                {
                    const aiColor4D& color = mesh.mColors[0][vertex];
                    colors[vertex] = { color.r, color.g, color.b, color.a };
                }
                data->SetColors(colors);
            }

            if (hasBones)
            {
                const Vector<VertexBoneData> boneData = ReadBoneWeights(mesh, boneIndices);
                Vector<glm::vec4> weights(mesh.mNumVertices);
                Vector<glm::ivec4> boneIds(mesh.mNumVertices);
                for (uint32_t vertex = 0; vertex < mesh.mNumVertices; vertex++)
                {
                    weights[vertex] = boneData[vertex].Weights;
                    boneIds[vertex] = boneData[vertex].Indices;
                }
                data->SetVertexData(VertexAttribute::BlendWeights, weights.data(), static_cast<uint32_t>(weights.size() * sizeof(glm::vec4)));
                data->SetVertexData(VertexAttribute::BlendIndices, boneIds.data(), static_cast<uint32_t>(boneIds.size() * sizeof(glm::ivec4)));
            }

            return data;
        }

        struct MorphChannelBuilder
        {
            String Name;
            float ShapeWeight = 1.0f;
            Vector<MorphData> Vertices;
        };

        void ReadMorphs(const aiMesh& source, uint32_t vertexOffset, const MeshInstanceTransform& instanceTransform,
                        Vector<MorphChannelBuilder>& morphs, UnorderedMap<String, uint32_t, StringHash, StringEqual>& morphIndices)
        {
            for (uint32_t morphIndex = 0; morphIndex < source.mNumAnimMeshes; morphIndex++)
            {
                const aiAnimMesh& target = *source.mAnimMeshes[morphIndex];
                if (target.mNumVertices != source.mNumVertices || !target.HasPositions())
                {
                    CW_ENGINE_WARN("Skipping morph target '{}' because its vertex data does not match mesh '{}'.", target.mName.C_Str(),
                                   source.mName.C_Str());
                    continue;
                }

                Vector<MorphData> changes;
                const bool hasNormals = source.HasNormals() && target.HasNormals();
                bool validTarget = true;
                for (uint32_t vertex = 0; vertex < target.mNumVertices; vertex++)
                {
                    const glm::vec3 vertexDelta = instanceTransform.Linear * ToGlm(target.mVertices[vertex] - source.mVertices[vertex]);
                    glm::vec3 normalDelta(0.0f);
                    if (hasNormals)
                    {
                        glm::vec3 sourceNormal = instanceTransform.Normal * ToGlm(source.mNormals[vertex]);
                        glm::vec3 targetNormal = instanceTransform.Normal * ToGlm(target.mNormals[vertex]);
                        if (!NormalizeDirection(sourceNormal) || !NormalizeDirection(targetNormal))
                        {
                            validTarget = false;
                            break;
                        }
                        normalDelta = targetNormal - sourceNormal;
                    }
                    if (!IsFinite(vertexDelta) || !IsFinite(normalDelta))
                    {
                        validTarget = false;
                        break;
                    }
                    if (glm::length2(vertexDelta) > 1e-10f || glm::length2(normalDelta) > 1e-10f)
                        changes.push_back({ vertexDelta, normalDelta, vertexOffset + vertex });
                }
                if (!validTarget)
                {
                    CW_ENGINE_WARN("Skipping morph target '{}' because its transformed vertex data is invalid.", target.mName.C_Str());
                    continue;
                }

                String name = target.mName.C_Str();
                if (name.empty())
                    name = String(source.mName.C_Str()) + "/Morph_" + std::to_string(morphIndex);

                const auto existing = morphIndices.find(name);
                if (existing != morphIndices.end())
                {
                    auto& vertices = morphs[existing->second].Vertices;
                    vertices.insert(vertices.end(), std::make_move_iterator(changes.begin()), std::make_move_iterator(changes.end()));
                    continue;
                }

                const float shapeWeight = std::isfinite(target.mWeight) && target.mWeight > 0.0f ? target.mWeight : 1.0f;
                morphIndices.emplace(name, static_cast<uint32_t>(morphs.size()));
                morphs.push_back({ std::move(name), shapeWeight, std::move(changes) });
            }
        }

        MeshImportResult ParseScene(const aiScene& scene, const MeshImportOptions& options)
        {
            MeshImportResult result;
            UnorderedMap<String, uint32_t, StringHash, StringEqual> boneIndices;
            if (options.ImportBones)
                boneIndices = ReadBones(scene, options, result);

            Vector<Ref<MeshData>> meshes;
            Vector<Vector<SubMesh>> meshSubMeshes;
            Vector<MorphChannelBuilder> morphs;
            UnorderedMap<String, uint32_t, StringHash, StringEqual> morphIndices;
            uint32_t vertexOffset = 0;
            const float scaleFactor = GetScaleFactor(options);

            meshes.reserve(scene.mNumMeshes);
            meshSubMeshes.reserve(scene.mNumMeshes);
            result.MaterialIndices.reserve(scene.mNumMeshes);
            Vector<uint8_t> referencedMeshes(scene.mNumMeshes, 0);

            const auto appendInstance = [&](uint32_t meshIndex, const glm::mat4& nodeToScene, StringView instanceName) {
                if (meshIndex >= scene.mNumMeshes)
                {
                    CW_ENGINE_WARN("Skipping mesh instance '{}' because it references missing mesh {}.", instanceName, meshIndex);
                    return;
                }

                referencedMeshes[meshIndex] = 1;
                const aiMesh& source = *scene.mMeshes[meshIndex];
                MeshInstanceTransform instanceTransform;
                if (!BuildInstanceTransform(nodeToScene, scaleFactor, instanceName, instanceTransform))
                    return;
                if (options.ImportBones && source.HasBones() && instanceTransform.HasNodeTransform)
                {
                    CW_ENGINE_WARN("Skipping transformed skinned mesh instance '{}' because Crowny stores one inverse bind pose per bone.",
                                   instanceName);
                    return;
                }

                DrawMode drawMode = DrawMode::TRIANGLE_LIST;
                Ref<MeshData> mesh = ReadMesh(source, options, boneIndices, instanceTransform, instanceName, drawMode);
                if (!mesh)
                    return;

                if (mesh->GetVertexCount() > std::numeric_limits<uint32_t>::max() - vertexOffset)
                {
                    CW_ENGINE_WARN("Skipping mesh instance '{}' because combined vertex offsets exceed 32-bit storage.", instanceName);
                    return;
                }

                if (options.ImportMorphMeshes)
                    ReadMorphs(source, vertexOffset, instanceTransform, morphs, morphIndices);

                meshSubMeshes.push_back({ SubMesh(0, mesh->GetIndexCount(), drawMode) });
                result.MaterialIndices.push_back(source.mMaterialIndex);
                vertexOffset += mesh->GetVertexCount();
                meshes.push_back(std::move(mesh));
            };

            std::function<void(const aiNode*, const glm::mat4&)> visitNode = [&](const aiNode* node, const glm::mat4& parentToScene) {
                const glm::mat4 nodeToScene = parentToScene * ToGlm(node->mTransformation);
                for (uint32_t nodeMeshIndex = 0; nodeMeshIndex < node->mNumMeshes; nodeMeshIndex++)
                    appendInstance(node->mMeshes[nodeMeshIndex], nodeToScene, node->mName.C_Str());
                for (uint32_t child = 0; child < node->mNumChildren; child++)
                    visitNode(node->mChildren[child], nodeToScene);
            };
            visitNode(scene.mRootNode, glm::mat4(1.0f));

            for (uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; meshIndex++)
            {
                if (referencedMeshes[meshIndex] != 0)
                    continue;
                const aiMesh& source = *scene.mMeshes[meshIndex];
                CW_ENGINE_WARN("Mesh '{}' is not referenced by the imported scene hierarchy; importing it once without a node transform.",
                               source.mName.C_Str());
                appendInstance(meshIndex, glm::mat4(1.0f), source.mName.C_Str());
            }

            if (meshes.empty())
                return result;

            if (meshes.size() == 1)
            {
                result.Data = meshes.front();
                result.SubMeshes = meshSubMeshes.front();
            }
            else
            {
                result.Data = MeshData::Combine(meshes, meshSubMeshes, result.SubMeshes);
            }

            if (!morphs.empty() && result.Data)
            {
                Vector<Ref<MorphChannel>> channels;
                channels.reserve(morphs.size());
                for (MorphChannelBuilder& morph : morphs)
                {
                    Ref<MorphShape> shape = MorphShape::Create(morph.Name, morph.ShapeWeight, std::move(morph.Vertices));
                    channels.push_back(MorphChannel::Create(morph.Name, { std::move(shape) }));
                }
                result.Morph = MeshMorph::Create(std::move(channels), result.Data->GetVertexCount());
            }

            return result;
        }

        const aiMesh* FindAnimatedMesh(const aiScene& scene, StringView channelName)
        {
            for (uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; meshIndex++)
            {
                if (StringView(scene.mMeshes[meshIndex]->mName.C_Str()) == channelName)
                    return scene.mMeshes[meshIndex];
            }

            std::function<const aiNode*(const aiNode*)> findNode = [&](const aiNode* node) -> const aiNode* {
                if (StringView(node->mName.C_Str()) == channelName)
                    return node;
                for (uint32_t child = 0; child < node->mNumChildren; child++)
                {
                    if (const aiNode* found = findNode(node->mChildren[child]))
                        return found;
                }
                return nullptr;
            };
            const aiNode* node = findNode(scene.mRootNode);
            if (node != nullptr && node->mNumMeshes > 0 && node->mMeshes[0] < scene.mNumMeshes)
                return scene.mMeshes[node->mMeshes[0]];
            return nullptr;
        }

        String GetMorphTargetName(const aiScene& scene, StringView channelName, uint32_t targetIndex)
        {
            const aiMesh* mesh = FindAnimatedMesh(scene, channelName);
            if (mesh != nullptr && targetIndex < mesh->mNumAnimMeshes)
            {
                String name = mesh->mAnimMeshes[targetIndex]->mName.C_Str();
                if (!name.empty())
                    return name;
            }
            for (uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; meshIndex++)
            {
                mesh = scene.mMeshes[meshIndex];
                if (targetIndex >= mesh->mNumAnimMeshes)
                    continue;
                String name = mesh->mAnimMeshes[targetIndex]->mName.C_Str();
                if (!name.empty())
                    return name;
            }
            return String(channelName) + "/Morph_" + std::to_string(targetIndex);
        }

        template <typename T> AnimationCurve<T> SliceCurve(const AnimationCurve<T>& curve, float start, float end)
        {
            if (curve.IsEmpty() || end <= start)
                return {};

            Vector<KeyFrame<T>> keys;
            keys.reserve(curve.GetKeyFrameCount() + 2);
            KeyFrame<T> first;
            first.Time = 0.0f;
            first.Value = curve.Evaluate(start, AnimationWrapMode::Clamp);
            keys.push_back(first);
            for (const KeyFrame<T>& source : curve.GetKeyFrames())
            {
                if (source.Time <= start || source.Time >= end)
                    continue;
                KeyFrame<T> key = source;
                key.Time -= start;
                keys.push_back(std::move(key));
            }
            KeyFrame<T> last;
            last.Time = end - start;
            last.Value = curve.Evaluate(end, AnimationWrapMode::Clamp);
            keys.push_back(last);
            return AnimationCurve<T>(std::move(keys));
        }

        Ref<AnimationClip> SliceClip(const AnimationClip& source, const ExtraAnimationClipInfo& range)
        {
            const float sampleRate = source.GetSampleRate();
            const float start = static_cast<float>(range.StartFrame) / sampleRate;
            const float end = static_cast<float>(range.EndFrame) / sampleRate;
            if (end <= start)
                return nullptr;

            Vector<AnimationTransformTrack> transformTracks;
            transformTracks.reserve(source.GetTransformTracks().size());
            for (const AnimationTransformTrack& sourceTrack : source.GetTransformTracks())
            {
                transformTracks.push_back({ sourceTrack.Name, SliceCurve(sourceTrack.Position, start, end),
                                            SliceCurve(sourceTrack.Rotation, start, end), SliceCurve(sourceTrack.Scale, start, end) });
            }
            Vector<AnimationMorphTrack> morphTracks;
            morphTracks.reserve(source.GetMorphTracks().size());
            for (const AnimationMorphTrack& sourceTrack : source.GetMorphTracks())
                morphTracks.push_back({ sourceTrack.Name, SliceCurve(sourceTrack.Weight, start, end) });
            Vector<AnimationGenericTrack> genericTracks;
            genericTracks.reserve(source.GetGenericTracks().size());
            for (const AnimationGenericTrack& sourceTrack : source.GetGenericTracks())
                genericTracks.push_back({ sourceTrack.Name, SliceCurve(sourceTrack.Curve, start, end) });

            RootMotionCurves rootMotion{ SliceCurve(source.GetRootMotion().Position, start, end),
                                         SliceCurve(source.GetRootMotion().Rotation, start, end) };
            Ref<AnimationClip> clip = AnimationClip::Create(std::move(transformTracks), std::move(morphTracks), std::move(genericTracks),
                                                            std::move(rootMotion), sampleRate, source.IsAdditive());
            Vector<AnimationEvent> events;
            for (const AnimationEvent& event : source.GetEvents())
            {
                if (event.Time >= start && event.Time <= end)
                    events.push_back({ event.Name, event.Time - start, event.Payload });
            }
            clip->SetEvents(std::move(events));
            return clip;
        }

        Vector<Ref<AnimationClip>> ImportAnimationClips(const aiScene& scene, const MeshImportOptions& options, const Ref<Skeleton>& skeleton)
        {
            Vector<Ref<AnimationClip>> clips;
            for (uint32_t animationIndex = 0; animationIndex < scene.mNumAnimations; animationIndex++)
            {
                const aiAnimation& animation = *scene.mAnimations[animationIndex];
                const double ticksPerSecond = animation.mTicksPerSecond > 0.0 ? animation.mTicksPerSecond : 30.0;
                Vector<AnimationTransformTrack> transformTracks;
                transformTracks.reserve(animation.mNumChannels);
                for (uint32_t channelIndex = 0; channelIndex < animation.mNumChannels; channelIndex++)
                {
                    const aiNodeAnim& channel = *animation.mChannels[channelIndex];
                    AnimationTransformTrack track;
                    track.Name = channel.mNodeName.C_Str();

                    Vector<KeyFrame<glm::vec3>> positions;
                    positions.reserve(channel.mNumPositionKeys);
                    for (uint32_t keyIndex = 0; keyIndex < channel.mNumPositionKeys; keyIndex++)
                    {
                        const aiVectorKey& key = channel.mPositionKeys[keyIndex];
                        positions.push_back({ static_cast<float>(key.mTime / ticksPerSecond), ToGlm(key.mValue) * GetScaleFactor(options) });
                    }
                    track.Position = AnimationCurve<glm::vec3>(std::move(positions));

                    Vector<KeyFrame<glm::quat>> rotations;
                    rotations.reserve(channel.mNumRotationKeys);
                    for (uint32_t keyIndex = 0; keyIndex < channel.mNumRotationKeys; keyIndex++)
                    {
                        const aiQuatKey& key = channel.mRotationKeys[keyIndex];
                        rotations.push_back({ static_cast<float>(key.mTime / ticksPerSecond), ToGlm(key.mValue) });
                    }
                    track.Rotation = AnimationCurve<glm::quat>(std::move(rotations));

                    Vector<KeyFrame<glm::vec3>> scales;
                    scales.reserve(channel.mNumScalingKeys);
                    for (uint32_t keyIndex = 0; keyIndex < channel.mNumScalingKeys; keyIndex++)
                    {
                        const aiVectorKey& key = channel.mScalingKeys[keyIndex];
                        scales.push_back({ static_cast<float>(key.mTime / ticksPerSecond), ToGlm(key.mValue) });
                    }
                    track.Scale = AnimationCurve<glm::vec3>(std::move(scales));
                    transformTracks.push_back(std::move(track));
                }

                UnorderedMap<String, Vector<KeyFrame<float>>, StringHash, StringEqual> morphKeys;
                for (uint32_t channelIndex = 0; channelIndex < animation.mNumMorphMeshChannels; channelIndex++)
                {
                    const aiMeshMorphAnim& channel = *animation.mMorphMeshChannels[channelIndex];
                    UnorderedSet<uint32_t> targets;
                    for (uint32_t keyIndex = 0; keyIndex < channel.mNumKeys; keyIndex++)
                    {
                        const aiMeshMorphKey& sourceKey = channel.mKeys[keyIndex];
                        for (uint32_t value = 0; value < sourceKey.mNumValuesAndWeights; value++)
                            targets.emplace(sourceKey.mValues[value]);
                    }

                    for (uint32_t target : targets)
                    {
                        const String targetName = GetMorphTargetName(scene, channel.mName.C_Str(), target);
                        Vector<KeyFrame<float>>& keys = morphKeys[targetName];
                        keys.reserve(keys.size() + channel.mNumKeys);
                        for (uint32_t keyIndex = 0; keyIndex < channel.mNumKeys; keyIndex++)
                        {
                            const aiMeshMorphKey& sourceKey = channel.mKeys[keyIndex];
                            float weight = 0.0f;
                            for (uint32_t value = 0; value < sourceKey.mNumValuesAndWeights; value++)
                            {
                                if (sourceKey.mValues[value] == target)
                                {
                                    weight = static_cast<float>(sourceKey.mWeights[value]);
                                    break;
                                }
                            }
                            keys.push_back({ static_cast<float>(sourceKey.mTime / ticksPerSecond), weight });
                        }
                    }
                }

                Vector<AnimationMorphTrack> morphTracks;
                morphTracks.reserve(morphKeys.size());
                for (auto& [name, keys] : morphKeys)
                    morphTracks.push_back({ name, AnimationCurve<float>(std::move(keys)) });
                std::stable_sort(morphTracks.begin(), morphTracks.end(),
                                 [](const AnimationMorphTrack& left, const AnimationMorphTrack& right) { return left.Name < right.Name; });

                RootMotionCurves rootMotion;
                if (options.ImportRootMotion && skeleton)
                {
                    const uint32_t rootIndex = skeleton->GetRootBoneIndex();
                    if (rootIndex != INVALID_BONE_INDEX)
                    {
                        const String& rootName = skeleton->GetBone(rootIndex).Name;
                        auto rootTrack = std::find_if(transformTracks.begin(), transformTracks.end(),
                                                      [&](const AnimationTransformTrack& track) { return track.Name == rootName; });
                        if (rootTrack == transformTracks.end())
                        {
                            rootTrack = std::find_if(transformTracks.begin(), transformTracks.end(), [&](const AnimationTransformTrack& track) {
                                return skeleton->FindBone(track.Name) >= 0 && (!track.Position.IsEmpty() || !track.Rotation.IsEmpty());
                            });
                        }
                        if (rootTrack != transformTracks.end())
                        {
                            rootMotion.Position = std::move(rootTrack->Position);
                            rootMotion.Rotation = std::move(rootTrack->Rotation);
                        }
                    }
                }

                Ref<AnimationClip> sourceClip = AnimationClip::Create(std::move(transformTracks), std::move(morphTracks), {}, std::move(rootMotion),
                                                                      static_cast<float>(ticksPerSecond));
                String name = animation.mName.C_Str();
                if (name.empty())
                    name = "Animation_" + std::to_string(animationIndex);
                sourceClip->SetName(name);

                if (options.AnimationInfo.empty())
                {
                    clips.push_back(std::move(sourceClip));
                }
                else
                {
                    for (const ExtraAnimationClipInfo& range : options.AnimationInfo)
                    {
                        Ref<AnimationClip> split = SliceClip(*sourceClip, range);
                        if (!split)
                        {
                            CW_ENGINE_WARN("Skipping invalid animation range '{}' ({}..{}).", range.Name, range.StartFrame, range.EndFrame);
                            continue;
                        }
                        split->SetName(scene.mNumAnimations > 1 ? name + "/" + range.Name : range.Name);
                        clips.push_back(std::move(split));
                    }
                }

                if (animation.mNumMeshChannels != 0)
                    CW_ENGINE_WARN("Animation '{}' contains legacy mesh-key channels, which Assimp does not expose as skeletal or morph curves.",
                                   name);
            }
            return clips;
        }

        UUID ImportedEntityId(const String& key)
        {
            // Fixed hashes of source-relative node paths, independent of machine and import order.
            uint64_t first = 14695981039346656037ull;
            uint64_t second = 7809847782465536322ull;
            for (unsigned char value : key)
            {
                first = (first ^ value) * 1099511628211ull;
                second = (second ^ value) * 1099511628211ull;
            }
            return UUID(static_cast<uint32_t>(first >> 32), static_cast<uint32_t>(first), static_cast<uint32_t>(second >> 32),
                        static_cast<uint32_t>(second));
        }

        glm::quat ImportedOrientation(const aiVector3D& direction, const aiVector3D& sourceUp)
        {
            glm::vec3 forward = ToGlm(direction);
            if (!IsFinite(forward) || glm::dot(forward, forward) < 1e-8f)
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            forward = glm::normalize(forward);
            glm::vec3 up = ToGlm(sourceUp);
            if (!IsFinite(up) || glm::dot(up, up) < 1e-8f || glm::length(glm::cross(forward, up)) < 1e-4f)
                up = std::abs(forward.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            return glm::quatLookAtRH(forward, glm::normalize(up));
        }

        Ref<Prefab> ImportPrefab(const aiScene& source, const Path& path, const MeshImportOptions& options)
        {
            const Ref<Prefab> prefab = CreateRef<Prefab>();
            prefab->SetName(path.stem().string() + " Prefab");
            const Ref<Scene>& scene = prefab->GetInternalScene();
            scene->CreateRootEntity();
            Entity root = scene->CreateEntityWithUuid(ImportedEntityId("model"), path.stem().string());
            prefab->SetRootEntityUuid(root.GetUuid());
            const float scaleFactor = GetScaleFactor(options);
            String extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool gltf = extension == ".gltf" || extension == ".glb";
            std::function<void(const aiNode&, Entity, const String&)> visit = [&](const aiNode& node, Entity parent, const String& key) {
                Entity entity = scene->CreateEntityWithUuid(ImportedEntityId(key), node.mName.C_Str());
                entity.SetParent(parent);
                entity.GetTransform().SetLocalTransform(ToTransform(node.mTransformation, scaleFactor));
                if (options.ImportLights)
                    for (uint32_t index = 0; index < source.mNumLights; index++)
                    {
                        const aiLight& light = *source.mLights[index];
                        if (light.mName != node.mName)
                            continue;
                        if (light.mType != aiLightSource_DIRECTIONAL && light.mType != aiLightSource_POINT && light.mType != aiLightSource_SPOT)
                        {
                            CW_ENGINE_WARN("Light '{}' uses an unsupported source light type {}.", light.mName.C_Str(),
                                           static_cast<int>(light.mType));
                            continue;
                        }
                        Entity child = scene->CreateEntityWithUuid(ImportedEntityId(key + "/@light"), String(node.mName.C_Str()) + " Light");
                        child.SetParent(entity);
                        child.GetTransform().SetPosition(ToGlm(light.mPosition) * scaleFactor);
                        child.GetTransform().SetRotation(ImportedOrientation(light.mDirection, light.mUp));
                        auto& component = child.AddComponent<LightComponent>();
                        component.Type = light.mType == aiLightSource_DIRECTIONAL ? LightType::Directional
                                         : light.mType == aiLightSource_SPOT      ? LightType::Spot
                                                                                  : LightType::Point;
                        const glm::vec3 radiance =
                          glm::max(glm::vec3(light.mColorDiffuse.r, light.mColorDiffuse.g, light.mColorDiffuse.b), glm::vec3(0));
                        component.Intensity = std::max({ radiance.x, radiance.y, radiance.z });
                        component.Color = component.Intensity > 0 ? radiance / component.Intensity : glm::vec3(1);
                        component.SpotInnerAngle = std::clamp(light.mAngleInnerCone, 0.0f, glm::half_pi<float>());
                        component.SpotOuterAngle = std::clamp(light.mAngleOuterCone, component.SpotInnerAngle, glm::half_pi<float>());
                        // Assimp folds glTF lux/candela into RGB. Crowny stores local lights in lumens.
                        if (component.Type == LightType::Point)
                            component.Intensity *= 4.0f * glm::pi<float>();
                        else if (component.Type == LightType::Spot)
                            component.Intensity *= std::max(2.0f * glm::pi<float>() * (1.0f - std::cos(component.SpotOuterAngle)), 0.001f);
                        float range = 0.0f;
                        if (node.mMetaData && node.mMetaData->Get("PBR_LightRange", range) && range > 0.0)
                            component.Range = range * std::abs(scaleFactor);
                        else if (component.Type != LightType::Directional)
                            component.Range = 1000.0f * std::abs(scaleFactor);
                    }
                if (options.ImportCameras)
                    for (uint32_t index = 0; index < source.mNumCameras; index++)
                    {
                        const aiCamera& camera = *source.mCameras[index];
                        if (camera.mName != node.mName)
                            continue;
                        Entity child = scene->CreateEntityWithUuid(ImportedEntityId(key + "/@camera"), String(node.mName.C_Str()) + " Camera");
                        child.SetParent(entity);
                        child.GetTransform().SetPosition(ToGlm(camera.mPosition) * scaleFactor);
                        child.GetTransform().SetRotation(ImportedOrientation(camera.mLookAt, camera.mUp));
                        auto& output = child.AddComponent<CameraComponent>().Camera;
                        const float aspect = camera.mAspect > 0 ? camera.mAspect : 1.0f;
                        output.SetAspectRatio(aspect);
                        const float nearClip = std::max(camera.mClipPlaneNear * std::abs(scaleFactor), 0.0001f);
                        const float farClip = std::max(camera.mClipPlaneFar * std::abs(scaleFactor), nearClip + 0.001f);
                        if (camera.mOrthographicWidth > 0)
                            output.SetOrthographic(2.0f * camera.mOrthographicWidth * std::abs(scaleFactor) / aspect, nearClip, farClip);
                        else
                        {
                            // This Assimp revision exposes full FOV for glTF, half FOV for FBX.
                            const float halfFov = camera.mHorizontalFOV * (gltf ? 0.5f : 1.0f);
                            output.SetPerspective(2.0f * std::atan(std::tan(halfFov) / aspect), nearClip, farClip);
                        }
                    }
                UnorderedMap<String, uint32_t> occurrences;
                for (uint32_t index = 0; index < node.mNumChildren; index++)
                {
                    const aiNode& child = *node.mChildren[index];
                    const String name = child.mName.C_Str();
                    visit(child, entity, key + "/" + std::to_string(name.size()) + ":" + name + "#" + std::to_string(occurrences[name]++));
                }
            };
            visit(*source.mRootNode, root, "model/source");
            return prefab;
        }

        struct ImportedTexture
        {
            Ref<Texture> TextureAsset;
            bool Cutout = false;
        };
        struct TextureCache
        {
            UnorderedMap<String, ImportedTexture> Textures;
            bool FastCompression = true;
        };

        Ref<TextureImportOptions> CreateMaterialTextureOptions(TextureMipMode mode, bool sRGB, bool fastCompression)
        {
            Ref<TextureImportOptions> options = CreateRef<TextureImportOptions>();
            options->MipMode = mode;
            options->SRGB = sRGB;
            options->DiskFormat = !fastCompression && mode == TextureMipMode::Color ? TextureDiskFormat::ETC1S : TextureDiskFormat::UASTC;
            options->UASTCEffort = fastCompression ? 1u : 2u;
            return options;
        }

        Ref<PixelData> DecodeEmbeddedTexels(const aiTexture& embedded)
        {
            if (embedded.mWidth == 0 || embedded.mHeight == 0 || embedded.pcData == nullptr)
                return nullptr;
            Ref<PixelData> pixels = PixelData::Create(embedded.mWidth, embedded.mHeight, 1, TextureFormat::RGBA8);
            if (!pixels || !pixels->IsValid())
                return nullptr;
            for (uint32_t y = 0; y < embedded.mHeight; y++)
            {
                for (uint32_t x = 0; x < embedded.mWidth; x++)
                {
                    const aiTexel& texel = embedded.pcData[static_cast<size_t>(y) * embedded.mWidth + x];
                    uint8_t* destination = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch() + x * 4u;
                    destination[0] = texel.r;
                    destination[1] = texel.g;
                    destination[2] = texel.b;
                    destination[3] = texel.a;
                }
            }
            return pixels;
        }

        bool HasVaryingAlpha(const Ref<PixelData>& pixels)
        {
            if (!pixels || !PixelUtils::HasAlpha(pixels->GetFormat()))
                return false;
            float minimum = 1.0f, maximum = 0.0f;
            const bool byteAlpha = pixels->GetFormat() == TextureFormat::RGBA8 || pixels->GetFormat() == TextureFormat::BGRA8;
            for (uint32_t y = 0; y < pixels->GetHeight(); ++y)
            {
                const uint8_t* row = pixels->GetData() + static_cast<size_t>(y) * pixels->GetRowPitch();
                for (uint32_t x = 0; x < pixels->GetWidth(); ++x)
                {
                    const float alpha = byteAlpha ? row[static_cast<size_t>(x) * 4u + 3u] / 255.0f : pixels->GetColorAt(x, y).a;
                    minimum = std::min(minimum, alpha);
                    maximum = std::max(maximum, alpha);
                    if (minimum < 0.5f && maximum > 0.5f)
                        return true;
                }
            }
            return false;
        }

        bool ReadAlbedoCoverage(const aiTexture* embedded, const Path& path)
        {
            Path cacheRoot;
            if (const auto* application = Application::TryGet(); application && !application->GetInternalDirectory().empty())
            {
                const Path& internal = application->GetInternalDirectory();
                cacheRoot = (internal.filename() == "Assets" ? internal.parent_path() : internal) / "ImportCache/Alpha-v1";
            }
            String source;
            if (!cacheRoot.empty())
            {
                if (embedded)
                {
                    const uint64_t bytes =
                      embedded->mHeight ? static_cast<uint64_t>(embedded->mWidth) * embedded->mHeight * sizeof(aiTexel) : embedded->mWidth;
                    if (bytes < 512 * 1024 * 1024)
                        source.assign(reinterpret_cast<const char*>(embedded->pcData), static_cast<size_t>(bytes));
                }
                else
                {
                    std::ifstream stream(path, std::ios::binary | std::ios::ate);
                    const auto bytes = stream.tellg();
                    if (bytes > 0 && bytes < 512 * 1024 * 1024)
                    {
                        source.resize(static_cast<size_t>(bytes));
                        stream.seekg(0);
                        if (!stream.read(source.data(), bytes))
                            source.clear();
                    }
                }
            }
            const String sourceKind =
              embedded && embedded->mHeight ? std::to_string(embedded->mWidth) + "x" + std::to_string(embedded->mHeight) : "encoded";
            const String digest = source.empty() ? String{} : Cryptography::SHA256(sourceKind + "|" + Cryptography::SHA256(source));
            const Path cachePath = digest.empty() ? Path{} : cacheRoot / (digest + ".coverage");
            if (!cachePath.empty())
            {
                std::ifstream stream(cachePath, std::ios::binary | std::ios::ate);
                if (stream.tellg() == 65)
                {
                    String entry(65, '\0');
                    stream.seekg(0);
                    if (stream.read(entry.data(), entry.size()) && (entry.back() == '0' || entry.back() == '1') &&
                        entry.substr(0, 64) == Cryptography::SHA256("alpha-v1|" + digest + "|" + entry.back()))
                        return entry.back() == '1';
                }
            }
            const Ref<PixelData> pixels =
              embedded ? (embedded->mHeight ? DecodeEmbeddedTexels(*embedded)
                                            : ImageLoader::DecodeMemory(reinterpret_cast<const uint8_t*>(embedded->pcData), embedded->mWidth).Pixels)
                       : ImageLoader::Decode(path).Pixels;
            const bool cutout = HasVaryingAlpha(pixels);
            if (pixels && !cachePath.empty())
            {
                const char value = cutout ? '1' : '0';
                const String entry = Cryptography::SHA256("alpha-v1|" + digest + "|" + value) + value;
                FileSystem::WriteFileAtomic(cachePath, reinterpret_cast<const byte*>(entry.data()), entry.size());
            }
            return cutout;
        }

        struct MaterialTextureRequest
        {
            String Key;
            String Name;
            Path File;
            const aiTexture* Embedded = nullptr;
            TextureMipMode Mode = TextureMipMode::Color;
            bool SRGB = true;
            bool Coverage = false;
        };

        bool DescribeTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const Path& meshPath, aiTextureType textureType,
                             TextureMipMode mode, bool sRGB, MaterialTextureRequest& request)
        {
            aiString importedPath;
            if (sourceMaterial.GetTexture(textureType, 0, &importedPath) != aiReturn_SUCCESS)
                return false;
            const String rawPath = importedPath.C_Str();
            request.Name = rawPath;
            const String profileKey = "|" + std::to_string(static_cast<uint32_t>(mode)) + (sRGB ? "|srgb" : "|linear");
            const aiTexture* embedded = request.Embedded = scene.GetEmbeddedTexture(rawPath.c_str());
            Path& texturePath = request.File;
            request.Key = embedded != nullptr ? "embedded:" + rawPath + profileKey : [&]() {
                texturePath = Path(rawPath);
                if (texturePath.is_relative())
                    texturePath = meshPath.parent_path() / texturePath;
                texturePath = texturePath.lexically_normal();
                return texturePath.generic_string() + profileKey;
            }();

            request.Mode = mode;
            request.SRGB = sRGB;
            return true;
        }

        struct MaterialTextureCooks
        {
            std::mutex Mutex;
            UnorderedMap<String, std::shared_future<Ref<Texture>>> Pending;

            Ref<Texture> Import(const Path& path, const Ref<TextureImportOptions>& options)
            {
                const auto cook = [&]() -> Ref<Texture> {
                    const auto assets = Importer::Get().ImportAllDeferred(path, options);
                    return !assets.empty() && assets.front()->GetAssetType() == AssetType::Texture ? StaticRefCast<Texture>(assets.front()) : nullptr;
                };
                // Coalesce identical source bytes within this import, including the
                // first-ever import. This table never survives the current batch.
                String bytes;
                std::ifstream input(path, std::ios::binary | std::ios::ate);
                const auto size = input.tellg();
                if (size <= 0 || size > 32 * 1024 * 1024)
                    return cook();
                const auto* importer = Importer::Get().GetImporterForFile(path);
                if (!importer || typeid(*importer) != typeid(TextureImporter))
                    return cook();
                bytes.resize(static_cast<size_t>(size));
                input.seekg(0);
                if (!input.read(bytes.data(), size))
                    return cook();
                const auto probe = ImageLoader::ProbeMemory(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
                if (!probe || probe.Info.Container != ImageContainerFormat::Raster || probe.Info.BitDepth > 8 || probe.Info.IsFloat ||
                    probe.Info.IsHDR)
                    return cook();
                // All other settings are fixed by CreateMaterialTextureOptions.
                const String key = Cryptography::SHA256(bytes) + "|" + path.extension().string() + "|" +
                                   std::to_string(static_cast<uint32_t>(options->MipMode)) + (options->SRGB ? "|srgb" : "|linear");
                bytes.clear();
                std::shared_future<Ref<Texture>> pending;
                std::promise<Ref<Texture>> producer;
                bool owner;
                {
                    std::lock_guard lock(Mutex);
                    const auto [entry, inserted] = Pending.emplace(key, producer.get_future().share());
                    owner = inserted;
                    pending = entry->second;
                }
                if (owner)
                {
                    try
                    {
                        auto texture = cook();
                        producer.set_value(texture);
                        return texture;
                    }
                    catch (...)
                    {
                        producer.set_exception(std::current_exception());
                        throw;
                    }
                }
                const auto original = pending.get();
                if (!original)
                    return nullptr;
                if (!original->HasEncodedSourceData())
                    return cook();
                // Different source names remain different assets and keep their
                // existing UUID reconciliation. Only their CPU cook is shared.
                auto desc = original->GetDesc();
                desc.DebugName = path.filename().string();
                auto texture = Texture::CreateDeferred(desc);
                texture->SetEncodedSourceData(original->GetDiskFormat(), original->GetSourceFormat(), original->GetEncodedSourceData());
                texture->SetCpuCached(original->IsCpuCached());
                texture->SetName(desc.DebugName);
                return texture;
            }
        };

        ImportedTexture PrepareMaterialTexture(const MaterialTextureRequest& request, const Path& meshPath, bool fastCompression,
                                               MaterialTextureCooks* cooks = nullptr)
        {
            const aiTexture* embedded = request.Embedded;
            const String& rawPath = request.Name;
            Path texturePath = request.File;
            const Ref<TextureImportOptions> options = CreateMaterialTextureOptions(request.Mode, request.SRGB, fastCompression);
            const auto importFile = [&options, cooks](const Path& path) -> Ref<Texture> {
                if (cooks)
                    return cooks->Import(path, options);
                const auto assets = Importer::Get().ImportAllDeferred(path, options);
                return !assets.empty() && assets.front()->GetAssetType() == AssetType::Texture ? StaticRefCast<Texture>(assets.front()) : nullptr;
            };
            Ref<Texture> texture;
            if (embedded != nullptr)
            {
                if (embedded->mHeight == 0)
                {
                    texture =
                      TextureImporter::ImportFromMemory(reinterpret_cast<const uint8_t*>(embedded->pcData), embedded->mWidth, rawPath, options);
                }
                else
                {
                    const Ref<PixelData> pixels = DecodeEmbeddedTexels(*embedded);
                    texture = TextureImporter::ImportFromPixels(pixels, rawPath, options, true);
                }
            }
            else
            {
                if (FileSystem::FileExists(texturePath))
                    texture = importFile(texturePath);
                if (!texture)
                {
                    // Source files frequently reference textures by the author's absolute path, or as GPU-compressed containers
                    // (.dds) the image loader cannot decode. Look next to the mesh (and in a Textures folder beside it) for a file
                    // with the same stem in a supported format, so scenes only need their textures converted, not relinked.
                    const Path meshDirectory = meshPath.parent_path();
                    const Path stem = texturePath.stem();
                    const Path directories[] = { texturePath.parent_path(), meshDirectory, meshDirectory / "Textures", meshDirectory / "textures" };
                    for (const Path& directory : directories)
                    {
                        for (const char* extension : { ".png", ".tga", ".jpg", ".jpeg", ".hdr" })
                        {
                            const Path candidate = directory / Path(stem.string() + extension);
                            if (candidate == texturePath || !fs::exists(candidate))
                                continue;
                            texture = importFile(candidate);
                            if (texture)
                            {
                                texturePath = candidate;
                                break;
                            }
                        }
                        if (texture)
                            break;
                    }
                }
            }
            if (!texture)
            {
                CW_ENGINE_WARN("Failed to import texture '{}' referenced by '{}'.", rawPath, meshPath);
                return {};
            }

            bool cutout = false;
            if (request.Coverage)
            {
                // Constant-zero alpha remains unused; explicit source alpha modes below take precedence.
                cutout = ReadAlbedoCoverage(embedded, texturePath);
            }
            return { texture, cutout };
        }

        Ref<Texture> ImportTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const Path& meshPath, aiTextureType textureType,
                                   const String& shaderParameter, TextureMipMode mode, bool sRGB, const Ref<Material>& material,
                                   TextureCache& textureCache)
        {
            MaterialTextureRequest request;
            if (!DescribeTexture(scene, sourceMaterial, meshPath, textureType, mode, sRGB, request))
                return nullptr;
            auto cached = textureCache.Textures.find(request.Key);
            if (cached == textureCache.Textures.end())
            {
                // A preferred source failed. Resolve later semantic fallbacks on
                // the caller thread without eagerly cooking unused texture slots.
                request.Coverage = shaderParameter == "albedoMap";
                auto prepared = PrepareMaterialTexture(request, meshPath, textureCache.FastCompression);
                if (prepared.TextureAsset)
                    prepared.TextureAsset->Init();
                cached = textureCache.Textures.emplace(request.Key, std::move(prepared)).first;
            }
            if (!cached->second.TextureAsset)
                return nullptr;
            material->SetTexture(shaderParameter, cached->second.TextureAsset);
            if (shaderParameter == "albedoMap" && cached->second.Cutout)
                material->SetAlphaMode(AlphaMode::Mask);
            return cached->second.TextureAsset;
        }

        TextureCache PrepareMaterialTextures(const aiScene& scene, const Path& meshPath, const Vector<uint32_t>& materialIndices,
                                             bool fastCompression)
        {
            Vector<MaterialTextureRequest> requests;
            UnorderedMap<String, size_t> indices;
            const auto add = [&](const aiMaterial& material, aiTextureType type, TextureMipMode mode, bool sRGB, bool coverage) {
                MaterialTextureRequest request;
                if (!DescribeTexture(scene, material, meshPath, type, mode, sRGB, request))
                    return false;
                const auto [entry, inserted] = indices.emplace(request.Key, requests.size());
                if (inserted)
                    requests.push_back(std::move(request));
                requests[entry->second].Coverage |= coverage;
                return true;
            };
            for (uint32_t index : materialIndices)
            {
                if (index >= scene.mNumMaterials)
                    continue;
                const auto& material = *scene.mMaterials[index];
                for (auto type : { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE })
                    if (add(material, type, TextureMipMode::Color, true, true))
                        break;
                for (auto type : { aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS })
                    add(material, type, TextureMipMode::Data, false, false);
                for (auto type : { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP })
                    if (add(material, type, TextureMipMode::Data, false, false))
                        break;
                for (auto type : { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT })
                    if (add(material, type, TextureMipMode::NormalMap, false, false))
                        break;
                for (auto type : { aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE })
                    if (add(material, type, TextureMipMode::Color, true, false))
                        break;
            }
            Vector<ImportedTexture> prepared(requests.size());
            MaterialTextureCooks cooks;
            std::atomic<size_t> next{ 0 };
            const auto prepare = [&] {
                for (size_t index = next.fetch_add(1); index < requests.size(); index = next.fetch_add(1))
                    prepared[index] = PrepareMaterialTexture(requests[index], meshPath, fastCompression, &cooks);
            };
            // Respect custom importers too. A main-thread or serialized override for
            // any source/fallback format keeps this batch on the caller thread.
            UnorderedSet<String> extensions{ ".png", ".tga", ".jpg", ".jpeg", ".hdr" };
            for (const auto& request : requests)
                if (!request.Embedded)
                    extensions.insert(request.File.extension().string());
            bool parallelSafe = true;
            for (const auto& extension : extensions)
            {
                if (!Importer::Get().SupportsFileType(extension))
                    continue;
                const auto* importer = Importer::Get().GetImporterForFile(Path("texture" + extension));
                parallelSafe &= importer && importer->GetThreadingPolicy() == ImporterThreadingPolicy::ParallelWorker;
            }
            const size_t workerCount = parallelSafe ? std::min<size_t>({ 4, std::max(1u, std::thread::hardware_concurrency()), requests.size() }) : 1;
            Vector<std::future<void>> workers;
            for (size_t worker = 1; worker < workerCount; ++worker)
                workers.push_back(std::async(std::launch::async, prepare));
            prepare();
            for (auto& worker : workers)
                worker.get();
            TextureCache cache;
            cache.FastCompression = fastCompression;
            // Publish on the caller thread, in source order. Workers only create deferred CPU assets.
            for (size_t index = 0; index < requests.size(); ++index)
            {
                if (prepared[index].TextureAsset)
                    prepared[index].TextureAsset->Init();
                cache.Textures.emplace(std::move(requests[index].Key), std::move(prepared[index]));
            }
            return cache;
        }

        Ref<Texture> ImportFirstTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const Path& meshPath,
                                        std::initializer_list<aiTextureType> textureTypes, const String& shaderParameter, TextureMipMode mode,
                                        bool sRGB, const Ref<Material>& material, TextureCache& textureCache)
        {
            for (aiTextureType type : textureTypes)
            {
                Ref<Texture> texture = ImportTexture(scene, sourceMaterial, meshPath, type, shaderParameter, mode, sRGB, material, textureCache);
                if (texture)
                    return texture;
            }
            return nullptr;
        }

        Vector<Ref<Asset>> ImportMaterials(const aiScene& scene, const Path& meshPath, const Vector<uint32_t>& materialIndices, bool fastCompression)
        {
            Vector<Ref<Asset>> assets;
            TextureCache textureCache = PrepareMaterialTextures(scene, meshPath, materialIndices, fastCompression);
            const AssetHandle<Shader> pbrShader = AssetManager::TryGet()->Load<Shader>(PBRIBL_SHADER_PATH);

            // ParseScene emits submeshes in node-instance order, which can differ
            // from the source mesh table and can contain repeated mesh instances.
            for (uint32_t materialIndex : materialIndices)
            {
                if (materialIndex >= scene.mNumMaterials)
                {
                    CW_ENGINE_WARN("Mesh '{}' references missing material {}.", meshPath, materialIndex);
                    const Ref<Material> fallback = Material::CreatePBR(pbrShader);
                    fallback->SetName("Material_" + std::to_string(materialIndex));
                    assets.push_back(fallback);
                    continue;
                }

                const aiMaterial& sourceMaterial = *scene.mMaterials[materialIndex];
                const Ref<Material> material = Material::CreatePBR(pbrShader);
                String materialName = sourceMaterial.GetName().C_Str();
                if (materialName.empty())
                    materialName = "Material_" + std::to_string(materialIndex);
                material->SetName(materialName);

                const auto addTexture = [&assets](const Ref<Texture>& texture) {
                    if (texture)
                        assets.push_back(texture);
                };
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, "albedoMap",
                                              TextureMipMode::Color, true, material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_METALNESS }, "metallicMap", TextureMipMode::Data,
                                              false, material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_DIFFUSE_ROUGHNESS }, "roughnessMap",
                                              TextureMipMode::Data, false, material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath,
                                              { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT }, "normalMap",
                                              TextureMipMode::NormalMap, false, material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP }, "aoMap",
                                              TextureMipMode::Data, false, material, textureCache));

                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE },
                                              "emissiveMap", TextureMipMode::Color, true, material, textureCache));
                aiColor3D emission(0.0f, 0.0f, 0.0f);
                if (sourceMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, emission) == aiReturn_SUCCESS)
                    material->SetColor("emissive", { emission.r, emission.g, emission.b, 1.0f });
                float emissionIntensity = 1.0f;
                sourceMaterial.Get(AI_MATKEY_EMISSIVE_INTENSITY, emissionIntensity);
                material->SetFloat("emissiveIntensity", emissionIntensity);

                aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
                if (sourceMaterial.Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS ||
                    sourceMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
                    material->SetColor("albedo", { color.r, color.g, color.b, color.a });
                float opacity = 1.0f;
                sourceMaterial.Get(AI_MATKEY_OPACITY, opacity);
                aiColor4D baseColor;
                // glTF exposes the same alpha through both base color and opacity.
                if (sourceMaterial.Get(AI_MATKEY_BASE_COLOR, baseColor) != aiReturn_SUCCESS)
                    color.a *= opacity;
                color.a = std::clamp(color.a, 0.0f, 1.0f);
                material->SetColor("albedo", { color.r, color.g, color.b, color.a });
                if (color.a < 1.0f)
                    material->SetAlphaMode(AlphaMode::WeightedOIT);
                aiString alphaMode;
                if (sourceMaterial.Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn_SUCCESS)
                {
                    const String mode = alphaMode.C_Str();
                    material->SetAlphaMode(mode == "MASK" ? AlphaMode::Mask : mode == "BLEND" ? AlphaMode::WeightedOIT : AlphaMode::Opaque);
                }
                float alphaCutoff = 0.5f;
                sourceMaterial.Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
                material->SetFloat("alphaCutoff", alphaCutoff);

                float metalness = 0.0f;
                if (sourceMaterial.Get(AI_MATKEY_METALLIC_FACTOR, metalness) == aiReturn_SUCCESS)
                    material->SetFloat("metalness", metalness);

                float roughness = 1.0f;
                if (sourceMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS)
                    material->SetFloat("roughness", roughness);

                assets.push_back(material);
            }
            return assets;
        }
    } // namespace

    bool MeshImporter::IsExtensionSupported(const String& ext) const
    {
        Assimp::Importer importer;
        return importer.IsExtensionSupported("." + ext);
    }

    bool MeshImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    MeshImportResult MeshImporter::Parse(const Path& path, const MeshImportOptions& importOptions, String* outError)
    {
        Assimp::Importer importer;
        const aiScene* scene = ReadScene(importer, path, importOptions);
        if (scene == nullptr)
        {
            if (outError != nullptr)
            {
                const char* error = importer.GetErrorString();
                *outError = error != nullptr && *error != 0 ? error : "The mesh reader produced no scene";
            }
            return {};
        }
        return ParseScene(*scene, importOptions);
    }

    Ref<Prefab> MeshImporter::ParsePrefab(const Path& path, const MeshImportOptions& importOptions)
    {
        Assimp::Importer importer;
        const aiScene* scene = ReadScene(importer, path, importOptions);
        return scene ? ImportPrefab(*scene, path, importOptions) : nullptr;
    }

    Ref<Asset> MeshImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Vector<Ref<Asset>> assets = ImportAll(path, importOptions);
        return assets.empty() ? nullptr : assets.front();
    }

    Vector<Ref<Asset>> MeshImporter::ImportAll(const Path& path, Ref<const ImportOptions> importOptions)
    {
        const Ref<const MeshImportOptions> options = StaticRefCast<const MeshImportOptions>(importOptions);
        Assimp::Importer importer;
        const aiScene* scene = ReadScene(importer, path, *options);
        if (!scene)
            return {};

        MeshImportResult parsed = ParseScene(*scene, *options);
        if (!parsed)
        {
            CW_ENGINE_WARN("Mesh import produced no mesh data: {}", path);
            return {};
        }

        MeshDesc desc;
        desc.Data = parsed.Data;
        desc.Usage = options->CpuCached || parsed.MeshSkeleton || parsed.Morph ? MeshUsage::CpuCached : MeshUsage::Static;
        desc.Topology = parsed.SubMeshes.size() == 1 ? parsed.SubMeshes.front().MeshDrawMode : DrawMode::TRIANGLE_LIST;
        desc.Morph = parsed.Morph;
        desc.MeshSkeleton = parsed.MeshSkeleton;
        desc.SubMeshes = parsed.SubMeshes;
        if (options->GenerateMeshlets || options->GenerateLods)
        {
            MeshProcessingSettings processingSettings;
            processingSettings.LodCount = options->GenerateLods ? options->LodCount : 1u;
            processingSettings.GenerateMeshlets = options->GenerateMeshlets;
            desc.GpuGeometry = MeshProcessing::BuildGpuGeometry(*parsed.Data, parsed.SubMeshes, processingSettings);
        }

        const Ref<Mesh> mesh = Mesh::Create(desc);
        mesh->SetName(path.filename().string());

        Vector<Ref<Asset>> assets{ mesh };
        if (options->GenerateCollision)
        {
            PhysicsMeshBuildSettings collisionSettings;
            collisionSettings.MaxConvexPoints = options->CollisionMaxConvexPoints;
            const Ref<PhysicsMesh> collision = PhysicsMesh::Build(*parsed.Data, parsed.SubMeshes, collisionSettings);
            if (collision != nullptr)
            {
                // Dependents are keyed by "<type>:<name>#<n>", so the name must be deterministic across reimports.
                collision->SetName(path.stem().string() + " Collision");
                assets.push_back(collision);
            }
            else
                CW_ENGINE_WARN("Mesh import produced no collision geometry: {}", path);
        }
        if (options->ImportAnimations)
        {
            const Vector<Ref<AnimationClip>> animations = ImportAnimationClips(*scene, *options, parsed.MeshSkeleton);
            assets.insert(assets.end(), animations.begin(), animations.end());
        }
        if (options->ImportMaterials)
        {
            const Vector<Ref<Asset>> materialAssets = ImportMaterials(*scene, path, parsed.MaterialIndices, options->FastTextureCompression);
            assets.insert(assets.end(), materialAssets.begin(), materialAssets.end());
        }
        if (options->GeneratePrefab)
        {
            const Ref<Prefab> prefab = ImportPrefab(*scene, path, *options);
            auto& renderer = prefab->GetRootEntity().AddComponent<MeshRendererComponent>();
            renderer.MeshHandle = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(mesh));
            for (const Ref<Asset>& asset : assets)
                if (asset->GetAssetType() == AssetType::Material)
                    renderer.Materials.push_back(static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(asset)));
            assets.push_back(prefab);
        }
        return assets;
    }

    Ref<ImportOptions> MeshImporter::CreateImportOptions() const { return CreateRef<MeshImportOptions>(); }
} // namespace Crowny
