#include "cwpch.h"

#include "Crowny/Common/Noise.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/ModifierNodes.h"
#include "Crowny/Renderer/Mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Crowny
{
    static BufferLayout GetStandardLayout()
    {
        return BufferLayout({ { ShaderDataType::Float3, VertexAttribute::Position },
                              { ShaderDataType::Float3, VertexAttribute::Normal },
                              { ShaderDataType::Float3, VertexAttribute::Tangent },
                              { ShaderDataType::Float2, VertexAttribute::TexCoord0 } });
    }

    // ---- TransformGeometryNode ----

    TransformGeometryNode::TransformGeometryNode(UUID id) : Node(id, "TransformGeometryNode")
    {
        AddInput("Geometry", PinDataType::MeshData);
        AddInput("Translation", PinDataType::Vec3, glm::vec3(0.0f));
        AddInput("Rotation", PinDataType::Vec3, glm::vec3(0.0f));
        AddInput("Scale", PinDataType::Vec3, glm::vec3(1.0f));
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void TransformGeometryNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        Ref<MeshData> input = GetInputValue<Ref<MeshData>>("Geometry", evaluator);
        if (!input)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", nullptr, evaluator);
            return;
        }

        glm::vec3 translation = GetInputValue<glm::vec3>("Translation", evaluator);
        glm::vec3 rotation = GetInputValue<glm::vec3>("Rotation", evaluator);
        glm::vec3 scale = GetInputValue<glm::vec3>("Scale", evaluator);

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation);
        transform *= glm::eulerAngleYXZ(glm::radians(rotation.y), glm::radians(rotation.x), glm::radians(rotation.z));
        transform = glm::scale(transform, scale);
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        Vector<glm::vec3> positions = input->GetPositions();
        Vector<glm::vec3> normals = input->GetNormals();
        Vector<glm::vec3> tangents = input->GetTangents();

        for (auto& pos : positions)
            pos = glm::vec3(transform * glm::vec4(pos, 1.0f));
        for (auto& n : normals)
            n = glm::normalize(normalMatrix * n);
        for (auto& t : tangents)
            t = glm::normalize(normalMatrix * t);

        uint32_t vertCount = input->GetVertexCount();
        uint32_t idxCount = input->GetIndexCount();
        auto result = MeshData::Create(vertCount, idxCount, GetStandardLayout());

        result->SetPositions(positions);
        result->SetNormals(normals);
        result->SetTangents(tangents);
        result->SetUVs(0, input->GetUVs(0));
        result->SetIndices(input->GetIndices());

        SetOutputValue<Ref<MeshData>>("Geometry", result, evaluator);
    }

    // ---- MergeGeometryNode ----

    MergeGeometryNode::MergeGeometryNode(UUID id) : Node(id, "MergeGeometryNode")
    {
        AddInput("A", PinDataType::MeshData);
        AddInput("B", PinDataType::MeshData);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void MergeGeometryNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        Ref<MeshData> a = GetInputValue<Ref<MeshData>>("A", evaluator);
        Ref<MeshData> b = GetInputValue<Ref<MeshData>>("B", evaluator);

        if (!a && !b)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", nullptr, evaluator);
            return;
        }
        if (!a)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", b, evaluator);
            return;
        }
        if (!b)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", a, evaluator);
            return;
        }

        // Manual merge: combine vertex and index data
        uint32_t vertCountA = a->GetVertexCount();
        uint32_t vertCountB = b->GetVertexCount();
        uint32_t idxCountA = a->GetIndexCount();
        uint32_t idxCountB = b->GetIndexCount();

        auto result = MeshData::Create(vertCountA + vertCountB, idxCountA + idxCountB, GetStandardLayout());

        Vector<glm::vec3> positions = a->GetPositions();
        Vector<glm::vec3> normals = a->GetNormals();
        Vector<glm::vec3> tangents = a->GetTangents();
        Vector<glm::vec2> uvs = a->GetUVs(0);
        Vector<uint32_t> indices = a->GetIndices();

        auto posB = b->GetPositions();
        auto normB = b->GetNormals();
        auto tanB = b->GetTangents();
        auto uvB = b->GetUVs(0);
        auto idxB = b->GetIndices();

        positions.insert(positions.end(), posB.begin(), posB.end());
        normals.insert(normals.end(), normB.begin(), normB.end());
        tangents.insert(tangents.end(), tanB.begin(), tanB.end());
        uvs.insert(uvs.end(), uvB.begin(), uvB.end());

        for (uint32_t idx : idxB)
            indices.push_back(idx + vertCountA);

        result->SetPositions(positions);
        result->SetNormals(normals);
        result->SetTangents(tangents);
        result->SetUVs(0, uvs);
        result->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>("Geometry", result, evaluator);
    }

    // ---- NoiseDisplaceNode ----

    NoiseDisplaceNode::NoiseDisplaceNode(UUID id) : Node(id, "NoiseDisplaceNode")
    {
        AddInput("Geometry", PinDataType::MeshData);
        AddInput("Strength", PinDataType::Float, 0.5f);
        AddInput("Frequency", PinDataType::Float, 1.0f);
        AddInput("Octaves", PinDataType::Int, 4);
        AddInput("Seed", PinDataType::Int, 0);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void NoiseDisplaceNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        Ref<MeshData> input = GetInputValue<Ref<MeshData>>("Geometry", evaluator);
        if (!input)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", nullptr, evaluator);
            return;
        }

        float strength = GetInputValue<float>("Strength", evaluator);
        float frequency = GetInputValue<float>("Frequency", evaluator);
        int32_t octaves = std::max(1, GetInputValue<int32_t>("Octaves", evaluator));
        int32_t seed = GetInputValue<int32_t>("Seed", evaluator);

        NoiseOptions noiseOps;
        noiseOps.Octaves = octaves;
        noiseOps.Smoothness = 1.0f;
        noiseOps.Roughness = 0.5f;
        noiseOps.Seed = seed;
        noiseOps.NoiseFunc = NoiseFunc::Perlin;

        Vector<glm::vec3> positions = input->GetPositions();
        Vector<glm::vec3> normals = input->GetNormals();

        for (size_t i = 0; i < positions.size(); i++)
        {
            glm::vec3 samplePos = positions[i] * frequency;
            float noiseVal = Noise::Noise3D(noiseOps, samplePos);
            glm::vec3 normal = (i < normals.size()) ? normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            positions[i] += normal * noiseVal * strength;
        }

        uint32_t vertCount = input->GetVertexCount();
        uint32_t idxCount = input->GetIndexCount();
        auto result = MeshData::Create(vertCount, idxCount, GetStandardLayout());

        result->SetPositions(positions);
        result->SetNormals(normals);
        result->SetTangents(input->GetTangents());
        result->SetUVs(0, input->GetUVs(0));
        result->SetIndices(input->GetIndices());

        SetOutputValue<Ref<MeshData>>("Geometry", result, evaluator);
    }

    // ---- RecalculateNormalsNode ----

    RecalculateNormalsNode::RecalculateNormalsNode(UUID id) : Node(id, "RecalculateNormalsNode")
    {
        AddInput("Geometry", PinDataType::MeshData);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void RecalculateNormalsNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        Ref<MeshData> input = GetInputValue<Ref<MeshData>>("Geometry", evaluator);
        if (!input)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", nullptr, evaluator);
            return;
        }

        Vector<glm::vec3> positions = input->GetPositions();
        Vector<uint32_t> indices = input->GetIndices();
        uint32_t vertCount = input->GetVertexCount();
        uint32_t idxCount = input->GetIndexCount();

        Vector<glm::vec3> normals(vertCount, glm::vec3(0.0f));

        // Accumulate face normals
        for (uint32_t i = 0; i + 2 < idxCount; i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                continue;

            glm::vec3 edge1 = positions[i1] - positions[i0];
            glm::vec3 edge2 = positions[i2] - positions[i0];
            glm::vec3 faceNormal = glm::cross(edge1, edge2);

            normals[i0] += faceNormal;
            normals[i1] += faceNormal;
            normals[i2] += faceNormal;
        }

        for (auto& n : normals)
        {
            float len = glm::length(n);
            if (len > 0.0001f)
                n /= len;
        }

        auto result = MeshData::Create(vertCount, idxCount, GetStandardLayout());

        result->SetPositions(positions);
        result->SetNormals(normals);
        result->SetTangents(input->GetTangents());
        result->SetUVs(0, input->GetUVs(0));
        result->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>("Geometry", result, evaluator);
    }

    // ---- SubdivideNode ----

    SubdivideNode::SubdivideNode(UUID id) : Node(id, "SubdivideNode")
    {
        AddInput("Geometry", PinDataType::MeshData);
        AddInput("Iterations", PinDataType::Int, 1);
        AddInput("Smooth", PinDataType::Bool, false);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void SubdivideNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        Ref<MeshData> input = GetInputValue<Ref<MeshData>>("Geometry", evaluator);
        if (!input)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", nullptr, evaluator);
            return;
        }

        int iterations = GetInputValue<int>("Iterations", evaluator);
        bool smooth = GetInputValue<bool>("Smooth", evaluator);

        if (iterations <= 0)
        {
            SetOutputValue<Ref<MeshData>>("Geometry", input, evaluator);
            return;
        }

        if (iterations > 4)
            iterations = 4;

        Ref<MeshData> current = input;

        for (int iter = 0; iter < iterations; ++iter)
        {
            uint32_t numVerts = current->GetVertexCount();
            uint32_t numIndices = current->GetIndexCount();
            auto indices = current->GetIndices();
            auto positions = current->GetPositions();
            auto normals = current->GetNormals();
            auto tangents = current->GetTangents();
            auto uvs = current->GetUVs(0);

            struct Edge
            {
                uint32_t v1, v2;
                bool operator<(const Edge& other) const { return v1 < other.v1 || (v1 == other.v1 && v2 < other.v2); }
            };

            std::map<Edge, uint32_t> midpoints;
            auto getMidpoint = [&](uint32_t v1, uint32_t v2) {
                if (v1 > v2)
                    std::swap(v1, v2);
                Edge e{ v1, v2 };
                if (midpoints.find(e) != midpoints.end())
                    return midpoints[e];

                uint32_t newIdx = (uint32_t)positions.size();
                positions.push_back((positions[v1] + positions[v2]) * 0.5f);
                if (!normals.empty())
                    normals.push_back(glm::normalize(normals[v1] + normals[v2]));
                if (!tangents.empty())
                    tangents.push_back(glm::normalize(tangents[v1] + tangents[v2]));
                if (!uvs.empty())
                    uvs.push_back((uvs[v1] + uvs[v2]) * 0.5f);

                midpoints[e] = newIdx;
                return newIdx;
            };

            Vector<uint32_t> newIndices;
            for (uint32_t i = 0; i < numIndices; i += 3)
            {
                uint32_t v1 = indices[i];
                uint32_t v2 = indices[i + 1];
                uint32_t v3 = indices[i + 2];

                uint32_t m12 = getMidpoint(v1, v2);
                uint32_t m23 = getMidpoint(v2, v3);
                uint32_t m31 = getMidpoint(v3, v1);

                newIndices.push_back(v1);
                newIndices.push_back(m12);
                newIndices.push_back(m31);

                newIndices.push_back(v2);
                newIndices.push_back(m23);
                newIndices.push_back(m12);

                newIndices.push_back(v3);
                newIndices.push_back(m31);
                newIndices.push_back(m23);

                newIndices.push_back(m12);
                newIndices.push_back(m23);
                newIndices.push_back(m31);
            }

            if (smooth)
            {
                // Basic Laplacian smoothing for old vertices
                Vector<glm::vec3> smoothedPositions = positions;
                Vector<uint32_t> neighborCount(positions.size(), 0);
                Vector<glm::vec3> neighborSum(positions.size(), glm::vec3(0.0f));

                auto addNeighbor = [&](uint32_t v, uint32_t n) {
                    neighborSum[v] += positions[n];
                    neighborCount[v]++;
                };

                for (uint32_t i = 0; i < (uint32_t)newIndices.size(); i += 3)
                {
                    uint32_t v1 = newIndices[i];
                    uint32_t v2 = newIndices[i + 1];
                    uint32_t v3 = newIndices[i + 2];

                    addNeighbor(v1, v2);
                    addNeighbor(v1, v3);
                    addNeighbor(v2, v1);
                    addNeighbor(v2, v3);
                    addNeighbor(v3, v1);
                    addNeighbor(v3, v2);
                }

                for (uint32_t i = 0; i < (uint32_t)positions.size(); ++i)
                {
                    if (neighborCount[i] > 0)
                    {
                        glm::vec3 centroid = neighborSum[i] / (float)neighborCount[i];
                        smoothedPositions[i] = positions[i] * 0.5f + centroid * 0.5f;
                    }
                }
                positions = smoothedPositions;
            }

            current = MeshData::Create((uint32_t)positions.size(), (uint32_t)newIndices.size(), GetStandardLayout());
            current->SetPositions(positions);
            if (!normals.empty())
                current->SetNormals(normals);
            if (!tangents.empty())
                current->SetTangents(tangents);
            if (!uvs.empty())
                current->SetUVs(0, uvs);
            current->SetIndices(newIndices);
        }

        SetOutputValue<Ref<MeshData>>("Geometry", current, evaluator);
    }

} // namespace Crowny
