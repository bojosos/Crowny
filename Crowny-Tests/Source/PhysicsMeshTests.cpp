#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Physics/PhysicsMesh.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/MeshFactory.h"

#include <algorithm>
#include <set>

using namespace Crowny;

namespace
{
    Vector<glm::vec3> QuadPositions()
    {
        // Two triangles sharing an edge, with the shared vertices duplicated the way a renderer would emit them.
        return { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
    }
} // namespace

TEST_CASE("PhysicsMesh welds duplicate positions and drops degenerate triangles", "[Physics][PhysicsMesh]")
{
    Vector<glm::vec3> positions = QuadPositions();
    positions.push_back({ 5.0f, 5.0f, 5.0f }); // referenced only by degenerate triangles
    Vector<uint32_t> indices = { 0, 1, 2, 3, 4, 5, 6, 6, 6, 0, 1, 1, 0, 1, 99 };

    const Ref<PhysicsMesh> mesh = PhysicsMesh::Build(positions, indices);
    REQUIRE(mesh != nullptr);
    CHECK(mesh->GetPositions().size() == 4);
    CHECK(mesh->GetTriangleCount() == 2);
    CHECK(mesh->GetSourceVertexCount() == 7);
    CHECK_FALSE(mesh->IsEmpty());
    CHECK(mesh->IsConvexCapable());
    for (uint32_t index : mesh->GetIndices())
        CHECK(index < mesh->GetPositions().size());
    CHECK(mesh->GetBounds().GetMin() == glm::vec3(0.0f));
    CHECK(mesh->GetBounds().GetMax() == glm::vec3(1.0f, 0.0f, 1.0f));
    CHECK(mesh->GetEdges().size() == 5);
    CHECK(mesh->GetConvexPoints().size() == 4);
}

TEST_CASE("PhysicsMesh only cooks triangle-list sub-meshes", "[Physics][PhysicsMesh]")
{
    const Ref<MeshData> cube = MeshFactory::CreateCubeData(1.0f);
    REQUIRE(cube != nullptr);

    const Ref<PhysicsMesh> whole = PhysicsMesh::Build(*cube, {});
    REQUIRE(whole != nullptr);
    CHECK(whole->GetPositions().size() == 8);
    CHECK(whole->GetTriangleCount() == 12);
    CHECK(whole->GetEdges().size() == 18);
    CHECK_THAT(whole->GetBounds().GetMin().x, Catch::Matchers::WithinAbs(-0.5f, 0.0001f));
    CHECK_THAT(whole->GetBounds().GetMax().y, Catch::Matchers::WithinAbs(0.5f, 0.0001f));

    const Vector<SubMesh> lines = { SubMesh(0, cube->GetIndexCount(), DrawMode::LINE_LIST) };
    const Ref<PhysicsMesh> noTriangles = PhysicsMesh::Build(*cube, lines);
    if (noTriangles != nullptr)
    {
        CHECK(noTriangles->GetTriangleCount() == 0);
        CHECK(noTriangles->IsConvexCapable());
    }

    const Vector<SubMesh> half = { SubMesh(0, cube->GetIndexCount() / 2, DrawMode::TRIANGLE_LIST) };
    const Ref<PhysicsMesh> partial = PhysicsMesh::Build(*cube, half);
    REQUIRE(partial != nullptr);
    CHECK(partial->GetTriangleCount() == 6);
}

TEST_CASE("PhysicsMesh caps the convex point cloud and keeps extreme points", "[Physics][PhysicsMesh]")
{
    Vector<glm::vec3> positions;
    for (int x = 0; x < 10; x++)
        for (int y = 0; y < 10; y++)
            for (int z = 0; z < 10; z++)
                positions.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

    PhysicsMeshBuildSettings settings;
    settings.MaxConvexPoints = 32;
    const Ref<PhysicsMesh> mesh = PhysicsMesh::Build(positions, {}, settings);
    REQUIRE(mesh != nullptr);
    CHECK(mesh->GetPositions().size() == 1000);
    CHECK(mesh->GetTriangleCount() == 0);
    REQUIRE(mesh->GetConvexPoints().size() == 32);
    const auto contains = [&](const glm::vec3& point) {
        return std::find(mesh->GetConvexPoints().begin(), mesh->GetConvexPoints().end(), point) != mesh->GetConvexPoints().end();
    };
    CHECK(contains({ 0.0f, 0.0f, 0.0f }));
    CHECK(contains({ 9.0f, 9.0f, 9.0f }));
    CHECK(contains({ 9.0f, 0.0f, 0.0f }));
    CHECK(contains({ 0.0f, 9.0f, 0.0f }));

    settings.MaxConvexPoints = 0;
    const Ref<PhysicsMesh> uncapped = PhysicsMesh::Build(positions, {}, settings);
    REQUIRE(uncapped != nullptr);
    CHECK(uncapped->GetConvexPoints().size() == 1000);
}

TEST_CASE("PhysicsMesh scaled copies flip winding for mirrored scales", "[Physics][PhysicsMesh]")
{
    const Ref<PhysicsMesh> mesh = PhysicsMesh::Build(QuadPositions(), { 0, 1, 2, 3, 4, 5 });
    REQUIRE(mesh != nullptr);

    Vector<glm::vec3> vertices;
    Vector<uint32_t> indices;
    mesh->CopyScaled({ 2.0f, 3.0f, 4.0f }, false, vertices, indices);
    REQUIRE(vertices.size() == mesh->GetPositions().size());
    CHECK(indices == mesh->GetIndices());
    for (size_t i = 0; i < vertices.size(); i++)
        CHECK(vertices[i] == mesh->GetPositions()[i] * glm::vec3(2.0f, 3.0f, 4.0f));

    mesh->CopyScaled({ -1.0f, 1.0f, 1.0f }, false, vertices, indices);
    REQUIRE(indices.size() == mesh->GetIndices().size());
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        CHECK(indices[i] == mesh->GetIndices()[i]);
        CHECK(indices[i + 1] == mesh->GetIndices()[i + 2]);
        CHECK(indices[i + 2] == mesh->GetIndices()[i + 1]);
    }

    mesh->CopyScaled({ -1.0f, -1.0f, 1.0f }, false, vertices, indices);
    CHECK(indices == mesh->GetIndices());

    mesh->CopyScaled({ 1.0f, 1.0f, 1.0f }, true, vertices, indices);
    CHECK(indices.empty());
    CHECK(vertices.size() == mesh->GetConvexPoints().size());
}

TEST_CASE("PhysicsMesh rejects unusable input", "[Physics][PhysicsMesh]")
{
    CHECK(PhysicsMesh::Build(Vector<glm::vec3>{}, Vector<uint32_t>{}) == nullptr);
    CHECK(PhysicsMesh::Build(Vector<glm::vec3>{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }, Vector<uint32_t>{ 0, 1, 2 }) ==
          nullptr);
    const Ref<PhysicsMesh> coplanar =
      PhysicsMesh::Build(Vector<glm::vec3>{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, Vector<uint32_t>{ 0, 1, 2 });
    REQUIRE(coplanar != nullptr);
    CHECK(coplanar->GetTriangleCount() == 1);
    CHECK_FALSE(coplanar->IsConvexCapable());
}

TEST_CASE("PhysicsMesh assets survive binary round trips", "[Assets][Physics][PhysicsMesh][Serialization]")
{
    const Path assetPath = fs::temp_directory_path() / "crowny-physics-mesh-roundtrip.asset";
    fs::remove(assetPath);

    AssetManager manager;
    const Ref<MeshData> cube = MeshFactory::CreateCubeData(2.0f);
    REQUIRE(cube != nullptr);
    PhysicsMeshBuildSettings settings;
    settings.MaxConvexPoints = 6;
    const Ref<PhysicsMesh> source = PhysicsMesh::Build(*cube, { SubMesh(0, cube->GetIndexCount(), DrawMode::TRIANGLE_LIST) }, settings);
    REQUIRE(source != nullptr);
    source->SetName("Cube Collision");
    REQUIRE(manager.Save(source, assetPath));

    const AssetHandle<PhysicsMesh> loaded = manager.Load<PhysicsMesh>(assetPath, false);
    REQUIRE(loaded);
    CHECK(loaded->GetName() == "Cube Collision");
    CHECK(loaded->GetPositions() == source->GetPositions());
    CHECK(loaded->GetIndices() == source->GetIndices());
    CHECK(loaded->GetConvexPoints() == source->GetConvexPoints());
    CHECK(loaded->GetSourceVertexCount() == source->GetSourceVertexCount());
    CHECK(loaded->GetBounds().GetMin() == source->GetBounds().GetMin());
    CHECK(loaded->GetBounds().GetMax() == source->GetBounds().GetMax());
    fs::remove(assetPath);
}

TEST_CASE("PhysicsMeshResolver prefers explicit registrations", "[Physics][PhysicsMesh]")
{
    AssetManager manager;
    const UUID meshUuid = UuidGenerator::Generate();
    const AssetHandle<Mesh> meshHandle = static_asset_cast<Mesh>(manager.GetAssetHandle(meshUuid));
    REQUIRE(meshHandle.HasUUID());
    CHECK_FALSE(meshHandle.IsLoaded());

    CHECK(PhysicsMeshResolver::Resolve(meshHandle) == nullptr);
    String error;
    CHECK(PhysicsMeshResolver::Resolve(AssetHandle<Mesh>{}, &error) == nullptr);
    CHECK_FALSE(error.empty());

    const Ref<MeshData> cube = MeshFactory::CreateCubeData(1.0f);
    const AssetHandle<PhysicsMesh> collision = CreateRuntimePhysicsMesh(manager, PhysicsMesh::Build(*cube, {}));
    REQUIRE(collision);
    PhysicsMeshResolver::Register(meshUuid, collision);
    CHECK(PhysicsMeshResolver::Resolve(meshHandle) == collision.GetInternalPtr());
    CHECK(PhysicsMeshResolver::CanResolve(meshHandle));

    PhysicsMeshResolver::Unregister(meshUuid);
    CHECK(PhysicsMeshResolver::Resolve(meshHandle) == nullptr);
}
