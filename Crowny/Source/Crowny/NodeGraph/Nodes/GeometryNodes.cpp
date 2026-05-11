#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/GeometryNodes.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    static BufferLayout GetStandardLayout()
    {
        return BufferLayout({ { ShaderDataType::Float3, VertexAttribute::Position },
                              { ShaderDataType::Float3, VertexAttribute::Normal },
                              { ShaderDataType::Float3, VertexAttribute::Tangent },
                              { ShaderDataType::Float2, VertexAttribute::TexCoord0 } });
    }

    // ---- BoxNode ----

    BoxNode::BoxNode(UUID id) : Node(id, "BoxNode")
    {
        AddInput("Width", PinDataType::Float, 1.0f);
        AddInput("Height", PinDataType::Float, 1.0f);
        AddInput("Depth", PinDataType::Float, 1.0f);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void BoxNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID geometryPin("Geometry");
        static const StringID widthPin("Width");
        static const StringID heightPin("Height");
        static const StringID depthPin("Depth");

        const float w = GetInputValue<float>(widthPin, evaluator) * 0.5f;
        const float h = GetInputValue<float>(heightPin, evaluator) * 0.5f;
        const float d = GetInputValue<float>(depthPin, evaluator) * 0.5f;

        // 24 vertices (4 per face), 36 indices
        const auto meshData = MeshData::Create(24, 36, GetStandardLayout());

        Vector<glm::vec3> positions = { // Front (+Z)
                                        { -w, -h, d },
                                        { w, -h, d },
                                        { w, h, d },
                                        { -w, h, d },
                                        // Back (-Z)
                                        { w, -h, -d },
                                        { -w, -h, -d },
                                        { -w, h, -d },
                                        { w, h, -d },
                                        // Top (+Y)
                                        { -w, h, d },
                                        { w, h, d },
                                        { w, h, -d },
                                        { -w, h, -d },
                                        // Bottom (-Y)
                                        { -w, -h, -d },
                                        { w, -h, -d },
                                        { w, -h, d },
                                        { -w, -h, d },
                                        // Right (+X)
                                        { w, -h, d },
                                        { w, -h, -d },
                                        { w, h, -d },
                                        { w, h, d },
                                        // Left (-X)
                                        { -w, -h, -d },
                                        { -w, -h, d },
                                        { -w, h, d },
                                        { -w, h, -d }
        };

        Vector<glm::vec3> normals = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 },
                                      { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, -1, 0 }, { 0, -1, 0 }, { 0, -1, 0 },
                                      { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 } };

        Vector<glm::vec3> tangents = {
            { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
            { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },  { 1, 0, 0 },
            { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, 1 },  { 0, 0, 1 },  { 0, 0, 1 },  { 0, 0, 1 }
        };

        Vector<glm::vec2> uvs = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
                                  { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
                                  { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

        Vector<uint32_t> indices;
        indices.reserve(36);
        for (uint32_t face = 0; face < 6; face++)
        {
            const uint32_t base = face * 4;
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
            indices.push_back(base + 0);
        }

        meshData->SetPositions(positions);
        meshData->SetNormals(normals);
        meshData->SetTangents(tangents);
        meshData->SetUVs(0, uvs);
        meshData->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>(geometryPin, meshData, evaluator);
    }

    // ---- SphereNode ----

    SphereNode::SphereNode(UUID id) : Node(id, "SphereNode")
    {
        AddInput("Radius", PinDataType::Float, 1.0f);
        AddInput("Segments", PinDataType::Int, 32);
        AddInput("Rings", PinDataType::Int, 16);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void SphereNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID geometryPin("Geometry");
        static const StringID radiusPin("Radius");
        static const StringID segmentsPin("Segments");
        static const StringID ringsPin("Rings");

        const float radius = GetInputValue<float>(radiusPin, evaluator);
        const int32_t segments = std::max(3, GetInputValue<int32_t>(segmentsPin, evaluator));
        const int32_t rings = std::max(2, GetInputValue<int32_t>(ringsPin, evaluator));

        const uint32_t vertexCount = (segments + 1) * (rings + 1);
        const uint32_t indexCount = segments * rings * 6;

        const auto meshData = MeshData::Create(vertexCount, indexCount, GetStandardLayout());

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec2> uvs;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        uvs.reserve(vertexCount);

        for (int32_t y = 0; y <= rings; y++)
        {
            for (int32_t x = 0; x <= segments; x++)
            {
                const float xSeg = (float)x / (float)segments;
                const float ySeg = (float)y / (float)rings;
                const float xPos = std::cos(xSeg * 2.0f * (float)M_PI) * std::sin(ySeg * (float)M_PI);
                const float yPos = std::cos(ySeg * (float)M_PI);
                const float zPos = std::sin(xSeg * 2.0f * (float)M_PI) * std::sin(ySeg * (float)M_PI);

                const glm::vec3 normal(xPos, yPos, zPos);
                positions.push_back(normal * radius);
                normals.push_back(normal);
                uvs.push_back(glm::vec2(xSeg, ySeg));

                // Tangent is the derivative with respect to the longitude angle
                const glm::vec3 tangent(-std::sin(xSeg * 2.0f * (float)M_PI), 0.0f, std::cos(xSeg * 2.0f * (float)M_PI));
                tangents.push_back(glm::normalize(tangent));
            }
        }

        Vector<uint32_t> indices;
        indices.reserve(indexCount);

        for (int32_t y = 0; y < rings; y++)
        {
            for (int32_t x = 0; x < segments; x++)
            {
                const uint32_t current = y * (segments + 1) + x;
                const uint32_t next = current + segments + 1;

                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);

                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }

        meshData->SetPositions(positions);
        meshData->SetNormals(normals);
        meshData->SetTangents(tangents);
        meshData->SetUVs(0, uvs);
        meshData->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>(geometryPin, meshData, evaluator);
    }

    // ---- PlaneNode ----

    PlaneNode::PlaneNode(UUID id) : Node(id, "PlaneNode")
    {
        AddInput("Width", PinDataType::Float, 1.0f);
        AddInput("Height", PinDataType::Float, 1.0f);
        AddInput("SubdivisionsX", PinDataType::Int, 1);
        AddInput("SubdivisionsY", PinDataType::Int, 1);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void PlaneNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID geometryPin("Geometry");
        static const StringID widthPin("Width");
        static const StringID heightPin("Height");
        static const StringID subdivisionsXPin("SubdivisionsX");
        static const StringID subdivisionsYPin("SubdivisionsY");

        const float width = GetInputValue<float>(widthPin, evaluator);
        const float height = GetInputValue<float>(heightPin, evaluator);
        const int32_t subsX = std::max(1, GetInputValue<int32_t>(subdivisionsXPin, evaluator));
        const int32_t subsY = std::max(1, GetInputValue<int32_t>(subdivisionsYPin, evaluator));

        const uint32_t vertexCount = (subsX + 1) * (subsY + 1);
        const uint32_t indexCount = subsX * subsY * 6;

        const auto meshData = MeshData::Create(vertexCount, indexCount, GetStandardLayout());

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec2> uvs;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        uvs.reserve(vertexCount);

        for (int32_t y = 0; y <= subsY; y++)
        {
            for (int32_t x = 0; x <= subsX; x++)
            {
                const float u = (float)x / (float)subsX;
                const float v = (float)y / (float)subsY;
                positions.push_back(glm::vec3((u - 0.5f) * width, 0.0f, (v - 0.5f) * height));
                normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
                tangents.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
                uvs.push_back(glm::vec2(u, v));
            }
        }

        Vector<uint32_t> indices;
        indices.reserve(indexCount);

        for (int32_t y = 0; y < subsY; y++)
        {
            for (int32_t x = 0; x < subsX; x++)
            {
                const uint32_t topLeft = y * (subsX + 1) + x;
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = topLeft + subsX + 1;
                const uint32_t bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        meshData->SetPositions(positions);
        meshData->SetNormals(normals);
        meshData->SetTangents(tangents);
        meshData->SetUVs(0, uvs);
        meshData->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>(geometryPin, meshData, evaluator);
    }

    // ---- GridNode ----

    GridNode::GridNode(UUID id) : Node(id, "GridNode")
    {
        AddInput("Size", PinDataType::Float, 10.0f);
        AddInput("Resolution", PinDataType::Int, 10);
        AddOutput("Geometry", PinDataType::MeshData);
    }

    void GridNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID geometryPin("Geometry");
        static const StringID sizePin("Size");
        static const StringID resolutionPin("Resolution");

        const float size = GetInputValue<float>(sizePin, evaluator);
        const int32_t resolution = std::max(1, GetInputValue<int32_t>(resolutionPin, evaluator));

        const uint32_t vertexCount = (resolution + 1) * (resolution + 1);
        const uint32_t indexCount = resolution * resolution * 6;

        const auto meshData = MeshData::Create(vertexCount, indexCount, GetStandardLayout());

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec2> uvs;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        uvs.reserve(vertexCount);

        const float halfSize = size * 0.5f;
        const float step = size / (float)resolution;

        for (int32_t z = 0; z <= resolution; z++)
        {
            for (int32_t x = 0; x <= resolution; x++)
            {
                positions.push_back(glm::vec3(-halfSize + x * step, 0.0f, -halfSize + z * step));
                normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
                tangents.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
                uvs.push_back(glm::vec2((float)x / resolution, (float)z / resolution));
            }
        }

        Vector<uint32_t> indices;
        indices.reserve(indexCount);

        for (int32_t z = 0; z < resolution; z++)
        {
            for (int32_t x = 0; x < resolution; x++)
            {
                const uint32_t topLeft = z * (resolution + 1) + x;
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = topLeft + resolution + 1;
                const uint32_t bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        meshData->SetPositions(positions);
        meshData->SetNormals(normals);
        meshData->SetTangents(tangents);
        meshData->SetUVs(0, uvs);
        meshData->SetIndices(indices);

        SetOutputValue<Ref<MeshData>>(geometryPin, meshData, evaluator);
    }

} // namespace Crowny
