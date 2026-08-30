#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/MeshFactory.h"

#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace Crowny;

namespace
{
    constexpr float EPSILON = 1e-4f;

    bool IsFinite(const glm::vec2& value) { return std::isfinite(value.x) && std::isfinite(value.y); }

    bool IsFinite(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    void CheckPrimitiveData(const Ref<MeshData>& data)
    {
        REQUIRE(data != nullptr);
        const BufferLayout& layout = data->GetBufferLayout();
        CHECK(layout.HasAttribute(VertexAttribute::Position));
        CHECK(layout.HasAttribute(VertexAttribute::Normal));
        CHECK(layout.HasAttribute(VertexAttribute::Tangent));
        CHECK(layout.HasAttribute(VertexAttribute::Bitangent));
        CHECK(layout.HasAttribute(VertexAttribute::TexCoord0));

        const Vector<glm::vec3> positions = data->GetPositions();
        const Vector<glm::vec3> normals = data->GetNormals();
        const Vector<glm::vec3> tangents = data->GetTangents();
        const Vector<glm::vec3> bitangents = data->GetBitangents();
        const Vector<glm::vec2> uvs = data->GetUVs();
        const Vector<uint32_t> indices = data->GetIndices();
        REQUIRE(positions.size() == data->GetVertexCount());
        REQUIRE(normals.size() == positions.size());
        REQUIRE(tangents.size() == positions.size());
        REQUIRE(bitangents.size() == positions.size());
        REQUIRE(uvs.size() == positions.size());
        REQUIRE(indices.size() == data->GetIndexCount());
        REQUIRE(indices.size() % 3u == 0u);

        for (size_t i = 0; i < positions.size(); i++)
        {
            CHECK(IsFinite(positions[i]));
            CHECK(IsFinite(normals[i]));
            CHECK(IsFinite(tangents[i]));
            CHECK(IsFinite(bitangents[i]));
            CHECK(IsFinite(uvs[i]));
            CHECK(std::abs(glm::length2(normals[i]) - 1.0f) < EPSILON);
            CHECK(std::abs(glm::length2(tangents[i]) - 1.0f) < EPSILON);
            CHECK(std::abs(glm::length2(bitangents[i]) - 1.0f) < EPSILON);
            CHECK(std::abs(glm::dot(normals[i], tangents[i])) < EPSILON);
            CHECK(std::abs(glm::dot(normals[i], bitangents[i])) < EPSILON);
            CHECK(std::abs(glm::dot(tangents[i], bitangents[i])) < EPSILON);
            CHECK(glm::dot(glm::cross(normals[i], tangents[i]), bitangents[i]) > 1.0f - EPSILON);
            CHECK(uvs[i].x >= -EPSILON);
            CHECK(uvs[i].x <= 1.0f + EPSILON);
            CHECK(uvs[i].y >= -EPSILON);
            CHECK(uvs[i].y <= 1.0f + EPSILON);
        }

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            REQUIRE(indices[i] < positions.size());
            REQUIRE(indices[i + 1] < positions.size());
            REQUIRE(indices[i + 2] < positions.size());
            const glm::vec3 edgeA = positions[indices[i + 1]] - positions[indices[i]];
            const glm::vec3 edgeB = positions[indices[i + 2]] - positions[indices[i]];
            const glm::vec3 faceNormal = glm::cross(edgeA, edgeB);
            REQUIRE(glm::length2(faceNormal) > EPSILON * EPSILON);
            const glm::vec3 averageNormal = normals[indices[i]] + normals[indices[i + 1]] + normals[indices[i + 2]];
            CHECK(glm::dot(faceNormal, averageNormal) > 0.0f);
        }
    }

    void CheckWrappedRows(const Ref<MeshData>& data, uint32_t segments, uint32_t rowCount)
    {
        const Vector<glm::vec3> positions = data->GetPositions();
        const Vector<glm::vec3> normals = data->GetNormals();
        const Vector<glm::vec3> tangents = data->GetTangents();
        const Vector<glm::vec2> uvs = data->GetUVs();
        const uint32_t rowStride = segments + 1u;
        REQUIRE(positions.size() == static_cast<size_t>(rowStride) * rowCount);

        for (uint32_t row = 0; row < rowCount; ++row)
        {
            const uint32_t first = row * rowStride;
            const uint32_t last = first + segments;
            CHECK(glm::length2(positions[first] - positions[last]) < EPSILON * EPSILON);
            CHECK(glm::length2(normals[first] - normals[last]) < EPSILON * EPSILON);
            CHECK(glm::length2(tangents[first] - tangents[last]) < EPSILON * EPSILON);
            CHECK(std::abs(uvs[first].x) < EPSILON);
            CHECK(std::abs(uvs[last].x - 1.0f) < EPSILON);
            CHECK(std::abs(uvs[first].y - uvs[last].y) < EPSILON);
        }
    }

    void CheckWrappedColumns(const Ref<MeshData>& data, uint32_t segments, uint32_t verticesPerColumn)
    {
        const Vector<glm::vec3> positions = data->GetPositions();
        const Vector<glm::vec3> normals = data->GetNormals();
        const Vector<glm::vec3> tangents = data->GetTangents();
        const Vector<glm::vec2> uvs = data->GetUVs();
        const uint32_t lastColumn = segments * verticesPerColumn;
        REQUIRE(positions.size() >= static_cast<size_t>(lastColumn + verticesPerColumn));

        for (uint32_t vertex = 0; vertex < verticesPerColumn; ++vertex)
        {
            CHECK(glm::length2(positions[vertex] - positions[lastColumn + vertex]) < EPSILON * EPSILON);
            CHECK(glm::length2(normals[vertex] - normals[lastColumn + vertex]) < EPSILON * EPSILON);
            CHECK(glm::length2(tangents[vertex] - tangents[lastColumn + vertex]) < EPSILON * EPSILON);
            CHECK(std::abs(uvs[vertex].x) < EPSILON);
            CHECK(std::abs(uvs[lastColumn + vertex].x - 1.0f) < EPSILON);
            CHECK(std::abs(uvs[vertex].y - uvs[lastColumn + vertex].y) < EPSILON);
        }
    }

    void CheckBounds(const Ref<MeshData>& data, const glm::vec3& expectedMin, const glm::vec3& expectedMax)
    {
        AABox bounds;
        SphereBounds sphere;
        data->CalculateBounds(bounds, sphere);
        CHECK(glm::length2(bounds.GetMin() - expectedMin) < EPSILON * EPSILON);
        CHECK(glm::length2(bounds.GetMax() - expectedMax) < EPSILON * EPSILON);
    }
} // namespace

TEST_CASE("MeshFactory creates render-ready primitive data", "[Renderer][MeshFactory]")
{
    SECTION("Subdivided plane with an arbitrary normal")
    {
        const Ref<MeshData> data = MeshFactory::CreatePlaneData(2.0f, 4.0f, glm::vec3(0.0f, 0.0f, 1.0f), 2, 3);
        CheckPrimitiveData(data);
        CHECK(data->GetVertexCount() == 12);
        CHECK(data->GetIndexCount() == 36);
        for (const glm::vec3& normal : data->GetNormals())
            CHECK(glm::length2(normal - glm::vec3(0.0f, 0.0f, 1.0f)) < EPSILON * EPSILON);
    }

    SECTION("Single quad")
    {
        const Ref<MeshData> data = MeshFactory::CreateQuadData(2.0f, 4.0f);
        CheckPrimitiveData(data);
        CHECK(data->GetVertexCount() == 4);
        CHECK(data->GetIndexCount() == 6);
        const Vector<uint32_t> expectedIndices{ 0, 1, 3, 0, 3, 2 };
        CHECK(data->GetIndices() == expectedIndices);
        CheckBounds(data, { -1.0f, 0.0f, -2.0f }, { 1.0f, 0.0f, 2.0f });
    }

    SECTION("Box and cube")
    {
        const Ref<MeshData> box = MeshFactory::CreateBoxData({ 2.0f, 4.0f, 6.0f });
        CheckPrimitiveData(box);
        CHECK(box->GetVertexCount() == 24);
        CHECK(box->GetIndexCount() == 36);
        CHECK(box->GetIndexType() == IndexType::Index_16);
        CheckBounds(box, { -1.0f, -2.0f, -3.0f }, { 1.0f, 2.0f, 3.0f });

        const Ref<MeshData> cube = MeshFactory::CreateCubeData(2.0f);
        CheckBounds(cube, glm::vec3(-1.0f), glm::vec3(1.0f));
    }

    SECTION("UV sphere")
    {
        const Ref<MeshData> data = MeshFactory::CreateSphereData(2.0f, 8, 4);
        CheckPrimitiveData(data);
        CHECK(data->GetVertexCount() == 45);
        CHECK(data->GetIndexCount() == 144);
        CheckWrappedRows(data, 8, 5);
        for (const glm::vec3& position : data->GetPositions())
            CHECK(std::abs(glm::length(position) - 2.0f) < EPSILON);
        CheckBounds(data, glm::vec3(-2.0f), glm::vec3(2.0f));
    }

    SECTION("Capped and open cylinders")
    {
        const Ref<MeshData> capped = MeshFactory::CreateCylinderData(1.0f, 2.0f, 8, true);
        CheckPrimitiveData(capped);
        CHECK(capped->GetVertexCount() == 38);
        CHECK(capped->GetIndexCount() == 96);
        CheckWrappedColumns(capped, 8, 2);
        CheckBounds(capped, glm::vec3(-1.0f), glm::vec3(1.0f));

        const Ref<MeshData> open = MeshFactory::CreateCylinderData(1.0f, 2.0f, 8, false);
        CheckPrimitiveData(open);
        CHECK(open->GetVertexCount() == 18);
        CHECK(open->GetIndexCount() == 48);
        CheckWrappedColumns(open, 8, 2);
    }

    SECTION("Cone")
    {
        const Ref<MeshData> data = MeshFactory::CreateConeData(1.0f, 2.0f, 8, true);
        CheckPrimitiveData(data);
        CHECK(data->GetVertexCount() == 28);
        CHECK(data->GetIndexCount() == 48);
        CheckWrappedColumns(data, 8, 2);
        CheckBounds(data, glm::vec3(-1.0f), glm::vec3(1.0f));
    }

    SECTION("Capsule")
    {
        const Ref<MeshData> data = MeshFactory::CreateCapsuleData(0.5f, 2.0f, 8, 2);
        CheckPrimitiveData(data);
        CHECK(data->GetVertexCount() == 54);
        CHECK(data->GetIndexCount() == 192);
        CheckWrappedRows(data, 8, 6);
        CheckBounds(data, { -0.5f, -1.0f, -0.5f }, { 0.5f, 1.0f, 0.5f });

        const Ref<MeshData> sphereCapsule = MeshFactory::CreateCapsuleData(0.5f, 1.0f, 8, 2);
        REQUIRE(sphereCapsule != nullptr);
        CHECK(sphereCapsule->GetVertexCount() == 45);
        CHECK(sphereCapsule->GetIndexCount() == 144);
    }
}

TEST_CASE("MeshFactory rejects invalid dimensions", "[Renderer][MeshFactory]")
{
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(MeshFactory::CreateCubeData(0.0f) == nullptr);
    CHECK(MeshFactory::CreateQuadData(-1.0f, 1.0f) == nullptr);
    CHECK(MeshFactory::CreateBoxData({ 1.0f, nan, 1.0f }) == nullptr);
    CHECK(MeshFactory::CreateSphereData(-1.0f) == nullptr);
    CHECK(MeshFactory::CreateSphereData(1.0f, 2, 8) == nullptr);
    CHECK(MeshFactory::CreateCylinderData(1.0f, 0.0f) == nullptr);
    CHECK(MeshFactory::CreateConeData(1.0f, 1.0f, 2) == nullptr);
    CHECK(MeshFactory::CreateCapsuleData(1.0f, 1.0f) == nullptr);
    CHECK(MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f)) == nullptr);
    CHECK(MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(infinity, 0.0f, 0.0f)) == nullptr);
}

TEST_CASE("MeshFactory bounds tessellation allocations and selects index width", "[Renderer][MeshFactory]")
{
    const Ref<MeshData> widePlane = MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 256, 256);
    REQUIRE(widePlane != nullptr);
    CHECK(widePlane->GetIndexType() == IndexType::Index_32);
    const Vector<uint32_t> indices = widePlane->GetIndices();
    REQUIRE(!indices.empty());
    CHECK(*std::max_element(indices.begin(), indices.end()) > std::numeric_limits<uint16_t>::max());

    const Ref<MeshData> maximumSingleAxis = MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 4096, 1);
    REQUIRE(maximumSingleAxis != nullptr);
    CHECK(maximumSingleAxis->GetVertexCount() == 8194);
    CHECK(maximumSingleAxis->GetIndexCount() == 24576);

    CHECK(MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 4096, 4096) == nullptr);
    CHECK(MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 4097, 1) == nullptr);
    CHECK(MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), std::numeric_limits<uint32_t>::max(), 1) == nullptr);
    CHECK(MeshFactory::CreateSphereData(1.0f, 4096, 4096) == nullptr);
    CHECK(MeshFactory::CreateCapsuleData(0.5f, 2.0f, 4096, 4096) == nullptr);
}

TEST_CASE("MeshFactory safely normalizes large finite inputs", "[Renderer][MeshFactory]")
{
    const float maximum = std::numeric_limits<float>::max();
    const Ref<MeshData> plane = MeshFactory::CreatePlaneData(1.0f, 1.0f, glm::vec3(maximum, 0.0f, 0.0f));
    CheckPrimitiveData(plane);
    for (const glm::vec3& normal : plane->GetNormals())
        CHECK(glm::length2(normal - glm::vec3(1.0f, 0.0f, 0.0f)) < EPSILON * EPSILON);

    const Ref<MeshData> cone = MeshFactory::CreateConeData(1.0f, maximum, 8, false);
    CheckPrimitiveData(cone);
}

TEST_CASE("MeshData reallocation clears and replaces its owned buffer", "[Renderer][Mesh]")
{
    const BufferLayout layout = { { ShaderDataType::Float3, VertexAttribute::Position } };
    const Ref<MeshData> data = MeshData::Create(4, 6, layout);
    std::memset(data->GetIndexData(), 0xff, data->GetIndexBufferSize() + data->GetVertexBufferSize());
    data->AllocateBuffer();

    const uint8_t* bytes = data->GetIndexData();
    for (uint32_t i = 0; i < data->GetIndexBufferSize() + data->GetVertexBufferSize(); i++)
        CHECK(bytes[i] == 0u);
}
