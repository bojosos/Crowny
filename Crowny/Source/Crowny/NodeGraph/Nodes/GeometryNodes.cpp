#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/GeometryNodes.h"
#include "Crowny/Renderer/Mesh.h"

#include <glm/gtc/constants.hpp>

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

    BoxNode::BoxNode(UUID id) : Node(id, "BoxNode"_sid)
    {
        AddInput("Width"_sid, PinDataType::Float, 1.0f);
        AddInput("Height"_sid, PinDataType::Float, 1.0f);
        AddInput("Depth"_sid, PinDataType::Float, 1.0f);
        AddOutput("Geometry"_sid, PinDataType::MeshData);
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

    // ---- CylinderNode ----

    CylinderNode::CylinderNode(UUID id) : Node(id, "CylinderNode"_sid)
    {
        AddInput("Radius"_sid, PinDataType::Float, 0.5f);
        AddInput("Depth"_sid, PinDataType::Float, 1.0f);
        AddInput("Segments"_sid, PinDataType::Int, 32);
        AddInput("Capped"_sid, PinDataType::Bool, true);
        AddOutput("Geometry"_sid, PinDataType::MeshData);
    }

    void CylinderNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        const float radius = GetInputValue<float>("Radius", evaluator);
        const float depth = GetInputValue<float>("Depth", evaluator);
        const int32_t segments = GetInputValue<int32_t>("Segments", evaluator);
        const bool capped = GetInputValue<bool>("Capped", evaluator);
        if (radius <= 0.0f || depth <= 0.0f || segments < 3 || segments > 512)
        {
            evaluator.ReportError("Cylinder requires positive dimensions and 3 to 512 segments");
            return;
        }

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        const uint32_t ringCount = static_cast<uint32_t>(segments + 1);
        const float halfDepth = depth * 0.5f;
        positions.reserve(ringCount * (capped ? 4 : 2) + (capped ? 2 : 0));
        normals.reserve(positions.capacity());
        tangents.reserve(positions.capacity());
        uvs.reserve(positions.capacity());
        indices.reserve(static_cast<size_t>(segments) * (capped ? 12 : 6));

        for (int32_t i = 0; i <= segments; ++i)
        {
            const float u = static_cast<float>(i) / static_cast<float>(segments);
            const float angle = u * glm::two_pi<float>();
            const float sine = std::sin(angle);
            const float cosine = std::cos(angle);
            const glm::vec3 normal(cosine, 0.0f, sine);
            const glm::vec3 tangent(-sine, 0.0f, cosine);
            positions.emplace_back(radius * cosine, -halfDepth, radius * sine);
            positions.emplace_back(radius * cosine, halfDepth, radius * sine);
            normals.push_back(normal);
            normals.push_back(normal);
            tangents.push_back(tangent);
            tangents.push_back(tangent);
            uvs.emplace_back(u, 0.0f);
            uvs.emplace_back(u, 1.0f);
        }
        for (uint32_t i = 0; i < static_cast<uint32_t>(segments); ++i)
        {
            const uint32_t bottom = i * 2;
            const uint32_t top = bottom + 1;
            indices.insert(indices.end(), { bottom, bottom + 2, top + 2, bottom, top + 2, top });
        }

        if (capped)
        {
            const auto addCap = [&](float y, float normalY, bool reverse) {
                const uint32_t center = static_cast<uint32_t>(positions.size());
                positions.emplace_back(0.0f, y, 0.0f);
                normals.emplace_back(0.0f, normalY, 0.0f);
                tangents.emplace_back(1.0f, 0.0f, 0.0f);
                uvs.emplace_back(0.5f, 0.5f);
                const uint32_t ringStart = static_cast<uint32_t>(positions.size());
                for (int32_t i = 0; i <= segments; ++i)
                {
                    const float angle = static_cast<float>(i) / static_cast<float>(segments) * glm::two_pi<float>();
                    const float sine = std::sin(angle);
                    const float cosine = std::cos(angle);
                    positions.emplace_back(radius * cosine, y, radius * sine);
                    normals.emplace_back(0.0f, normalY, 0.0f);
                    tangents.emplace_back(1.0f, 0.0f, 0.0f);
                    uvs.emplace_back(cosine * 0.5f + 0.5f, sine * 0.5f + 0.5f);
                }
                for (uint32_t i = 0; i < static_cast<uint32_t>(segments); ++i)
                {
                    if (reverse)
                        indices.insert(indices.end(), { center, ringStart + i + 1, ringStart + i });
                    else
                        indices.insert(indices.end(), { center, ringStart + i, ringStart + i + 1 });
                }
            };
            addCap(halfDepth, 1.0f, false);
            addCap(-halfDepth, -1.0f, true);
        }

        const Ref<MeshData> mesh =
          MeshData::Create(static_cast<uint32_t>(positions.size()), static_cast<uint32_t>(indices.size()), GetStandardLayout());
        mesh->SetPositions(positions);
        mesh->SetNormals(normals);
        mesh->SetTangents(tangents);
        mesh->SetUVs(0, uvs);
        mesh->SetIndices(indices);
        SetOutputValue("Geometry", mesh, evaluator);
    }

    // ---- SphereNode ----

    SphereNode::SphereNode(UUID id) : Node(id, "SphereNode"_sid)
    {
        AddInput("Radius"_sid, PinDataType::Float, 1.0f);
        AddInput("Segments"_sid, PinDataType::Int, 32);
        AddInput("Rings"_sid, PinDataType::Int, 16);
        AddOutput("Geometry"_sid, PinDataType::MeshData);
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

    PlaneNode::PlaneNode(UUID id) : Node(id, "PlaneNode"_sid)
    {
        AddInput("Width"_sid, PinDataType::Float, 1.0f);
        AddInput("Height"_sid, PinDataType::Float, 1.0f);
        AddInput("SubdivisionsX"_sid, PinDataType::Int, 1);
        AddInput("SubdivisionsY"_sid, PinDataType::Int, 1);
        AddOutput("Geometry"_sid, PinDataType::MeshData);
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

    GridNode::GridNode(UUID id) : Node(id, "GridNode"_sid)
    {
        AddInput("Size"_sid, PinDataType::Float, 10.0f);
        AddInput("Resolution"_sid, PinDataType::Int, 10);
        AddOutput("Geometry"_sid, PinDataType::MeshData);
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
