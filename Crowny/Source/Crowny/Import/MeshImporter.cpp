#include "cwpch.h"

#include "Crowny/Import/MeshImporter.h"

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MeshProcessing.h"

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

        bool IsFinite(const glm::vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

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

        bool BuildInstanceTransform(const glm::mat4& nodeToScene, float scaleFactor, StringView instanceName,
                                    MeshInstanceTransform& transform)
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
                positions[vertex] = glm::vec3(instanceTransform.NodeToScene * glm::vec4(ToGlm(mesh.mVertices[vertex]), 1.0f)) *
                                    instanceTransform.ScaleFactor;
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
                        if (!std::isfinite(sourceOrientation) || std::abs(sourceOrientation) <= std::numeric_limits<float>::epsilon() ||
                            !NormalizeDirection(tangents[vertex]))
                        {
                            CW_ENGINE_WARN("Skipping mesh instance '{}' because tangent basis {} is degenerate.", instanceName, vertex);
                            return nullptr;
                        }
                        bitangents[vertex] = glm::cross(normal, tangents[vertex]);
                        bitangents[vertex] *= sourceOrientation < 0.0f ? -1.0f : 1.0f;
                        bitangents[vertex] *= instanceTransform.Mirrored ? -1.0f : 1.0f;
                    }
                    else
                    {
                        bitangents[vertex] = instanceTransform.Linear * sourceBitangent;
                        if (!NormalizeDirection(tangents[vertex]) || !NormalizeDirection(bitangents[vertex]))
                        {
                            CW_ENGINE_WARN("Skipping mesh instance '{}' because tangent basis {} is degenerate.", instanceName, vertex);
                            return nullptr;
                        }
                    }
                }
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

        using TextureCache = UnorderedMap<String, Ref<Texture>>;

        Ref<Texture> ImportTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const Path& meshPath, aiTextureType textureType,
                                   const String& shaderParameter, const Ref<Material>& material, TextureCache& textureCache)
        {
            aiString importedPath;
            if (sourceMaterial.GetTexture(textureType, 0, &importedPath) != aiReturn_SUCCESS)
                return nullptr;

            const String rawPath = importedPath.C_Str();
            if (scene.GetEmbeddedTexture(rawPath.c_str()) != nullptr)
            {
                CW_ENGINE_WARN("Embedded texture '{}' cannot be imported until TextureImporter supports memory streams.", rawPath);
                return nullptr;
            }

            Path texturePath(rawPath);
            if (texturePath.is_relative())
                texturePath = meshPath.parent_path() / texturePath;
            texturePath = texturePath.lexically_normal();
            const String cacheKey = texturePath.generic_string();

            auto cached = textureCache.find(cacheKey);
            if (cached != textureCache.end())
            {
                material->SetTexture(shaderParameter, cached->second);
                return cached->second;
            }

            Ref<Texture> texture = Importer::Get().Import<Texture>(texturePath);
            if (!texture)
            {
                CW_ENGINE_WARN("Failed to import texture '{}' referenced by '{}'.", texturePath, meshPath);
                return nullptr;
            }

            textureCache.emplace(cacheKey, texture);
            material->SetTexture(shaderParameter, texture);
            return texture;
        }

        Ref<Texture> ImportFirstTexture(const aiScene& scene, const aiMaterial& sourceMaterial, const Path& meshPath,
                                        std::initializer_list<aiTextureType> textureTypes, const String& shaderParameter,
                                        const Ref<Material>& material, TextureCache& textureCache)
        {
            for (aiTextureType type : textureTypes)
            {
                Ref<Texture> texture = ImportTexture(scene, sourceMaterial, meshPath, type, shaderParameter, material, textureCache);
                if (texture)
                    return texture;
            }
            return nullptr;
        }

        Vector<Ref<Asset>> ImportMaterials(const aiScene& scene, const Path& meshPath)
        {
            Vector<Ref<Asset>> assets;
            TextureCache textureCache;
            const AssetHandle<Shader> pbrShader = AssetManager::TryGet()->Load<Shader>(PBRIBL_SHADER_PATH);

            for (uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; meshIndex++)
            {
                const aiMesh& mesh = *scene.mMeshes[meshIndex];
                if (mesh.mMaterialIndex >= scene.mNumMaterials)
                {
                    CW_ENGINE_WARN("Mesh '{}' references missing material {}.", mesh.mName.C_Str(), mesh.mMaterialIndex);
                    continue;
                }

                const aiMaterial& sourceMaterial = *scene.mMaterials[mesh.mMaterialIndex];
                const Ref<Material> material = Material::Create(pbrShader);
                String materialName = sourceMaterial.GetName().C_Str();
                if (materialName.empty())
                    materialName = "Material_" + std::to_string(mesh.mMaterialIndex);
                material->SetName(materialName);

                const auto addTexture = [&assets](const Ref<Texture>& texture) {
                    if (texture)
                        assets.push_back(texture);
                };
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, "albedoMap",
                                              material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_METALNESS }, "metallicMap", material, textureCache));
                addTexture(
                  ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_DIFFUSE_ROUGHNESS }, "roughnessMap", material, textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath,
                                              { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA, aiTextureType_HEIGHT }, "normalMap", material,
                                              textureCache));
                addTexture(ImportFirstTexture(scene, sourceMaterial, meshPath, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP }, "aoMap",
                                              material, textureCache));

                aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
                if (sourceMaterial.Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS ||
                    sourceMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
                    material->SetColor("albedo", { color.r, color.g, color.b, color.a });

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

    MeshImportResult MeshImporter::Parse(const Path& path, const MeshImportOptions& importOptions)
    {
        Assimp::Importer importer;
        const aiScene* scene = ReadScene(importer, path, importOptions);
        return scene ? ParseScene(*scene, importOptions) : MeshImportResult{};
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
        if (options->ImportAnimations)
        {
            const Vector<Ref<AnimationClip>> animations = ImportAnimationClips(*scene, *options, parsed.MeshSkeleton);
            assets.insert(assets.end(), animations.begin(), animations.end());
        }
        if (options->ImportMaterials)
        {
            const Vector<Ref<Asset>> materialAssets = ImportMaterials(*scene, path);
            assets.insert(assets.end(), materialAssets.begin(), materialAssets.end());
        }
        return assets;
    }

    Ref<ImportOptions> MeshImporter::CreateImportOptions() const { return CreateRef<MeshImportOptions>(); }
} // namespace Crowny
