#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Renderer/PrimitiveMeshLibrary.h"
#include "Crowny/Scene/Scene.h"
#include "Editor/EntityFactory.h"

using namespace Crowny;

namespace
{
    constexpr PrimitiveMeshType AllPrimitives[] = { PrimitiveMeshType::Cube,     PrimitiveMeshType::Sphere, PrimitiveMeshType::Plane,
                                                    PrimitiveMeshType::Cylinder, PrimitiveMeshType::Cone,   PrimitiveMeshType::Capsule };
}

TEST_CASE("Primitive mesh library exposes stable, unique identifiers", "[Editor][EntityFactory][Renderer]")
{
    Vector<UUID> seen;
    for (PrimitiveMeshType type : AllPrimitives)
    {
        const UUID& uuid = PrimitiveMeshLibrary::GetUuid(type);
        REQUIRE(uuid != UUID::EMPTY);
        CHECK(std::find(seen.begin(), seen.end(), uuid) == seen.end());
        seen.push_back(uuid);

        PrimitiveMeshType resolved = PrimitiveMeshType::Count;
        REQUIRE(PrimitiveMeshLibrary::TryGetType(uuid, resolved));
        CHECK(resolved == type);
        CHECK(PrimitiveMeshLibrary::IsPrimitiveMesh(uuid));
        CHECK(String(PrimitiveMeshLibrary::GetName(type)).size() > 0);

        Ref<MeshData> data = PrimitiveMeshLibrary::CreateData(type);
        REQUIRE(data);
        CHECK(data->GetVertexCount() > 0);
        CHECK(data->GetIndexCount() >= 3);
    }

    // The identifiers are part of the scene format; changing them breaks saved scenes.
    CHECK(PrimitiveMeshLibrary::GetUuid(PrimitiveMeshType::Cube) == UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0001u, 0x43726f77u));
    CHECK_FALSE(PrimitiveMeshLibrary::IsPrimitiveMesh(UUID::EMPTY));
    CHECK_FALSE(PrimitiveMeshLibrary::IsPrimitiveMesh(UuidGenerator::Generate()));
    CHECK(PrimitiveMeshLibrary::GetUuid(PrimitiveMeshType::Count) == UUID::EMPTY);
}

TEST_CASE("Entity factory creates lights parented to the requested entity", "[Editor][EntityFactory]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");

    Entity directional = EntityFactory::CreateLight(scene, parent, LightType::Directional);
    REQUIRE(directional);
    CHECK(directional.GetName() == "Directional Light");
    CHECK(directional.GetParent() == parent);
    REQUIRE(directional.HasComponent<LightComponent>());
    const LightComponent& sun = directional.GetComponent<LightComponent>();
    CHECK(sun.Type == LightType::Directional);
    CHECK(sun.Intensity > 0.0f);
    CHECK(sun.Shadows.Mode != LightShadowMode::Disabled);
    const glm::quat rotation = directional.GetTransform().GetLocalTransform().GetRotation();
    CHECK_FALSE(glm::all(glm::epsilonEqual(rotation, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 1e-4f)));

    Entity point = EntityFactory::CreateLight(scene, Entity{}, LightType::Point);
    REQUIRE(point);
    CHECK(point.GetName() == "Point Light");
    CHECK(point.GetParent() == scene->GetRootEntity());
    CHECK(point.GetComponent<LightComponent>().Type == LightType::Point);

    Entity spot = EntityFactory::CreateLight(scene, parent, LightType::Spot);
    REQUIRE(spot);
    CHECK(spot.GetName() == "Spot Light");
    const LightComponent& spotLight = spot.GetComponent<LightComponent>();
    CHECK(spotLight.Type == LightType::Spot);
    CHECK(spotLight.SpotInnerAngle < spotLight.SpotOuterAngle);

    // A parent from another scene falls back to the scene root instead of cross-linking scenes.
    Ref<Scene> other = CreateRef<Scene>(false);
    Entity foreignParent = other->CreateEntity("Foreign");
    Entity fallback = EntityFactory::CreateEmpty(scene, foreignParent);
    REQUIRE(fallback);
    CHECK(fallback.GetName() == EntityFactory::DefaultEntityName);
    CHECK(fallback.GetParent() == scene->GetRootEntity());

    CHECK_FALSE(EntityFactory::CreateLight(nullptr, parent, LightType::Point));
}

TEST_CASE("Entity factory primitives reference the shared built-in mesh without a renderer", "[Editor][EntityFactory][Assets]")
{
    AssetManager assetManager;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");

    for (PrimitiveMeshType type : AllPrimitives)
    {
        Entity entity = EntityFactory::CreatePrimitive(scene, parent, type);
        REQUIRE(entity);
        CHECK(entity.GetName() == PrimitiveMeshLibrary::GetName(type));
        CHECK(entity.GetParent() == parent);
        REQUIRE(entity.HasComponent<MeshRendererComponent>());
        const MeshRendererComponent& meshRenderer = entity.GetComponent<MeshRendererComponent>();
        // Without a RenderAPI no GPU mesh exists, but the UUID that gets serialized must be the fixed one.
        CHECK(meshRenderer.MeshHandle.GetUUID() == PrimitiveMeshLibrary::GetUuid(type));
        CHECK(meshRenderer.Materials.empty());
    }

    // Two cubes share one handle identity.
    Entity first = EntityFactory::CreatePrimitive(scene, Entity{}, PrimitiveMeshType::Cube);
    Entity second = EntityFactory::CreatePrimitive(scene, Entity{}, PrimitiveMeshType::Cube);
    CHECK(first.GetComponent<MeshRendererComponent>().MeshHandle.GetUUID() == second.GetComponent<MeshRendererComponent>().MeshHandle.GetUUID());
    CHECK(first.GetParent() == scene->GetRootEntity());

    CHECK_FALSE(EntityFactory::CreatePrimitive(scene, parent, PrimitiveMeshType::Count));
    PrimitiveMeshLibrary::Shutdown();
}
