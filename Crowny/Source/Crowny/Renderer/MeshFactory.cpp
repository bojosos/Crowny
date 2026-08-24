#include "cwpch.h"

#include "Crowny/Renderer/MeshFactory.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MAX_TESSELLATION = 4096;
        constexpr size_t MAX_PRIMITIVE_VERTICES = 256u * 1024u;
        constexpr size_t MAX_PRIMITIVE_INDICES = MAX_PRIMITIVE_VERTICES * 6u;

        BufferLayout GetPrimitiveLayout()
        {
            return { { ShaderDataType::Float3, VertexAttribute::Position },
                     { ShaderDataType::Float3, VertexAttribute::Normal },
                     { ShaderDataType::Float3, VertexAttribute::Tangent },
                     { ShaderDataType::Float3, VertexAttribute::Bitangent },
                     { ShaderDataType::Float2, VertexAttribute::TexCoord0 } };
        }

        bool IsPositiveFinite(float value) { return std::isfinite(value) && value > 0.0f; }

        bool IsFinite(const glm::vec2& value) { return std::isfinite(value.x) && std::isfinite(value.y); }

        bool IsFinite(const glm::vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool TryNormalize(const glm::vec3& value, glm::vec3& normalized)
        {
            if (!IsFinite(value))
                return false;
            const float scale = std::max({ std::abs(value.x), std::abs(value.y), std::abs(value.z) });
            if (!(scale > 0.0f))
                return false;
            const glm::vec3 scaled = value / scale;
            const float length = glm::length(scaled);
            if (!std::isfinite(length) || !(length > 0.0f))
                return false;
            normalized = scaled / length;
            return true;
        }

        bool ValidateSegments(uint32_t segments, uint32_t minimum, const char* shape)
        {
            if (segments >= minimum && segments <= MAX_TESSELLATION)
                return true;

            CW_ENGINE_ERROR("{} requires {} to {} segments, got {}.", shape, minimum, MAX_TESSELLATION, segments);
            return false;
        }

        bool ValidatePrimitiveCounts(size_t vertexCount, size_t indexCount, const char* shape)
        {
            if (vertexCount > 0 && indexCount > 0 && indexCount % 3u == 0u && vertexCount <= MAX_PRIMITIVE_VERTICES &&
                indexCount <= MAX_PRIMITIVE_INDICES && vertexCount <= std::numeric_limits<uint32_t>::max() &&
                indexCount <= std::numeric_limits<uint32_t>::max())
                return true;

            CW_ENGINE_ERROR("{} tessellation produces {} vertices and {} indices. The limits are {} vertices and {} indices.", shape,
                            vertexCount, indexCount, MAX_PRIMITIVE_VERTICES, MAX_PRIMITIVE_INDICES);
            return false;
        }

        IndexType SelectIndexType(size_t vertexCount)
        {
            return vertexCount <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1u ? IndexType::Index_16
                                                                                                 : IndexType::Index_32;
        }

        Ref<MeshData> BuildMeshData(const Vector<glm::vec3>& positions, const Vector<glm::vec3>& normals,
                                    const Vector<glm::vec3>& tangents, const Vector<glm::vec3>& bitangents,
                                    const Vector<glm::vec2>& uvs, const Vector<uint32_t>& indices)
        {
            const size_t vertexCount = positions.size();
            if (vertexCount == 0 || indices.empty() || normals.size() != vertexCount || tangents.size() != vertexCount ||
                bitangents.size() != vertexCount || uvs.size() != vertexCount || vertexCount > std::numeric_limits<uint32_t>::max() ||
                indices.size() > std::numeric_limits<uint32_t>::max() || indices.size() % 3u != 0u)
            {
                CW_ENGINE_ERROR("Cannot create primitive mesh from invalid vertex streams.");
                return nullptr;
            }

            for (size_t vertex = 0; vertex < vertexCount; ++vertex)
            {
                if (!IsFinite(positions[vertex]) || !IsFinite(normals[vertex]) || !IsFinite(tangents[vertex]) ||
                    !IsFinite(bitangents[vertex]) || !IsFinite(uvs[vertex]))
                {
                    CW_ENGINE_ERROR("Cannot create primitive mesh with non-finite vertex data.");
                    return nullptr;
                }
            }

            for (uint32_t index : indices)
            {
                if (index >= vertexCount)
                {
                    CW_ENGINE_ERROR("Cannot create primitive mesh with an out-of-range index.");
                    return nullptr;
                }
            }

            Ref<MeshData> data = MeshData::Create(static_cast<uint32_t>(vertexCount), static_cast<uint32_t>(indices.size()),
                                                  GetPrimitiveLayout(), SelectIndexType(vertexCount));
            data->SetPositions(positions);
            data->SetNormals(normals);
            data->SetTangents(tangents);
            data->SetBitangents(bitangents);
            data->SetUVs(0, uvs);
            data->SetIndices(indices);
            return data;
        }

        Ref<Mesh> BuildMesh(const Ref<MeshData>& data, MeshUsageFlags usage, const char* name)
        {
            if (!data)
                return nullptr;

            MeshDesc desc;
            desc.Data = data;
            desc.Usage = usage;
            desc.SubMeshes.emplace_back(0, data->GetIndexCount(), DrawMode::TRIANGLE_LIST);
            Ref<Mesh> mesh = Mesh::Create(desc);
            mesh->SetName(name);
            return mesh;
        }

        void AppendVertex(Vector<glm::vec3>& positions, Vector<glm::vec3>& normals, Vector<glm::vec3>& tangents,
                          Vector<glm::vec3>& bitangents, Vector<glm::vec2>& uvs, const glm::vec3& position,
                          const glm::vec3& normal, const glm::vec3& tangent, const glm::vec2& uv)
        {
            positions.push_back(position);
            normals.push_back(normal);
            tangents.push_back(tangent);
            bitangents.push_back(glm::normalize(glm::cross(normal, tangent)));
            uvs.push_back(uv);
        }
    } // namespace

    Ref<MeshData> MeshFactory::CreatePlaneData(float width, float height, const glm::vec3& inputNormal,
                                               uint32_t subdivisionsX, uint32_t subdivisionsY)
    {
        glm::vec3 normal;
        if (!IsPositiveFinite(width) || !IsPositiveFinite(height) || !ValidateSegments(subdivisionsX, 1, "Plane") ||
            !ValidateSegments(subdivisionsY, 1, "Plane") || !TryNormalize(inputNormal, normal))
        {
            CW_ENGINE_ERROR("Plane requires positive dimensions, a non-zero normal, and valid subdivisions.");
            return nullptr;
        }

        const glm::vec3 reference = std::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(normal, reference));
        const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
        const size_t vertexCount = static_cast<size_t>(subdivisionsX + 1u) * (subdivisionsY + 1u);
        const size_t indexCount = static_cast<size_t>(subdivisionsX) * subdivisionsY * 6u;
        if (!ValidatePrimitiveCounts(vertexCount, indexCount, "Plane"))
            return nullptr;

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        bitangents.reserve(vertexCount);
        uvs.reserve(vertexCount);
        indices.reserve(indexCount);

        for (uint32_t y = 0; y <= subdivisionsY; y++)
        {
            const float v = static_cast<float>(y) / subdivisionsY;
            for (uint32_t x = 0; x <= subdivisionsX; x++)
            {
                const float u = static_cast<float>(x) / subdivisionsX;
                positions.push_back(tangent * ((u - 0.5f) * width) + bitangent * ((v - 0.5f) * height));
                normals.push_back(normal);
                tangents.push_back(tangent);
                bitangents.push_back(bitangent);
                uvs.emplace_back(u, v);
            }
        }

        const uint32_t rowStride = subdivisionsX + 1u;
        for (uint32_t y = 0; y < subdivisionsY; y++)
        {
            for (uint32_t x = 0; x < subdivisionsX; x++)
            {
                const uint32_t topLeft = y * rowStride + x;
                const uint32_t topRight = topLeft + 1u;
                const uint32_t bottomLeft = topLeft + rowStride;
                const uint32_t bottomRight = bottomLeft + 1u;
                indices.insert(indices.end(), { topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft });
            }
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<MeshData> MeshFactory::CreateBoxData(const glm::vec3& dimensions)
    {
        if (!IsPositiveFinite(dimensions.x) || !IsPositiveFinite(dimensions.y) || !IsPositiveFinite(dimensions.z))
        {
            CW_ENGINE_ERROR("Box requires positive finite dimensions.");
            return nullptr;
        }

        struct Face
        {
            glm::vec3 Normal;
            glm::vec3 Tangent;
            float HalfWidth;
            float HalfHeight;
            float Offset;
        };

        const glm::vec3 half = dimensions * 0.5f;
        const Face faces[] = {
            { { 0, 0, 1 }, { 1, 0, 0 }, half.x, half.y, half.z },   { { 0, 0, -1 }, { -1, 0, 0 }, half.x, half.y, half.z },
            { { 0, 1, 0 }, { 1, 0, 0 }, half.x, half.z, half.y },  { { 0, -1, 0 }, { 1, 0, 0 }, half.x, half.z, half.y },
            { { 1, 0, 0 }, { 0, 0, -1 }, half.z, half.y, half.x }, { { -1, 0, 0 }, { 0, 0, 1 }, half.z, half.y, half.x }
        };

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        positions.reserve(24);
        normals.reserve(24);
        tangents.reserve(24);
        bitangents.reserve(24);
        uvs.reserve(24);
        indices.reserve(36);

        for (const Face& face : faces)
        {
            const glm::vec3 bitangent = glm::cross(face.Normal, face.Tangent);
            const glm::vec3 center = face.Normal * face.Offset;
            const glm::vec3 corners[] = { center - face.Tangent * face.HalfWidth - bitangent * face.HalfHeight,
                                          center + face.Tangent * face.HalfWidth - bitangent * face.HalfHeight,
                                          center + face.Tangent * face.HalfWidth + bitangent * face.HalfHeight,
                                          center - face.Tangent * face.HalfWidth + bitangent * face.HalfHeight };
            const glm::vec2 faceUvs[] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
            const uint32_t first = static_cast<uint32_t>(positions.size());
            for (uint32_t vertex = 0; vertex < 4; vertex++)
            {
                positions.push_back(corners[vertex]);
                normals.push_back(face.Normal);
                tangents.push_back(face.Tangent);
                bitangents.push_back(bitangent);
                uvs.push_back(faceUvs[vertex]);
            }
            indices.insert(indices.end(), { first, first + 1u, first + 2u, first, first + 2u, first + 3u });
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<MeshData> MeshFactory::CreateCubeData(float size)
    {
        return CreateBoxData(glm::vec3(size));
    }

    Ref<MeshData> MeshFactory::CreateSphereData(float radius, uint32_t segments, uint32_t rings)
    {
        if (!IsPositiveFinite(radius) || !ValidateSegments(segments, 3, "Sphere") || !ValidateSegments(rings, 2, "Sphere"))
        {
            CW_ENGINE_ERROR("Sphere requires a positive radius and valid tessellation.");
            return nullptr;
        }

        const size_t vertexCount = static_cast<size_t>(segments + 1u) * (rings + 1u);
        const size_t indexCount = static_cast<size_t>(segments) * (rings - 1u) * 6u;
        if (!ValidatePrimitiveCounts(vertexCount, indexCount, "Sphere"))
            return nullptr;

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        bitangents.reserve(vertexCount);
        uvs.reserve(vertexCount);
        indices.reserve(indexCount);

        for (uint32_t y = 0; y <= rings; y++)
        {
            const float v = static_cast<float>(y) / rings;
            const float latitude = v * glm::pi<float>();
            const float latitudeSine = std::sin(latitude);
            const float latitudeCosine = std::cos(latitude);
            for (uint32_t x = 0; x <= segments; x++)
            {
                const float u = static_cast<float>(x) / segments;
                const float longitude = u * glm::two_pi<float>();
                const float longitudeSine = std::sin(longitude);
                const float longitudeCosine = std::cos(longitude);
                glm::vec3 normal;
                if (y == 0u)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else if (y == rings)
                    normal = glm::vec3(0.0f, -1.0f, 0.0f);
                else
                    normal = glm::vec3(longitudeCosine * latitudeSine, latitudeCosine, longitudeSine * latitudeSine);
                const glm::vec3 tangent(-longitudeSine, 0.0f, longitudeCosine);
                AppendVertex(positions, normals, tangents, bitangents, uvs, normal * radius, normal, tangent, { u, v });
            }
        }

        const uint32_t rowStride = segments + 1u;
        for (uint32_t y = 0; y < rings; y++)
        {
            for (uint32_t x = 0; x < segments; x++)
            {
                const uint32_t current = y * rowStride + x;
                const uint32_t next = current + rowStride;
                if (y > 0u)
                    indices.insert(indices.end(), { current, current + 1u, next + 1u });
                if (y + 1u < rings)
                    indices.insert(indices.end(), { current, next + 1u, next });
            }
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<MeshData> MeshFactory::CreateCylinderData(float radius, float height, uint32_t segments, bool capped)
    {
        if (!IsPositiveFinite(radius) || !IsPositiveFinite(height) || !ValidateSegments(segments, 3, "Cylinder"))
        {
            CW_ENGINE_ERROR("Cylinder requires positive dimensions and valid tessellation.");
            return nullptr;
        }

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        const size_t sideVertexCount = static_cast<size_t>(segments + 1u) * 2u;
        const size_t capVertexCount = capped ? static_cast<size_t>(segments + 2u) * 2u : 0u;
        const size_t vertexCount = sideVertexCount + capVertexCount;
        const size_t indexCount = static_cast<size_t>(segments) * (capped ? 12u : 6u);
        if (!ValidatePrimitiveCounts(vertexCount, indexCount, "Cylinder"))
            return nullptr;

        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        bitangents.reserve(vertexCount);
        uvs.reserve(vertexCount);
        indices.reserve(indexCount);
        const float halfHeight = height * 0.5f;

        for (uint32_t i = 0; i <= segments; i++)
        {
            const float u = static_cast<float>(i) / segments;
            const float angle = u * glm::two_pi<float>();
            const float sine = std::sin(angle);
            const float cosine = std::cos(angle);
            const glm::vec3 normal(cosine, 0.0f, sine);
            const glm::vec3 tangent(-sine, 0.0f, cosine);
            AppendVertex(positions, normals, tangents, bitangents, uvs, { radius * cosine, -halfHeight, radius * sine }, normal,
                         tangent, { u, 1.0f });
            AppendVertex(positions, normals, tangents, bitangents, uvs, { radius * cosine, halfHeight, radius * sine }, normal,
                         tangent, { u, 0.0f });
        }
        for (uint32_t i = 0; i < segments; i++)
        {
            const uint32_t bottom = i * 2u;
            const uint32_t top = bottom + 1u;
            indices.insert(indices.end(), { bottom, top, top + 2u, bottom, top + 2u, bottom + 2u });
        }

        if (capped)
        {
            const auto appendCap = [&](float y, const glm::vec3& normal, bool top) {
                const glm::vec3 tangent(1.0f, 0.0f, 0.0f);
                const uint32_t center = static_cast<uint32_t>(positions.size());
                AppendVertex(positions, normals, tangents, bitangents, uvs, { 0.0f, y, 0.0f }, normal, tangent, { 0.5f, 0.5f });
                const uint32_t ringStart = static_cast<uint32_t>(positions.size());
                for (uint32_t i = 0; i <= segments; i++)
                {
                    const float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
                    const float sine = std::sin(angle);
                    const float cosine = std::cos(angle);
                    AppendVertex(positions, normals, tangents, bitangents, uvs, { radius * cosine, y, radius * sine }, normal,
                                 tangent, { cosine * 0.5f + 0.5f, (top ? -sine : sine) * 0.5f + 0.5f });
                }
                for (uint32_t i = 0; i < segments; i++)
                {
                    if (top)
                        indices.insert(indices.end(), { center, ringStart + i + 1u, ringStart + i });
                    else
                        indices.insert(indices.end(), { center, ringStart + i, ringStart + i + 1u });
                }
            };
            appendCap(halfHeight, { 0.0f, 1.0f, 0.0f }, true);
            appendCap(-halfHeight, { 0.0f, -1.0f, 0.0f }, false);
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<MeshData> MeshFactory::CreateConeData(float radius, float height, uint32_t segments, bool capped)
    {
        if (!IsPositiveFinite(radius) || !IsPositiveFinite(height) || !ValidateSegments(segments, 3, "Cone"))
        {
            CW_ENGINE_ERROR("Cone requires positive dimensions and valid tessellation.");
            return nullptr;
        }

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        const size_t sideVertexCount = static_cast<size_t>(segments + 1u) * 2u;
        const size_t vertexCount = sideVertexCount + (capped ? static_cast<size_t>(segments + 2u) : 0u);
        const size_t indexCount = static_cast<size_t>(segments) * (capped ? 6u : 3u);
        if (!ValidatePrimitiveCounts(vertexCount, indexCount, "Cone"))
            return nullptr;

        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        bitangents.reserve(vertexCount);
        uvs.reserve(vertexCount);
        indices.reserve(indexCount);
        const float halfHeight = height * 0.5f;
        const float normalScale = std::max(radius, height);

        for (uint32_t i = 0; i <= segments; i++)
        {
            const float u = static_cast<float>(i) / segments;
            const float angle = u * glm::two_pi<float>();
            const float sine = std::sin(angle);
            const float cosine = std::cos(angle);
            const glm::vec3 normal =
                glm::normalize(glm::vec3((height / normalScale) * cosine, radius / normalScale, (height / normalScale) * sine));
            const glm::vec3 tangent(-sine, 0.0f, cosine);
            AppendVertex(positions, normals, tangents, bitangents, uvs, { radius * cosine, -halfHeight, radius * sine }, normal,
                         tangent, { u, 1.0f });
            AppendVertex(positions, normals, tangents, bitangents, uvs, { 0.0f, halfHeight, 0.0f }, normal, tangent, { u, 0.0f });
        }
        for (uint32_t i = 0; i < segments; i++)
        {
            const uint32_t bottom = i * 2u;
            indices.insert(indices.end(), { bottom, bottom + 1u, bottom + 2u });
        }

        if (capped)
        {
            const glm::vec3 normal(0.0f, -1.0f, 0.0f);
            const glm::vec3 tangent(1.0f, 0.0f, 0.0f);
            const uint32_t center = static_cast<uint32_t>(positions.size());
            AppendVertex(positions, normals, tangents, bitangents, uvs, { 0.0f, -halfHeight, 0.0f }, normal, tangent,
                         { 0.5f, 0.5f });
            const uint32_t ringStart = static_cast<uint32_t>(positions.size());
            for (uint32_t i = 0; i <= segments; i++)
            {
                const float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
                const float sine = std::sin(angle);
                const float cosine = std::cos(angle);
                AppendVertex(positions, normals, tangents, bitangents, uvs,
                             { radius * cosine, -halfHeight, radius * sine }, normal, tangent,
                             { cosine * 0.5f + 0.5f, sine * 0.5f + 0.5f });
            }
            for (uint32_t i = 0; i < segments; i++)
                indices.insert(indices.end(), { center, ringStart + i, ringStart + i + 1u });
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<MeshData> MeshFactory::CreateCapsuleData(float radius, float height, uint32_t segments, uint32_t hemisphereRings)
    {
        if (!IsPositiveFinite(radius) || !IsPositiveFinite(height) || radius > height * 0.5f ||
            !ValidateSegments(segments, 3, "Capsule") || !ValidateSegments(hemisphereRings, 2, "Capsule"))
        {
            CW_ENGINE_ERROR("Capsule requires positive dimensions, height of at least two radii, and valid tessellation.");
            return nullptr;
        }

        if (std::abs(height - radius * 2.0f) <= std::numeric_limits<float>::epsilon() * height)
            return CreateSphereData(radius, segments, hemisphereRings * 2u);

        const uint32_t rowCount = (hemisphereRings + 1u) * 2u;
        const size_t vertexCount = static_cast<size_t>(rowCount) * (segments + 1u);
        const size_t indexCount = static_cast<size_t>(rowCount - 2u) * segments * 6u;
        if (!ValidatePrimitiveCounts(vertexCount, indexCount, "Capsule"))
            return nullptr;

        Vector<glm::vec3> positions;
        Vector<glm::vec3> normals;
        Vector<glm::vec3> tangents;
        Vector<glm::vec3> bitangents;
        Vector<glm::vec2> uvs;
        Vector<uint32_t> indices;
        positions.reserve(vertexCount);
        normals.reserve(vertexCount);
        tangents.reserve(vertexCount);
        bitangents.reserve(vertexCount);
        uvs.reserve(vertexCount);
        indices.reserve(indexCount);
        const float cylinderHalfHeight = (height - radius * 2.0f) * 0.5f;

        const auto appendRow = [&](float latitude, float centerY, uint32_t row) {
            const float latitudeSine = std::sin(latitude);
            const float latitudeCosine = std::cos(latitude);
            for (uint32_t x = 0; x <= segments; x++)
            {
                const float u = static_cast<float>(x) / segments;
                const float longitude = u * glm::two_pi<float>();
                const float longitudeSine = std::sin(longitude);
                const float longitudeCosine = std::cos(longitude);
                glm::vec3 normal;
                if (row == 0u)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else if (row + 1u == rowCount)
                    normal = glm::vec3(0.0f, -1.0f, 0.0f);
                else
                    normal = glm::vec3(latitudeCosine * longitudeCosine, latitudeSine, latitudeCosine * longitudeSine);
                const glm::vec3 tangent(-longitudeSine, 0.0f, longitudeCosine);
                const glm::vec3 position(radius * normal.x, centerY + radius * normal.y, radius * normal.z);
                const float v = static_cast<float>(row) / (rowCount - 1u);
                AppendVertex(positions, normals, tangents, bitangents, uvs, position, normal, tangent, { u, v });
            }
        };

        uint32_t row = 0;
        for (uint32_t ring = 0; ring <= hemisphereRings; ring++, row++)
        {
            const float t = static_cast<float>(ring) / hemisphereRings;
            appendRow(glm::half_pi<float>() * (1.0f - t), cylinderHalfHeight, row);
        }
        for (uint32_t ring = 0; ring <= hemisphereRings; ring++, row++)
        {
            const float t = static_cast<float>(ring) / hemisphereRings;
            appendRow(-glm::half_pi<float>() * t, -cylinderHalfHeight, row);
        }

        const uint32_t rowStride = segments + 1u;
        for (uint32_t y = 0; y + 1u < rowCount; y++)
        {
            for (uint32_t x = 0; x < segments; x++)
            {
                const uint32_t current = y * rowStride + x;
                const uint32_t next = current + rowStride;
                if (y > 0u)
                    indices.insert(indices.end(), { current, current + 1u, next + 1u });
                if (y + 2u < rowCount)
                    indices.insert(indices.end(), { current, next + 1u, next });
            }
        }

        return BuildMeshData(positions, normals, tangents, bitangents, uvs, indices);
    }

    Ref<Mesh> MeshFactory::CreatePlane(float width, float height, const glm::vec3& normal, uint32_t subdivisionsX,
                                       uint32_t subdivisionsY, MeshUsageFlags usage)
    {
        return BuildMesh(CreatePlaneData(width, height, normal, subdivisionsX, subdivisionsY), usage, "Plane");
    }

    Ref<Mesh> MeshFactory::CreateBox(const glm::vec3& dimensions, MeshUsageFlags usage)
    {
        return BuildMesh(CreateBoxData(dimensions), usage, "Box");
    }

    Ref<Mesh> MeshFactory::CreateCube(float size, MeshUsageFlags usage)
    {
        return BuildMesh(CreateCubeData(size), usage, "Cube");
    }

    Ref<Mesh> MeshFactory::CreateSphere(float radius, uint32_t segments, uint32_t rings, MeshUsageFlags usage)
    {
        return BuildMesh(CreateSphereData(radius, segments, rings), usage, "Sphere");
    }

    Ref<Mesh> MeshFactory::CreateCylinder(float radius, float height, uint32_t segments, bool capped, MeshUsageFlags usage)
    {
        return BuildMesh(CreateCylinderData(radius, height, segments, capped), usage, "Cylinder");
    }

    Ref<Mesh> MeshFactory::CreateCone(float radius, float height, uint32_t segments, bool capped, MeshUsageFlags usage)
    {
        return BuildMesh(CreateConeData(radius, height, segments, capped), usage, "Cone");
    }

    Ref<Mesh> MeshFactory::CreateCapsule(float radius, float height, uint32_t segments, uint32_t hemisphereRings,
                                         MeshUsageFlags usage)
    {
        return BuildMesh(CreateCapsuleData(radius, height, segments, hemisphereRings), usage, "Capsule");
    }
} // namespace Crowny
