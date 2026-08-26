#include <catch2/catch_test_macros.hpp>

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
}

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
