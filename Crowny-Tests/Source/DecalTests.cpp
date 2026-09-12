#include "cwtpch.h"

#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Decal.h"
#include "Crowny/Renderer/DecalRenderer.h"
#include "Crowny/Renderer/DecalWorld.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/EntityInstantiation.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/PrefabSync.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SceneComponentCodec.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>

using namespace Crowny;

TEST_CASE("Scene simulation advances decal lifetime and honors expiry policy", "[decals][Scene]")
{
    auto scene = CreateRef<Scene>(false);
    Entity label = scene->CreateEntity("Persistent owner");
    auto& decal = label.AddComponent<DecalComponent>();
    decal.FadeIn = 1.0f;
    decal.Lifetime = 2.0f;
    decal.FadeOut = 1.0f;
    scene->OnUpdateEditor(Timestep(1.0f));
    CHECK(decal.Age == 0.0f);
    scene->OnRuntimeStart();
    scene->OnFixedUpdate(Timestep(0.5f));
    CHECK(decal.Age == Catch::Approx(0.5f));
    CHECK(DecalMath::LifetimeOpacity(decal, decal.Age) == Catch::Approx(0.5f));
    decal.StopLifetime();
    scene->OnFixedUpdate(Timestep(1.0f));
    CHECK(decal.Age == Catch::Approx(0.5f));
    decal.RestartLifetime();
    scene->OnFixedUpdate(Timestep(2.5f));
    CHECK(decal.Enabled);
    CHECK(DecalMath::LifetimeOpacity(decal, decal.Age) == Catch::Approx(0.5f));
    scene->OnFixedUpdate(Timestep(0.5f));
    CHECK_FALSE(decal.Enabled);
    CHECK(scene->TryGetEntityFromUuid(label.GetUuid()));

    Entity temporary = scene->CreateEntity("Explicitly disposable owner");
    const UUID temporaryId = temporary.GetUuid();
    auto& disposable = temporary.AddComponent<DecalComponent>();
    disposable.Lifetime = 0.25f;
    disposable.DestroyOwnerOnExpiry = true;
    scene->OnFixedUpdate(Timestep(0.25f));
    CHECK_FALSE(scene->TryGetEntityFromUuid(temporaryId));
    scene->OnRuntimeStop();
    decal.RestartLifetime();
    scene->OnFixedUpdate(Timestep(1.0f));
    CHECK(decal.Age == 0.0f);
}

TEST_CASE("Decal boxes clip and retain projection under parent scale", "[decals]")
{
    DecalSettings settings;
    const glm::mat4 world = glm::scale(glm::mat4(1), glm::vec3(2, 3, 4));
    auto sample = DecalMath::Project(settings, world, { 0.5f, 0, 0 }, { 0, 0, 1 });
    REQUIRE(sample.Coverage == 1.0f);
    REQUIRE(sample.UV.x == Catch::Approx(0.75f));
    REQUIRE(DecalMath::Project(settings, world, { 1.1f, 0, 0 }, { 0, 0, 1 }).Coverage == 0.0f);
    REQUIRE(DecalMath::Project(settings, world, { 0, 0, 0 }, { 0, 0, -1 }).Coverage == 0.0f);
    REQUIRE_FALSE(DecalMath::IsValid(settings, glm::mat4(0)));
}

TEST_CASE("Tapered decals reject the interior and wrap their seam", "[decals]")
{
    DecalSettings s;
    s.Projection = DecalProjection::Cylinder;
    s.TopRadius = 0.75f;
    s.AngleFadeStart = 120;
    s.AngleFadeEnd = 150;
    REQUIRE(DecalMath::Project(s, glm::mat4(1), { 0, 0, 0.625f }, { 0, 0, 1 }).Coverage == 1.0f);
    REQUIRE(DecalMath::Project(s, glm::mat4(1), { 0, 0, 0.1f }, { 0, 0, 1 }).Coverage == 0.0f);
    auto left = DecalMath::Project(s, glm::mat4(1), { -0.0001f, 0, 0.625f }, { 0, 0, 1 });
    auto right = DecalMath::Project(s, glm::mat4(1), { 0.0001f, 0, 0.625f }, { 0, 0, 1 });
    REQUIRE(left.UV.x > 0.99f);
    REQUIRE(right.UV.x < 0.01f);
    REQUIRE(left.Coverage == right.Coverage);
    s.Arc = 90;
    REQUIRE(DecalMath::Project(s, glm::mat4(1), { -0.625f, 0, 0 }, { -1, 0, 0 }).Coverage == 0.0f);
}

TEST_CASE("Decal corrections and coating preserve independent channels", "[decals]")
{
    DecalSurface s;
    s.Color = { 0.2f, 0.4f, 0.7f };
    s.Opacity = 0.2f;
    DecalMaterialDesc m;
    m.Channels = 64u;
    DecalMath::Blend(s, m, 1);
    REQUIRE(s.Color.x == Catch::Approx(0.2f));
    REQUIRE(s.Color.y == Catch::Approx(0.4f));
    REQUIRE(s.Opacity == 0.2f);
    m.Exposure = 1;
    DecalMath::Blend(s, m, 0.5f);
    REQUIRE(s.Color.x == Catch::Approx(0.3f));
    REQUIRE(s.Roughness == 0.5f);
    m.Channels = 128u;
    DecalMath::Blend(s, m, 0.5f);
    REQUIRE(s.Opacity == Catch::Approx(0.6f));
    DecalMath::Blend(s, m, 1.0f);
    REQUIRE(s.Opacity == 1.0f);
}

TEST_CASE("Decal lifetime is persistent by default and fades at expiry", "[decals]")
{
    DecalSettings s;
    REQUIRE(DecalMath::LifetimeOpacity(s, 10000) == 1);
    s.FadeIn = 2;
    s.Lifetime = 10;
    s.FadeOut = 4;
    REQUIRE(DecalMath::LifetimeOpacity(s, 1) == Catch::Approx(0.5f));
    REQUIRE(DecalMath::LifetimeOpacity(s, 12) == Catch::Approx(0.5f));
    REQUIRE(DecalMath::LifetimeOpacity(s, 14) == 0);
}

TEST_CASE("Decal handles reject stale updates after slot reuse", "[decals]")
{
    DecalWorld world;
    auto first = world.Create({});
    REQUIRE(world.Destroy(first));
    auto second = world.Create({});
    REQUIRE(first.Index == second.Index);
    REQUIRE_FALSE(world.Update(first, {}));
    REQUIRE(world.Update(second, {}));
    Vector<DecalChange> changes;
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 4);
    world.Clear();
    REQUIRE_FALSE(world.IsAlive(second));
}

TEST_CASE("Decal clusters retain order and use an explicit complete-list overflow", "[decals]")
{
    ClusteredLightGridDesc desc;
    desc.ViewportWidth = desc.ViewportHeight = 64;
    DecalClusterGrid grid;
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, desc.NearPlane, desc.FarPlane);
    Vector<glm::vec4> spheres(64, glm::vec4(0, 0, -2, 0.5f));
    grid.Build(desc, glm::mat4(1), projection, spheres);
    REQUIRE(grid.Overflow == 0);
    size_t occupied = 0;
    for (auto cell : grid.Cells)
        if (cell.y)
        {
            ++occupied;
            REQUIRE(cell.y == 64);
            for (uint32_t i = 0; i < cell.y; ++i)
                REQUIRE(grid.Indices[cell.x + i] == i);
        }
    REQUIRE(occupied > 0);
    spheres.push_back(spheres.front());
    grid.Build(desc, glm::mat4(1), projection, spheres);
    REQUIRE(grid.Overflow == occupied);
    REQUIRE(grid.Indices.empty());
    for (auto cell : grid.Cells)
        REQUIRE((cell.y == 0 || cell.y == 0xffffffffu));
    grid.Build(desc, glm::mat4(1), projection, {});
    REQUIRE(grid.Overflow == 0);
    REQUIRE(grid.Indices.empty());
}

TEST_CASE("Decal channel rejection preserves all receiver attributes", "[decals]")
{
    DecalSurface surface;
    surface.Color = { 0.1f, 0.2f, 0.3f };
    surface.Roughness = 0.2f;
    DecalMaterialDesc material;
    material.Channels = 255;
    material.Exposure = 4;
    DecalMath::Blend(surface, material, 1, 0);
    REQUIRE(surface.Color == glm::vec3(0.1f, 0.2f, 0.3f));
    REQUIRE(surface.Roughness == 0.2f);
    material.Channels = 4;
    material.RoughnessBlend = DecalBlend::Multiply;
    material.Roughness = 0.5f;
    DecalMath::Blend(surface, material, 1);
    REQUIRE(surface.Roughness == Catch::Approx(0.1f));
    REQUIRE(surface.Color == glm::vec3(0.1f, 0.2f, 0.3f));
}

TEST_CASE("Decal gradients preserve existing detail and support replacement", "[decals]")
{
    DecalSurface surface;
    surface.Normal = glm::normalize(glm::vec3(0.2f, 0, 1));
    const auto original = surface.Normal;
    DecalMaterialDesc material;
    material.Channels = 2;
    DecalMath::Blend(surface, material, 1);
    CHECK(glm::distance(surface.Normal, original) < 0.00001f);
    material.NormalGradient = { 0, -0.3f, 0 };
    DecalMath::Blend(surface, material, 1);
    CHECK(surface.Normal.x > 0);
    CHECK(surface.Normal.y > 0);
    material.ReplaceNormal = true;
    material.NormalGradient = glm::vec3(0);
    DecalMath::Blend(surface, material, 1);
    CHECK(surface.Normal == glm::vec3(0, 0, 1));
    material.Channels = 4;
    surface.Roughness = 0;
    DecalMath::Blend(surface, material, 0);
    CHECK(surface.Roughness == 0);
    material.Strengths.z = 0;
    DecalMath::Blend(surface, material, 1);
    CHECK(surface.Roughness == 0);
    material.Channels = 8;
    material.Strengths.w = 0;
    surface.Metallic = 1.2f;
    DecalMath::Blend(surface, material, 1);
    CHECK(surface.Metallic == 1.2f);
}

TEST_CASE("Decal settings reject invalid angles and preserve explicit UV mirroring", "[decals]")
{
    DecalSettings s;
    CHECK(DecalMath::IsValid(s, glm::scale(glm::mat4(1), glm::vec3(0.001f))));
    s.AngleFadeStart = 190;
    REQUIRE_FALSE(DecalMath::IsValid(s, glm::mat4(1)));
    s.AngleFadeStart = 60;
    s.Size.z = 0;
    REQUIRE_FALSE(DecalMath::IsValid(s, glm::mat4(1)));
    s.Size.z = 1;
    s.UVScale.x = -1;
    auto sample = DecalMath::Project(s, glm::mat4(1), { 0.25f, 0, 0 }, { 0, 0, 1 });
    REQUIRE(sample.UV.x == Catch::Approx(0.25f));
    REQUIRE(sample.Coverage == 1);
    s.EdgeFeather = 0.5f;
    REQUIRE(DecalMath::Project(s, glm::mat4(1), { 0.25f, 0, 0 }, { 0, 0, 1 }).Coverage == Catch::Approx(0.5f));
}

TEST_CASE("Copied decal receiver references follow the copied subtree", "[decals]")
{
    auto scene = CreateRef<Scene>();
    Entity bottle = scene->CreateEntity("Bottle");
    Entity label = scene->CreateEntity("Label");
    label.SetParent(bottle);
    auto& decal = label.AddComponent<DecalComponent>();
    decal.Target = bottle.GetUuid();
    decal.TargetMode = DecalTargetMode::Entity;
    decal.Age = 12;
    Entity copy = scene->DuplicateEntity(bottle);
    REQUIRE(copy.GetUuid() != bottle.GetUuid());
    REQUIRE(copy.GetChildren().size() == 1);
    const auto& copied = copy.GetChildren()[0].GetComponent<DecalComponent>();
    REQUIRE(copied.Target == copy.GetUuid());
    REQUIRE(copied.Age == 0);
    REQUIRE(decal.Target == bottle.GetUuid());
}

TEST_CASE("Decal YAML codecs retain settings and reset transient lifetime state", "[decals]")
{
    auto scene = CreateRef<Scene>();
    Entity source = scene->CreateEntity("Decal");
    auto& decal = source.AddComponent<DecalComponent>();
    decal.Projection = DecalProjection::Cylinder;
    decal.TopRadius = 0.7f;
    decal.Target = source.GetUuid();
    decal.UVScale = { -2, 3 };
    decal.SortOrder = -7;
    decal.Age = 13;
    const DecalSettings expected = decal;
    const auto* codec = FindSceneComponentCodec(SceneComponentId::Decal);
    REQUIRE(codec != nullptr);
    YAML::Emitter yaml;
    yaml << YAML::BeginMap;
    codec->WriteYaml(yaml, source);
    yaml << YAML::EndMap;
    Entity copy = scene->CreateEntity("Restored");
    SceneComponentReadContext context;
    context.TargetScene = scene.get();
    codec->ReadYaml(YAML::Load(yaml.c_str()), copy, context);
    const auto& restored = copy.GetComponent<DecalComponent>();
    REQUIRE(static_cast<const DecalSettings&>(restored) == expected);
    REQUIRE(restored.Age == 0);
}

TEST_CASE("Decal binary codecs retain every authored field without serializing playback", "[decals][Serialization]")
{
    auto scene = CreateRef<Scene>(false);
    Entity source = scene->CreateEntity("Label");
    auto& decal = source.AddComponent<DecalComponent>();
    decal.Projection = DecalProjection::Cylinder;
    decal.Offset = { 1, 2, 3 };
    decal.Size = { 2, 3, 4 };
    decal.BottomRadius = 0.8f;
    decal.TopRadius = 0.6f;
    decal.Height = 2;
    decal.ShellThickness = 0.03f;
    decal.Arc = 270;
    decal.SeamRotation = -45;
    decal.Tint = { 0.2f, 0.3f, 0.4f, 0.5f };
    decal.Opacity = 0.7f;
    decal.UVOffset = { 0.1f, -0.2f };
    decal.UVScale = { -2, 3 };
    decal.UVRotation = 37;
    decal.PreserveTexelDensity = true;
    decal.SortOrder = -19;
    decal.ReceiverLayers = 0x80000002u;
    decal.TargetMode = DecalTargetMode::Subtree;
    decal.Target = source.GetUuid();
    decal.EdgeFeather = 0.02f;
    decal.DepthFeather = 0.01f;
    decal.AngleFadeStart = 45;
    decal.AngleFadeEnd = 75;
    decal.DistanceFadeStart = 20;
    decal.DistanceFadeEnd = 50;
    decal.Enabled = false;
    decal.FadeIn = 2;
    decal.Lifetime = 10;
    decal.FadeOut = 3;
    decal.DestroyOwnerOnExpiry = true;
    decal.Age = 6;
    decal.LifetimeRunning = false;
    const DecalSettings expected = decal;
    const auto* codec = FindSceneComponentCodec(SceneComponentId::Decal);
    auto stream = CreateRef<MemoryDataStream>();
    BinaryDataStreamOutputArchive output(stream);
    codec->WriteBinary(output, source);
    output(uint32_t(0xdecafbad));
    stream->Seek(0);
    BinaryDataStreamInputArchive input(stream);
    SceneComponentReadContext context;
    context.TargetScene = scene.get();
    Entity copy = scene->CreateEntity("Restored");
    codec->ReadBinary(input, copy, context);
    uint32_t sentinel = 0;
    input(sentinel);
    CHECK(sentinel == 0xdecafbadu);
    const auto& restored = copy.GetComponent<DecalComponent>();
    CHECK(static_cast<const DecalSettings&>(restored) == expected);
    CHECK(restored.Age == 0);
    CHECK(restored.LifetimeRunning);
}

TEST_CASE("Pre-decal receiver records keep their defaults and binary alignment", "[decals][Serialization]")
{
    for (uint32_t version : { 12u, 13u })
    {
        CAPTURE(version);
        auto scene = CreateRef<Scene>(false);
        SceneComponentReadContext context;
        context.TargetScene = scene.get();
        context.FormatVersion = version;
        for (auto id : { SceneComponentId::MeshRenderer, SceneComponentId::ProceduralMesh })
        {
            CAPTURE(static_cast<uint32_t>(id));
            auto stream = CreateRef<MemoryDataStream>();
            BinaryDataStreamOutputArchive output(stream);
            if (id == SceneComponentId::MeshRenderer)
                output(UUID::EMPTY, uint32_t(7), 0.0f, int32_t(-2), true, true, true, true, uint32_t(0));
            else
                output(UUID::EMPTY, uint32_t(0), uint32_t(0));
            output(uint32_t(0xdecafbad));
            stream->Seek(0);
            BinaryDataStreamInputArchive input(stream);
            Entity receiver = scene->CreateEntity("Legacy receiver");
            FindSceneComponentCodec(id)->ReadBinary(input, receiver, context);
            uint32_t sentinel = 0;
            input(sentinel);
            CHECK(sentinel == 0xdecafbadu);
            if (id == SceneComponentId::MeshRenderer)
            {
                const auto& mesh = receiver.GetComponent<MeshRendererComponent>();
                CHECK(mesh.ReceiveDecals);
                CHECK(mesh.DecalLayers == 0xffffffffu);
                CHECK(mesh.VisibilityLayers.Value == 7);
            }
            else
            {
                const auto& mesh = receiver.GetComponent<ProceduralMeshComponent>();
                CHECK(mesh.ReceiveDecals);
                CHECK(mesh.DecalLayers == 0xffffffffu);
            }
        }
    }
}

TEST_CASE("Decal prefab capture retains receivers and sync respects individual overrides", "[decals][Prefab]")
{
    auto scene = CreateRef<Scene>(false);
    Entity bottle = scene->CreateEntity("Bottle");
    Entity label = scene->CreateEntity("Label");
    label.SetParent(bottle);
    auto& decal = label.AddComponent<DecalComponent>();
    decal.TargetMode = DecalTargetMode::Entity;
    decal.Target = bottle.GetUuid();
    decal.Size = { 2, 3, 0.1f };
    Prefab prefab;
    prefab.CaptureFromEntity(*scene, bottle);
    const Entity capturedBottle = prefab.GetRootEntity();
    REQUIRE(capturedBottle);
    REQUIRE(capturedBottle.GetChildCount() == 1);
    CHECK(capturedBottle.GetChild(0).GetComponent<DecalComponent>().Target == capturedBottle.GetUuid());

    Entity instance = scene->CreateEntity("Instance label");
    auto& instanceDecal = instance.AddComponent<DecalComponent>();
    instanceDecal.Size = { 4, 5, 0.2f };
    instanceDecal.UVScale = { -2, 3 };
    PrefabComponent overrides;
    overrides.MarkOverridden("Decal.Size");
    overrides.MarkOverridden("Decal.UVScale");
    PrefabSync::SyncEntity(instance, label, overrides);
    CHECK(instanceDecal.Size == glm::vec3(4, 5, 0.2f));
    CHECK(instanceDecal.UVScale == glm::vec2(-2, 3));
    CHECK(instanceDecal.TargetMode == DecalTargetMode::Entity);
    CHECK(instanceDecal.Target == bottle.GetUuid());
}

TEST_CASE("Prefab receiver settings propagate unless individually overridden", "[decals][Prefab]")
{
    auto scene = CreateRef<Scene>(false);
    Entity source = scene->CreateEntity("Source receiver");
    Entity instance = scene->CreateEntity("Instance receiver");
    source.AddComponent<MeshRendererComponent>().ReceiveDecals = false;
    source.AddComponent<ProceduralMeshComponent>().DecalLayers = 0x12u;
    instance.AddComponent<MeshRendererComponent>();
    instance.AddComponent<ProceduralMeshComponent>();
    PrefabComponent overrides;
    PrefabSync::SyncEntity(instance, source, overrides);
    CHECK_FALSE(instance.GetComponent<MeshRendererComponent>().ReceiveDecals);
    CHECK(instance.GetComponent<ProceduralMeshComponent>().DecalLayers == 0x12u);
    instance.GetComponent<MeshRendererComponent>().ReceiveDecals = true;
    instance.GetComponent<ProceduralMeshComponent>().DecalLayers = 0x40u;
    overrides.MarkOverridden("Mesh Filter.ReceiveDecals");
    overrides.MarkOverridden("Procedural Mesh.DecalLayers");
    PrefabSync::SyncEntity(instance, source, overrides);
    CHECK(instance.GetComponent<MeshRendererComponent>().ReceiveDecals);
    CHECK(instance.GetComponent<ProceduralMeshComponent>().DecalLayers == 0x40u);
}

TEST_CASE("Decal snapshots isolate views and preserve stable overlap and target filtering", "[decals][RenderSnapshot]")
{
    struct ListenerScope
    {
        bool Owned = AssetListenerManager::TryGet() == nullptr;
        ListenerScope()
        {
            if (Owned)
                AssetListenerManager::StartUp();
        }
        ~ListenerScope()
        {
            if (Owned)
                AssetListenerManager::Shutdown();
        }
    } listeners;
    AssetManager assets;
    ShaderDesc description;
    description.Techniques = { ShaderTechnique::Create({ "material_model=decal" }, {}, {}) };
    const auto shader = static_asset_cast<Shader>(assets.CreateAssetHandle(Shader::Create(description)));
    const auto material = static_asset_cast<Material>(assets.CreateAssetHandle(Material::Create(shader)));
    auto scene = CreateRef<Scene>(false);
    Entity receiver = scene->CreateEntity("Receiver");
    receiver.AddComponent<MeshRendererComponent>().DecalLayers = 0x40u;
    Vector<Entity> labels;
    for (int order : { 3, -2, 3 })
    {
        Entity entity = scene->CreateEntity("Label");
        entity.SetPosition({ 0, 0, -2 });
        auto& decal = entity.AddComponent<DecalComponent>();
        decal.Material = material;
        decal.SortOrder = order;
        decal.TargetMode = DecalTargetMode::Entity;
        decal.Target = receiver.GetUuid();
        labels.push_back(entity);
    }
    RenderSnapshot front;
    front.ViewMatrix = glm::mat4(1);
    front.ProjectionMatrix = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    DecalRenderer::Extract(*scene, front);
    REQUIRE(front.Decals.Size() == 3);
    CHECK(front.Decals[0].SortOrder == -2);
    CHECK(front.Decals[1].Id < front.Decals[2].Id);
    REQUIRE(front.DecalReceivers.Size() == 1);
    CHECK(front.DecalReceivers[0].y == 0x40u);
    CHECK(front.Decals[0].Data.Metadata.y == front.DecalReceivers[0].z);
    CHECK(front.Decals[0].Data.Metadata.z == front.DecalReceivers[0].z + 1u);

    RenderSnapshot away;
    away.ViewMatrix = glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    away.ProjectionMatrix = front.ProjectionMatrix;
    DecalRenderer::Extract(*scene, away);
    CHECK(away.Decals.Empty());
    CHECK(front.Decals.Size() == 3);
    labels[0].GetComponent<DecalComponent>().Opacity = 0.25f;
    CHECK(front.Decals[2].Data.Tint.a == 1);
    scene->DestroyEntity(receiver);
    RenderSnapshot deleted;
    deleted.ViewMatrix = front.ViewMatrix;
    deleted.ProjectionMatrix = front.ProjectionMatrix;
    DecalRenderer::Extract(*scene, deleted);
    CHECK(deleted.Decals.Empty());
    CHECK(front.Decals.Size() == 3);
}
