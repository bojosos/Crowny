#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/MeshProcessing.h"

#include <limits>

using namespace Crowny;

namespace
{
    Ref<MeshData> CreateGrid(uint32_t cells)
    {
        const uint32_t side = cells + 1u;
        Vector<glm::vec3> positions;
        positions.reserve(side * side);
        for (uint32_t y = 0; y < side; y++)
        {
            for (uint32_t x = 0; x < side; x++)
                positions.emplace_back(static_cast<float>(x), 0.0f, static_cast<float>(y));
        }

        Vector<uint32_t> indices;
        indices.reserve(cells * cells * 6u);
        for (uint32_t y = 0; y < cells; y++)
        {
            for (uint32_t x = 0; x < cells; x++)
            {
                const uint32_t first = y * side + x;
                indices.insert(indices.end(), { first, first + side, first + 1u, first + 1u, first + side, first + side + 1u });
            }
        }

        BufferLayout layout{ { ShaderDataType::Float3, VertexAttribute::Position } };
        Ref<MeshData> data = MeshData::Create(static_cast<uint32_t>(positions.size()), static_cast<uint32_t>(indices.size()), layout);
        data->SetPositions(positions);
        data->SetIndices(indices);
        return data;
    }
} // namespace

TEST_CASE("Mesh processing creates bounded meshlets and conventional LODs", "[Renderer][MeshProcessing]")
{
    const Ref<MeshData> grid = CreateGrid(20);
    const Vector<SubMesh> subMeshes{ SubMesh(0, grid->GetIndexCount(), DrawMode::TRIANGLE_LIST) };
    const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(*grid, subMeshes);

    REQUIRE_FALSE(geometry.Lods.empty());
    REQUIRE_FALSE(geometry.Meshlets.empty());
    CHECK(geometry.Lods.front().SubMeshCount == 1);
    CHECK((geometry.Lods.front().MeshletCount == geometry.Meshlets.size() || geometry.Lods.size() > 1));
    for (const Meshlet& meshlet : geometry.Meshlets)
    {
        CHECK(meshlet.VertexCount <= 64);
        CHECK(meshlet.TriangleCount <= 64);
        CHECK(meshlet.VertexCount > 0);
        CHECK(meshlet.TriangleCount > 0);
        CHECK(meshlet.VertexOffset + meshlet.VertexCount <= geometry.MeshletVertices.size());
        CHECK(meshlet.TriangleOffset + meshlet.TriangleCount * 3u <= geometry.MeshletTriangles.size());
        CHECK(meshlet.TriangleOffset + meshlet.TriangleCount * 3u <= geometry.MeshletIndices.size());
        CHECK(meshlet.BoundingSphere.w >= 0.0f);
    }

    for (uint32_t lod = 1; lod < geometry.Lods.size(); lod++)
    {
        const MeshLodSubMesh& previous = geometry.LodSubMeshes[geometry.Lods[lod - 1u].FirstSubMesh];
        const MeshLodSubMesh& current = geometry.LodSubMeshes[geometry.Lods[lod].FirstSubMesh];
        CHECK(current.IndexCount < previous.IndexCount);
    }
}

TEST_CASE("Mesh processing output is deterministic", "[Renderer][MeshProcessing]")
{
    const Ref<MeshData> grid = CreateGrid(8);
    const Vector<SubMesh> subMeshes{ SubMesh(0, grid->GetIndexCount(), DrawMode::TRIANGLE_LIST) };
    const MeshGpuGeometry first = MeshProcessing::BuildGpuGeometry(*grid, subMeshes);
    const MeshGpuGeometry second = MeshProcessing::BuildGpuGeometry(*grid, subMeshes);

    CHECK(first.LodIndices == second.LodIndices);
    CHECK(first.MeshletVertices == second.MeshletVertices);
    CHECK(first.MeshletTriangles == second.MeshletTriangles);
    CHECK(first.MeshletIndices == second.MeshletIndices);
    REQUIRE(first.Meshlets.size() == second.Meshlets.size());
    for (size_t index = 0; index < first.Meshlets.size(); index++)
    {
        CHECK(first.Meshlets[index].VertexOffset == second.Meshlets[index].VertexOffset);
        CHECK(first.Meshlets[index].TriangleOffset == second.Meshlets[index].TriangleOffset);
        CHECK(first.Meshlets[index].BoundingSphere == second.Meshlets[index].BoundingSphere);
        CHECK(first.Meshlets[index].NormalCone == second.Meshlets[index].NormalCone);
    }
}

TEST_CASE("Mesh processing allocation grows with the total geometry across material groups", "[Renderer][MeshProcessing]")
{
    const auto measure = [](uint32_t subMeshCount) {
        Vector<glm::vec3> positions;
        Vector<uint32_t> indices;
        Vector<SubMesh> subMeshes;
        for (uint32_t group = 0; group < subMeshCount; group++)
        {
            const float x = static_cast<float>(group) * 2.0f;
            positions.insert(positions.end(), { { x, 0, 0 }, { x + 1, 0, 0 }, { x, 0, 1 }, { x + 1, 0, 1 } });
            const uint32_t first = group * 4u;
            subMeshes.emplace_back(static_cast<uint32_t>(indices.size()), 6, DrawMode::TRIANGLE_LIST);
            indices.insert(indices.end(), { first, first + 2u, first + 1u, first + 1u, first + 2u, first + 3u });
        }
        const BufferLayout layout{ { ShaderDataType::Float3, VertexAttribute::Position } };
        Ref<MeshData> data = MeshData::Create(static_cast<uint32_t>(positions.size()), static_cast<uint32_t>(indices.size()), layout);
        data->SetPositions(positions);
        data->SetIndices(indices);
        MeshProcessingSettings settings;
        settings.LodCount = 1;
        const auto before = Memory::GetThreadAllocationSnapshot();
        const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(*data, subMeshes, settings);
        const auto allocations = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());
        REQUIRE(geometry.LodSubMeshes.size() == subMeshCount);
        REQUIRE(geometry.Meshlets.size() == subMeshCount);
        return allocations.RequestedBytes;
    };
    const uint64_t small = measure(32);
    const uint64_t large = measure(128);
    INFO("32 groups allocated " << small << " bytes; 128 groups allocated " << large << " bytes");
    // Four times the independent geometry must not cause quadratic scratch/output allocation.
    CHECK(large < small * 6);
}

TEST_CASE("Mesh processing preserves scattered submesh vertices and world-space LOD errors", "[Renderer][MeshProcessing]")
{
    const Ref<MeshData> grid = CreateGrid(8);
    Vector<glm::vec3> gridPositions = grid->GetPositions();
    for (glm::vec3& position : gridPositions)
        position.y = std::sin(position.x) * std::cos(position.z);
    grid->SetPositions(gridPositions);

    // Interleave two distant material groups so neither occupies a contiguous vertex range.
    Vector<glm::vec3> positions;
    for (const glm::vec3& position : gridPositions)
    {
        positions.push_back(position + glm::vec3(1024.0f, 0.0f, 0.0f));
        positions.push_back(position);
    }
    Vector<uint32_t> indices;
    for (uint32_t materialSlot = 0; materialSlot < 2; materialSlot++)
    {
        for (uint32_t index : grid->GetIndices())
            indices.push_back(index * 2u + materialSlot);
    }
    Ref<MeshData> combined =
      MeshData::Create(static_cast<uint32_t>(positions.size()), static_cast<uint32_t>(indices.size()), grid->GetBufferLayout());
    combined->SetPositions(positions);
    combined->SetIndices(indices);
    MeshProcessingSettings settings;
    settings.LodTargetError = 0.2f;
    const MeshGpuGeometry isolated = MeshProcessing::BuildGpuGeometry(*grid, {}, settings);
    const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(
      *combined,
      { SubMesh(0, grid->GetIndexCount(), DrawMode::TRIANGLE_LIST), SubMesh(grid->GetIndexCount(), grid->GetIndexCount(), DrawMode::TRIANGLE_LIST) },
      settings);

    REQUIRE(isolated.Lods.size() > 1);
    REQUIRE(geometry.Lods.size() == isolated.Lods.size());
    CHECK(isolated.Lods[1].Error > 0.0f);
    for (size_t lodIndex = 0; lodIndex < geometry.Lods.size(); lodIndex++)
    {
        const MeshLod& lod = geometry.Lods[lodIndex];
        CHECK(lod.Error == Catch::Approx(isolated.Lods[lodIndex].Error).margin(0.0001f));
        REQUIRE(lod.SubMeshCount == 2);
        REQUIRE(lod.MeshletCount > 0);
        for (uint32_t offset = 0; offset < lod.SubMeshCount; offset++)
        {
            const MeshLodSubMesh& subMesh = geometry.LodSubMeshes[lod.FirstSubMesh + offset];
            REQUIRE(subMesh.IndexCount > 0);
            for (uint32_t index = 0; index < subMesh.IndexCount; index++)
            {
                const uint32_t vertex = geometry.LodIndices[subMesh.IndexOffset + index];
                REQUIRE(vertex < positions.size());
                CHECK(vertex % 2u == subMesh.MaterialSlot);
            }
        }
        for (uint32_t offset = 0; offset < lod.MeshletCount; offset++)
        {
            const Meshlet& meshlet = geometry.Meshlets[lod.FirstMeshlet + offset];
            CHECK(meshlet.LodError == Catch::Approx(isolated.Lods[lodIndex].Error).margin(0.0001f));
            for (uint32_t index = 0; index < meshlet.VertexCount; index++)
            {
                const uint32_t vertex = geometry.MeshletVertices[meshlet.VertexOffset + index];
                REQUIRE(vertex < positions.size());
                CHECK(vertex % 2u == meshlet.MaterialSlot);
                CHECK(glm::distance(positions[vertex], glm::vec3(meshlet.BoundingSphere)) <= meshlet.BoundingSphere.w + 0.001f);
            }
            for (uint32_t index = 0; index < meshlet.TriangleCount * 3u; index++)
            {
                const uint32_t localVertex = geometry.MeshletTriangles[meshlet.TriangleOffset + index];
                REQUIRE(localVertex < meshlet.VertexCount);
                CHECK(geometry.MeshletIndices[meshlet.TriangleOffset + index] == geometry.MeshletVertices[meshlet.VertexOffset + localVertex]);
            }
        }
    }
}

TEST_CASE("Mesh processing rejects malformed geometry without reading out of bounds", "[Renderer][MeshProcessing]")
{
    MeshProcessingSettings settings;
    settings.LodCount = 1;

    SECTION("overflowing submesh ranges are skipped while valid material slots survive")
    {
        const Ref<MeshData> grid = CreateGrid(1);
        const Vector<SubMesh> subMeshes{ SubMesh(std::numeric_limits<uint32_t>::max() - 2u, 6, DrawMode::TRIANGLE_LIST),
                                         SubMesh(0, grid->GetIndexCount(), DrawMode::TRIANGLE_LIST) };
        const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(*grid, subMeshes, settings);

        REQUIRE(geometry.Lods.size() == 1);
        REQUIRE(geometry.LodSubMeshes.size() == 1);
        CHECK(geometry.LodSubMeshes[0].MaterialSlot == 1);
        REQUIRE_FALSE(geometry.Meshlets.empty());
        for (const Meshlet& meshlet : geometry.Meshlets)
            CHECK(meshlet.MaterialSlot == 1);
    }

    SECTION("out-of-range vertex indices reject the affected submesh")
    {
        const Ref<MeshData> grid = CreateGrid(1);
        Vector<uint32_t> indices = grid->GetIndices();
        indices[2] = grid->GetVertexCount();
        grid->SetIndices(indices);

        const MeshGpuGeometry geometry =
          MeshProcessing::BuildGpuGeometry(*grid, { SubMesh(0, grid->GetIndexCount(), DrawMode::TRIANGLE_LIST) }, settings);
        CHECK(geometry.IsEmpty());
        CHECK(geometry.LodIndices.empty());
        CHECK(geometry.Meshlets.empty());
    }

    SECTION("partial triangles are rejected")
    {
        const Ref<MeshData> grid = CreateGrid(1);
        const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(*grid, { SubMesh(0, 4, DrawMode::TRIANGLE_LIST) }, settings);
        CHECK(geometry.IsEmpty());
    }

    SECTION("non-finite positions are rejected before meshoptimizer sees them")
    {
        const Ref<MeshData> grid = CreateGrid(1);
        Vector<glm::vec3> positions = grid->GetPositions();
        positions[0].x = std::numeric_limits<float>::quiet_NaN();
        grid->SetPositions(positions);

        const MeshGpuGeometry geometry = MeshProcessing::BuildGpuGeometry(*grid, {}, settings);
        CHECK(geometry.IsEmpty());
    }
}
