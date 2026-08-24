#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "cwpch.h"
#include "Crowny/Renderer/Mesh.h"

using namespace Crowny;

TEST_CASE("MeshData Functionality", "[Renderer][Mesh]")
{
    BufferLayout layout = {
        { ShaderDataType::Float3, VertexAttribute::Position },
        { ShaderDataType::Float3, VertexAttribute::Normal },
        { ShaderDataType::Float4, VertexAttribute::Color },
        { ShaderDataType::Float2, VertexAttribute::TexCoord0 }
    };

    SECTION("Allocation and Basic Properties")
    {
        uint32_t vertexCount = 10;
        uint32_t indexCount = 30;
        Ref<MeshData> data = MeshData::Create(vertexCount, indexCount, layout, IndexType::Index_32);

        CHECK(data->GetVertexCount() == vertexCount);
        CHECK(data->GetIndexCount() == indexCount);
        CHECK(data->GetIndexType() == IndexType::Index_32);
        CHECK(data->GetIndexSize() == 4);
        CHECK(data->GetBufferLayout().GetStride() == layout.GetStride());
        CHECK(data->GetVertexBufferSize() == vertexCount * layout.GetStride());
        CHECK(data->GetIndexBufferSize() == indexCount * 4);
    }

    SECTION("Vertex Data: Positions, Normals, Colors, UVs")
    {
        uint32_t vertexCount = 2;
        Ref<MeshData> data = MeshData::Create(vertexCount, 0, layout);

        Vector<glm::vec3> positions = { { 1, 2, 3 }, { 4, 5, 6 } };
        Vector<glm::vec3> normals = { { 0, 1, 0 }, { 1, 0, 0 } };
        Vector<glm::vec4> colors = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 } };
        Vector<glm::vec2> uvs = { { 0, 0 }, { 1, 1 } };

        data->SetPositions(positions);
        data->SetNormals(normals);
        data->SetColors(colors);
        data->SetUVs(0, uvs);

        CHECK(data->GetPositions() == positions);
        CHECK(data->GetNormals() == normals);
        CHECK(data->GetColors() == colors);
        CHECK(data->GetUVs(0) == uvs);
    }

    SECTION("Vertex Data: Tangents")
    {
        BufferLayout tangentLayout = {
            { ShaderDataType::Float3, VertexAttribute::Position },
            { ShaderDataType::Float3, VertexAttribute::Tangent }
        };
        Ref<MeshData> data = MeshData::Create(1, 0, tangentLayout);
        Vector<glm::vec3> tangents = { { 1, 0, 0 } };
        data->SetTangents(tangents);
        CHECK(data->GetTangents() == tangents);
    }

    SECTION("Index Data (16-bit and 32-bit)")
    {
        SECTION("32-bit")
        {
            Ref<MeshData> data = MeshData::Create(4, 3, layout, IndexType::Index_32);
            Vector<uint32_t> indices = { 0, 1, 2 };
            data->SetIndices(indices);
            CHECK(data->GetIndices() == indices);
            CHECK(data->GetIndexData<uint32_t>()[0] == 0);
        }

        SECTION("16-bit Conversion")
        {
            Ref<MeshData> data = MeshData::Create(4, 3, layout, IndexType::Index_16);
            Vector<uint32_t> indices = { 0, 1, 2 };
            data->SetIndices(indices); // Converts to uint16 internally
            
            CHECK(data->GetIndices() == indices); // Converts back to uint32
            CHECK(data->GetIndexData<uint16_t>()[0] == 0);
            CHECK(data->GetIndexData<uint16_t>()[2] == 2);
        }
    }

    SECTION("Bounds Calculation")
    {
        SECTION("Normal Mesh")
        {
            Ref<MeshData> data = MeshData::Create(3, 0, layout);
            data->SetPositions({ 
                { -1.0f, 0.0f, 0.0f }, 
                { 1.0f, 0.0f, 0.0f }, 
                { 0.0f, 2.0f, 0.0f } 
            });

            AABox aabb;
            SphereBounds sphere;
            data->CalculateBounds(aabb, sphere);

            CHECK(aabb.GetMin() == glm::vec3(-1.0f, 0.0f, 0.0f));
            CHECK(aabb.GetMax() == glm::vec3(1.0f, 2.0f, 0.0f));
            
            // Center should be avg of points: (-1+1+0)/3, (0+0+2)/3, (0+0+0)/3 = (0, 0.666, 0)
            CHECK_THAT(sphere.GetCenter().y, Catch::Matchers::WithinRel(0.6666f, 0.001f));
        }

        SECTION("Single Vertex")
        {
            Ref<MeshData> data = MeshData::Create(1, 0, layout);
            data->SetPositions({ { 10, 10, 10 } });
            AABox aabb;
            SphereBounds sphere;
            data->CalculateBounds(aabb, sphere);
            CHECK(aabb.GetMin() == glm::vec3(10, 10, 10));
            CHECK(aabb.GetMax() == glm::vec3(10, 10, 10));
            CHECK(sphere.GetCenter() == glm::vec3(10, 10, 10));
            CHECK(sphere.GetRadius() == 0.0f);
        }

        SECTION("Sphere radius encloses all vertices")
        {
            Ref<MeshData> data = MeshData::Create(3, 0, layout);
            data->SetPositions({
                { 0.0f, 0.0f, 0.0f },
                { 3.0f, 0.0f, 0.0f },
                { 0.0f, 4.0f, 0.0f }
            });
            AABox aabb;
            SphereBounds sphere;
            data->CalculateBounds(aabb, sphere);
            // Every vertex must be within the sphere radius
            for (const auto& p : { glm::vec3(0,0,0), glm::vec3(3,0,0), glm::vec3(0,4,0) })
            {
                float dist = glm::distance(p, sphere.GetCenter());
                CHECK(dist <= sphere.GetRadius() + 0.001f);
            }
        }
    }

    SECTION("Combining MeshData")
    {
        Ref<MeshData> mesh1 = MeshData::Create(3, 3, layout);
        Ref<MeshData> mesh2 = MeshData::Create(3, 3, layout);

        mesh1->SetPositions({ {0,0,0}, {1,0,0}, {0,1,0} });
        mesh1->SetIndices({ 0, 1, 2 });
        
        mesh2->SetPositions({ {10,10,10}, {11,10,10}, {10,11,10} });
        mesh2->SetIndices({ 0, 1, 2 });

        Vector<Ref<MeshData>> meshes = { mesh1, mesh2 };
        Vector<Vector<SubMesh>> subMeshes = { 
            { { 0, 3, DrawMode::TRIANGLE_LIST } },
            { { 0, 3, DrawMode::TRIANGLE_LIST } }
        };

        Vector<SubMesh> outSubMeshes;
        Ref<MeshData> combined = MeshData::Combine(meshes, subMeshes, outSubMeshes);

        REQUIRE(combined != nullptr);
        CHECK(combined->GetVertexCount() == 6);
        CHECK(combined->GetIndexCount() == 6);
        REQUIRE(outSubMeshes.size() == 2);
        CHECK(outSubMeshes[0].IndexOffset == 0);
        CHECK(outSubMeshes[0].IndexCount == 3);
        CHECK(outSubMeshes[1].IndexOffset == 3);
        CHECK(outSubMeshes[1].IndexCount == 3);
        
        Vector<uint32_t> indices = combined->GetIndices();
        CHECK(indices[0] == 0);
        CHECK(indices[3] == 3); // 0 + vertexOffset(3)
        CHECK(indices[5] == 5); // 2 + vertexOffset(3)

        Vector<glm::vec3> pos = combined->GetPositions();
        CHECK(pos[0] == glm::vec3(0,0,0));
        CHECK(pos[3] == glm::vec3(10,10,10));
    }

    SECTION("Combining MeshData with all 16-bit indices")
    {
        Ref<MeshData> mesh1 = MeshData::Create(3, 3, layout, IndexType::Index_16);
        Ref<MeshData> mesh2 = MeshData::Create(3, 3, layout, IndexType::Index_16);

        mesh1->SetIndices({ 0, 1, 2 });
        mesh2->SetIndices({ 0, 1, 2 });

        Vector<Ref<MeshData>> meshes = { mesh1, mesh2 };
        Vector<Vector<SubMesh>> subMeshes = {
            { { 0, 3, DrawMode::TRIANGLE_LIST } },
            { { 0, 3, DrawMode::TRIANGLE_LIST } }
        };

        Vector<SubMesh> outSubMeshes;
        Ref<MeshData> combined = MeshData::Combine(meshes, subMeshes, outSubMeshes);

        REQUIRE(combined != nullptr);
        CHECK(combined->GetIndexType() == IndexType::Index_16);
        CHECK(combined->GetIndexCount() == 6);

        Vector<uint32_t> indices = combined->GetIndices();
        CHECK(indices[0] == 0);
        CHECK(indices[3] == 3);
        CHECK(indices[5] == 5);
    }

    SECTION("Combining MeshData with mixed 16/32-bit indices")
    {
        Ref<MeshData> mesh1 = MeshData::Create(3, 3, layout, IndexType::Index_16);
        Ref<MeshData> mesh2 = MeshData::Create(3, 3, layout, IndexType::Index_32);

        mesh1->SetIndices({ 0, 1, 2 });
        mesh2->SetIndices({ 0, 1, 2 });

        Vector<Ref<MeshData>> meshes = { mesh1, mesh2 };
        Vector<Vector<SubMesh>> subMeshes = { 
            { { 0, 3, DrawMode::TRIANGLE_LIST } },
            { { 0, 3, DrawMode::TRIANGLE_LIST } }
        };

        Vector<SubMesh> outSubMeshes;
        Ref<MeshData> combined = MeshData::Combine(meshes, subMeshes, outSubMeshes);

        REQUIRE(combined != nullptr);
        CHECK(combined->GetIndexType() == IndexType::Index_32);
        CHECK(combined->GetIndexCount() == 6);

        Vector<uint32_t> indices = combined->GetIndices();
        CHECK(indices[0] == 0);
        CHECK(indices[3] == 3);
        CHECK(indices[5] == 5);
    }

    SECTION("Combining MeshData merges layouts and zero-fills missing attributes")
    {
        BufferLayout positionLayout = { { ShaderDataType::Float3, VertexAttribute::Position } };
        BufferLayout coloredLayout = { { ShaderDataType::Float3, VertexAttribute::Position },
                                       { ShaderDataType::Float4, VertexAttribute::Color } };
        Ref<MeshData> mesh1 = MeshData::Create(1, 1, positionLayout, IndexType::Index_16);
        Ref<MeshData> mesh2 = MeshData::Create(1, 1, coloredLayout, IndexType::Index_16);
        mesh1->SetPositions({ { 1.0f, 2.0f, 3.0f } });
        mesh1->SetIndices({ 0 });
        mesh2->SetPositions({ { 4.0f, 5.0f, 6.0f } });
        mesh2->SetColors({ { 0.25f, 0.5f, 0.75f, 1.0f } });
        mesh2->SetIndices({ 0 });

        Vector<SubMesh> outSubMeshes;
        Ref<MeshData> combined = MeshData::Combine({ mesh1, mesh2 },
                                                   { { { 0, 1, DrawMode::POINT_LIST } }, { { 0, 1, DrawMode::POINT_LIST } } }, outSubMeshes);

        REQUIRE(combined != nullptr);
        CHECK(combined->GetBufferLayout().HasAttribute(VertexAttribute::Color));
        const Vector<glm::vec4> colors = combined->GetColors();
        CHECK(colors[0] == glm::vec4(0.0f));
        CHECK(colors[1] == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
        REQUIRE(outSubMeshes.size() == 2);
        CHECK(outSubMeshes[0].MeshDrawMode == DrawMode::POINT_LIST);
        CHECK(outSubMeshes[1].IndexOffset == 1);
    }
}
